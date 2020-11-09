#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Converts a data file, e.g. a JSON file, into a C++ raw string that
can be #included.
"""

import argparse
import os
import sys

FORMAT_STRING = 'R"({})";'


def convert(input_path, output_path):
    if not os.path.exists(input_path):
        print('Invalid path supplied: {}'.format(input_path))
        return 1

    content = False
    with open(input_path, 'r') as f:
        content = f.read()

    with open(output_path, 'w') as f:
        print("Writing to output file: {}".format(output_path))
        f.write(FORMAT_STRING.format(content))


def main():
    parser = argparse.ArgumentParser(
        description='Convert a file to a C++ data file')
    parser.add_argument('input_path', help='Path to file to convert')
    parser.add_argument('output_path', help='Output path of converted file')
    args = parser.parse_args()

    input_path = os.path.abspath(args.input_path)
    output_path = os.path.abspath(args.output_path)
    convert(input_path, output_path)


if __name__ == '__main__':
    sys.exit(main())
