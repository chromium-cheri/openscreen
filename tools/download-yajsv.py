#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script is used to download the clang update script. It runs as a gclient
hook.

It's equivalent to using curl to download the latest update script:

  $ curl --silent --create-dirs -o tools/clang/scripts/update.py \
      https://raw.githubusercontent.com/chromium/chromium/master/tools/clang/scripts/update.py
"""

import argparse
import curlish
import os
import stat
import sys

RELEASES_DOWNLOAD_URL = 'https://github.com/neilpa/yajsv/releases/download/'
VERSION = 'v1.4.0'
YAJSV_FLAVOR_DICT = {
    'linux32': 'yajsv.linux.386',
    'linux64': 'yajsv.linux.amd64',
    'mac64': 'yajsv.darwin.amd64'
}


def main():

    parser = argparse.ArgumentParser(description='Download a YAJSV release.')
    parser.add_argument('flavor',
                        help='Flavor to download (currently one of {})'.format(
                            ', '.join(YAJSV_FLAVOR_DICT.keys())))
    args = parser.parse_args()

    if not args.flavor:
        print('flavor not provided, defaulting to linux64')
        args.flavor = 'linux64'

    output_path = os.path.abspath(
        os.path.join(os.path.dirname(os.path.relpath(__file__)), 'yajsv'))
    download_url = (RELEASES_DOWNLOAD_URL + VERSION +
                    '/' + YAJSV_FLAVOR_DICT[args.flavor])
    result = curlish.curlish(download_url, output_path)

    # YAJSV isn't useful if it's not executable.
    if result:
        current_mode = os.stat(output_path).st_mode
        os.chmod(output_path, current_mode | stat.S_IEXEC)

    return 0 if result else 1


if __name__ == '__main__':
    sys.exit(main())
