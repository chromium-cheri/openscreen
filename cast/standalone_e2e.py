#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script is intended to be a simple, "golden case" end to end test for
the standalone sender and receiver executables in cast. This ensures that
the basic functionality of these executables is not impaired, such as the
TLS/UDP connections and encoding and decoding video.
"""

import argparse
import pathlib
import os
import logging
import subprocess
import sys
import time
import urllib.request

TEST_VIDEO_NAME = 'Contador_Glam.mp4'
# NOTE: we use the HTTP protocol instead of HTTPS due to certificate issues
# in the legacy urllib.request API.
TEST_VIDEO_URL = ('http://storage.googleapis.com/openscreen_standalone/' +
                  TEST_VIDEO_NAME)

OPENSCREEN_ROOT_FOLDER = pathlib.Path(__file__).resolve().parent.parent
OUT_DIR = OPENSCREEN_ROOT_FOLDER.joinpath('out/Default').resolve()

TEST_VIDEO_PATH = OUT_DIR.joinpath(TEST_VIDEO_NAME).resolve()
CAST_RECEIVER_BINARY = (OUT_DIR.joinpath('cast_receiver')).resolve()
CAST_SENDER_BINARY = (OUT_DIR.joinpath('cast_sender')).resolve()
PROCESS_TIMEOUT = 15  # seconds

EXPECTED_RECEIVER_MESSAGES = [
    "CastService is running.", "Selected opus as codec for streaming",
    "Selected vp8 as codec for streaming",
    "Found codec: opus (known to FFMPEG as opus)",
    "Found codec: vp8 (known to FFMPEG as vp8)"
]
EXPECTED_SENDER_MESSAGES = [
    "Launching Mirroring App on the Cast Receiver",
    "Contador_Glam.mp4 (starts in one second)...",
    "The video capturer has reached the end of the media stream.",
    "The audio capturer has reached the end of the media stream.",
    "Video complete. Exiting...", "Shutting down..."
]

def _set_log_level(verbose=False):
    """Sets the logging level, either DEBUG or ERROR as appropriate."""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(stream=sys.stdout, level=level)


def _get_loopback_adapter_name():
    """Retrieves the name of the loopback adapter (lo on Linux/lo0 on Mac)."""
    if sys.platform == 'linux' or sys.platform == 'linux2':
        return 'lo'
    if sys.platform == 'darwin':
        return 'lo0'
    return None


def _download_video():
    """Downloads the test video from Google storage."""
    if os.path.exists(TEST_VIDEO_PATH):
        logging.debug('Video already exists, skipping download...')
        return

    logging.debug('Downloading video from %s', TEST_VIDEO_URL)
    urllib.request.urlretrieve(TEST_VIDEO_URL, TEST_VIDEO_PATH)


def _generate_certificates():
    """Generates test certificates using the cast receiver."""
    logging.debug('Generating certificates...')
    return subprocess.check_output(
        [
            CAST_RECEIVER_BINARY,
            '-g',  # Generate certificate and private key.
            '-v'  # Enable verbose logging.
        ],
        stderr=subprocess.STDOUT)


def _launch_receiver():
    """Launches the receiver process with discovery disabled."""
    logging.debug('Launching the receiver application...')
    loopback = _get_loopback_adapter_name()
    if not loopback:
        return None

    return subprocess.Popen(
        [
            CAST_RECEIVER_BINARY,
            '-d',
            './generated_root_cast_receiver.crt',
            '-p',
            './generated_root_cast_receiver.key',
            '-x',  # Skip discovery, only necessary on Mac OS X.
            '-v',  # Enable verbose logging.
            loopback
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)


def _launch_sender():
    """Launches the sender process, running the test video file once."""
    logging.debug('Launching the sender application...')
    return subprocess.Popen(
        [
            CAST_SENDER_BINARY,
            '127.0.0.1:8010',
            TEST_VIDEO_PATH,
            '-d',
            './generated_root_cast_receiver.crt',
            '-n'  # Only play the video once, and then exit.
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)


def _check_logs(logs):
    """Checks that the outputted logs contain expected behavior."""
    for message in EXPECTED_RECEIVER_MESSAGES:
        assert message in logs[0], "Missing log message: " + message
    for message in EXPECTED_SENDER_MESSAGES:
        assert message in logs[1], "Missing log message: " + message
    for log, prefix in logs, ["[ERROR:", "[FATAL:"]:
        assert prefix not in log, "Logs contained an error"
    logging.info('Finished validating log output')


def _parse_args(args):
    """Parses the command line arguments and sets up the logging module."""
    parser = argparse.ArgumentParser(description='Run end to end tests on'
                                     'the standalone cast implementations.')
    parser.add_argument('-v',
                        '--verbose',
                        help='enable debug logging',
                        action='store_true')
    parsed_args = parser.parse_args(args)
    _set_log_level(parsed_args.verbose)


def test():
    """
    Exported function for standalone testing. This method manages downloading
    video, generating certificates, running the standalone receiver and then
    the sender, and finally checking the output of the logs for errors and
    expected values.
    """
    _download_video()
    _generate_certificates()
    receiver_process = _launch_receiver()
    logging.debug('Waiting a bit to give the receiver a chance to start up...')
    time.sleep(3)
    sender_process = _launch_sender()

    logging.debug('collating output...')
    output = (receiver_process.communicate(
        timeout=PROCESS_TIMEOUT)[1].decode('utf-8'),
              sender_process.communicate(
                  timeout=PROCESS_TIMEOUT)[1].decode('utf-8'))

    _check_logs(output)


def main(args):
    """
    Main entry point. Sets up the current working directory, parses logs and
    sets up the logger, and then executes the standalone |test()| function.
    """
    os.chdir(OPENSCREEN_ROOT_FOLDER)
    _parse_args(args)
    test()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
