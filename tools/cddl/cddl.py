# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys

def main():
    exit(subprocess.call(['./cddl'] + sys.argv[1:]))

main()
