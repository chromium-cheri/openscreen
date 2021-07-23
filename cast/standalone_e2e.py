#!/usr/bin/env python3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script is intended to cover end to end testing for the standalone sender
and receiver executables in cast. This ensures that the basic functionality of
these executables is not impaired, such as the TLS/UDP connections and encoding
and decoding video.
"""

import argparse
import os
import pathlib
import logging
import subprocess
import sys
import time
import urllib.request
import unittest

from enum import IntFlag

# Environment variables that can be overridden to set test properties.
ROOT_ENVVAR = 'OPENSCREEN_ROOT_DIR'
BUILD_ENVVAR = 'OPENSCREEN_BUILD_DIR'

TEST_VIDEO_NAME = 'Contador_Glam.mp4'
# NOTE: we use the HTTP protocol instead of HTTPS due to certificate issues
# in the legacy urllib.request API.
TEST_VIDEO_URL = ('http://storage.googleapis.com/openscreen_standalone/' +
                  TEST_VIDEO_NAME)

PROCESS_TIMEOUT = 15  # seconds

# Open screen test certificates expire after 3 days. We crop this slightly (by
# 8 hours) to account for potential errors in time calculations.
CERT_EXPIRY_AGE = (3 * 24 - 8) * 60 * 60

# These properties are based on compiled settings in Open Screen, and should
# not change without updating this file.
TEST_CERT_NAME = 'generated_root_cast_receiver.crt'
TEST_KEY_NAME = 'generated_root_cast_receiver.key'
SENDER_BINARY_NAME = 'cast_sender'
RECEIVER_BINARY_NAME = 'cast_receiver'

EXPECTED_RECEIVER_MESSAGES = [
    "CastService is running.", "Found codec: opus (known to FFMPEG as opus)",
    "Found codec: vp8 (known to FFMPEG as vp8)",
    "Successfully negotiated a session, creating SDL players.",
    "Receivers are currently destroying, resetting SDL players."
]

EXPECTED_SENDER_MESSAGES = [
    "Launching Mirroring App on the Cast Receiver",
    "Max allowed media bitrate (audio + video) will be",
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


def _get_file_age_in_seconds(path):
    """Get the age of a given file in seconds"""
    # Time is stored in seconds since epoch
    file_last_modified = 0
    if path.exists():
        file_last_modified = path.stat().st_mtime
    return time.time() - file_last_modified


class TestFlags(IntFlag):
    """
    Test flags, primarily used to control sender and receiver configuration
    to test different features of the standalone libraries.
    """
    UseRemoting = 1
    UseAndroidHack = 2


class TestConfiguration():
    """Constructs paths and handles various test property setup."""
    def __init__(self):
        self.root_dir = pathlib.Path(
            os.environ[ROOT_ENVVAR] if os.getenv(ROOT_ENVVAR) else subprocess.
            getoutput('git rev-parse --show-toplevel'))
        assert self.root_dir.exists(), 'Could not find openscreen root!'
        self.build_dir = pathlib.Path(os.environ[BUILD_ENVVAR]) if os.getenv(
            BUILD_ENVVAR) else self.root_dir.joinpath('out',
                                                      'Default').resolve()
        assert self.build_dir.exists(), 'Could not find openscreen build!'

        self.parse_args(sys.argv[1:])
        # NOTE: logging is not set up until |parse_args| has been called.
        logging.debug(
            'Using root directory: "{}", build directory: "{}"'.format(
                self.root_dir, self.build_dir))

        self.test_video_path = self.build_dir.joinpath(
            TEST_VIDEO_NAME).resolve()
        self.cast_receiver_binary = (
            self.build_dir.joinpath(RECEIVER_BINARY_NAME)).resolve()
        self.cast_sender_binary = (
            self.build_dir.joinpath(SENDER_BINARY_NAME)).resolve()

    def parse_args(self, args):
        """Parses the command line arguments and sets up the logging module."""
        parser = argparse.ArgumentParser(
            description='Run end to end tests on'
            'the standalone cast implementations.')
        parser.add_argument('-v',
                            '--verbose',
                            help='enable debug logging',
                            action='store_true')
        parsed_args = parser.parse_args(args)
        _set_log_level(parsed_args.verbose)
        self.is_verbose = parsed_args.verbose


class StandaloneCastTest(unittest.TestCase):
    """
    Test class for setting up and running end to end tests on the
    standalone sender and receiver binaries. This class uses the unittest
    package, so methods that are executed as tests all have named prefixed
    with "test_".
    """
    @classmethod
    def setUpClass(cls):
        """Shared setup method for all tests, handles one-time updates."""
        cls.config = TestConfiguration()
        os.chdir(cls.config.root_dir)
        cls.download_video()
        cls.generate_certificates()

    @classmethod
    def download_video(cls):
        """Downloads the test video from Google storage."""
        if os.path.exists(cls.config.test_video_path):
            logging.debug('Video already exists, skipping download...')
            return

        logging.debug('Downloading video from %s', TEST_VIDEO_URL)
        urllib.request.urlretrieve(TEST_VIDEO_URL, cls.config.test_video_path)

    @classmethod
    def generate_certificates(cls):
        """Generates test certificates using the cast receiver."""
        cert_age = _get_file_age_in_seconds(pathlib.Path(TEST_CERT_NAME))
        key_age = _get_file_age_in_seconds(pathlib.Path(TEST_KEY_NAME))
        if cert_age < CERT_EXPIRY_AGE and key_age < CERT_EXPIRY_AGE:
            logging.debug('Credentials are up to date...')
            return

        logging.debug('Credentials out of date, generating new ones...')
        return subprocess.check_output(
            [
                cls.config.cast_receiver_binary,
                '-g',  # Generate certificate and private key.
                '-v'  # Enable verbose logging.
            ],
            stderr=subprocess.STDOUT)

    def launch_receiver(self):
        """Launches the receiver process with discovery disabled."""
        logging.debug('Launching the receiver application...')
        loopback = _get_loopback_adapter_name()
        self.assertTrue(loopback)
        return subprocess.Popen(
            [
                self.config.cast_receiver_binary,
                '-d',
                './' + TEST_CERT_NAME,
                '-p',
                './' + TEST_KEY_NAME,
                '-x',  # Skip discovery, only necessary on Mac OS X.
                '-v',  # Enable verbose logging.
                loopback
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

    def launch_sender(self, flags):
        """Launches the sender process, running the test video file once."""
        logging.debug('Launching the sender application...')
        command = [
            self.config.cast_sender_binary,
            '127.0.0.1:8010',
            self.config.test_video_path,
            '-d',
            './' + TEST_CERT_NAME,
            '-n'  # Only play the video once, and then exit.
        ]
        if TestFlags.UseAndroidHack in flags:
            command.append('-a')
        if TestFlags.UseRemoting in flags:
            command.append('-r')

        return subprocess.Popen(command,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)

    def check_logs(self, logs):
        """Checks that the outputted logs contain expected behavior."""
        for message in EXPECTED_RECEIVER_MESSAGES:
            self.assertTrue(
                message in logs[0],
                "Missing log message: " + message + ' of ' + str(logs[0]))
        for message in EXPECTED_SENDER_MESSAGES:
            self.assertTrue(message in logs[1],
                            "Missing log message: " + message)
        for log, prefix in logs, ["[ERROR:", "[FATAL:"]:
            self.assertTrue(prefix not in log, "Logs contained an error")
        logging.debug('Finished validating log output')

    def get_output(self, flags):
        """Launches the sender and receiver, and handles exit output."""
        receiver_process = self.launch_receiver()
        logging.debug('Letting the receiver start up...')
        time.sleep(3)
        sender_process = self.launch_sender(flags)

        logging.debug('Launched sender PID {} and receiver PID {}...'.format(
            sender_process.pid, receiver_process.pid))
        logging.debug('collating output...')
        output = (receiver_process.communicate(
            timeout=PROCESS_TIMEOUT)[1].decode('utf-8'),
                  sender_process.communicate(
                      timeout=PROCESS_TIMEOUT)[1].decode('utf-8'))

        # TODO(issuetracker.google.com/194292855): standalones should exit zero.
        # Remoting causes the sender to exit with code -4.
        if not TestFlags.UseRemoting in flags:
            self.assertEqual(sender_process.returncode, 0,
                             'sender had non-zero exit code')
        return output

    def test_golden_case(self):
        """Tests that when settings are normal, things work end to end."""
        output = self.get_output([])
        self.check_logs(output)

    def test_remoting(self):
        """Tests that basic remoting works."""
        output = self.get_output(TestFlags.UseRemoting)
        self.check_logs(output)

    def test_with_android_hack(self):
        """Tests that things work when the Android RTP hack is enabled."""
        output = self.get_output(TestFlags.UseAndroidHack)
        self.check_logs(output)


if __name__ == '__main__':
    unittest.main()
