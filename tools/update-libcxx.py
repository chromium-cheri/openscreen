#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script is used to download the libcxx version necessary for clang.
"""

import argparse
import distutils.spawn
import glob
import os
import pipes
import re
import shutil
import subprocess
import stat
import sys
import tarfile
import tempfile
import time
import urllib2
import zipfile

# Do NOT CHANGE this if you don't know what you're doing -- see
# https://chromium.googlesource.com/chromium/src/+/master/docs/updating_clang.md
# Reverting problematic clang rolls is safe, though.
CLANG_REVISION = '351477'

use_head_revision = bool(os.environ.get('LLVM_FORCE_HEAD_REVISION', '0')
                         in ('1', 'YES'))
if use_head_revision:
  CLANG_REVISION = 'HEAD'

# This is incremented when pushing a new build of Clang at the same revision.
CLANG_SUB_REVISION=1

PACKAGE_VERSION = "%s-%s" % (CLANG_REVISION, CLANG_SUB_REVISION)

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
GCLIENT_CONFIG = os.path.join(os.path.dirname(CHROMIUM_DIR), '.gclient')
THIRD_PARTY_DIR = os.path.join(CHROMIUM_DIR, 'third_party')
LLVM_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm')
LLVM_BOOTSTRAP_DIR = os.path.join(THIRD_PARTY_DIR, 'llvm-bootstrap')
LLVM_BOOTSTRAP_INSTALL_DIR = os.path.join(THIRD_PARTY_DIR,
                                          'llvm-bootstrap-install')
CHROME_TOOLS_SHIM_DIR = os.path.join(LLVM_DIR, 'tools', 'chrometools')
LLVM_BUILD_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                              'Release+Asserts')
THREADS_ENABLED_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, 'threads_enabled')
COMPILER_RT_BUILD_DIR = os.path.join(LLVM_BUILD_DIR, 'compiler-rt')
CLANG_DIR = os.path.join(LLVM_DIR, 'tools', 'clang')
LLD_DIR = os.path.join(LLVM_DIR, 'tools', 'lld')
# compiler-rt is built as part of the regular LLVM build on Windows to get
# the 64-bit runtime, and out-of-tree elsewhere.
# TODO(thakis): Try to unify this.
if sys.platform == 'win32':
  COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'projects', 'compiler-rt')
else:
  COMPILER_RT_DIR = os.path.join(LLVM_DIR, 'compiler-rt')
LIBCXX_DIR = os.path.join(LLVM_DIR, 'projects', 'libcxx')
LIBCXXABI_DIR = os.path.join(LLVM_DIR, 'projects', 'libcxxabi')
LLVM_BUILD_TOOLS_DIR = os.path.abspath(
    os.path.join(LLVM_DIR, '..', 'llvm-build-tools'))
STAMP_FILE = os.path.normpath(
    os.path.join(LLVM_DIR, '..', 'llvm-build', 'cr_build_revision'))
VERSION = '9.0.0'
ANDROID_NDK_DIR = os.path.join(
    CHROMIUM_DIR, 'third_party', 'android_ndk')
FUCHSIA_SDK_DIR = os.path.join(CHROMIUM_DIR, 'third_party', 'fuchsia-sdk',
                               'sdk')

# URL for pre-built binaries.
CDS_URL = os.environ.get('CDS_CLANG_BUCKET_OVERRIDE',
    'https://commondatastorage.googleapis.com/chromium-browser-clang')

LLVM_REPO_URL='https://llvm.org/svn/llvm-project'
if 'LLVM_REPO_URL' in os.environ:
  LLVM_REPO_URL = os.environ['LLVM_REPO_URL']

def CheckoutRepos(args):
  # (i.e. this is needed for bootstrap builds).
  Checkout('libcxx', LLVM_REPO_URL + '/libcxx/trunk', LIBCXX_DIR)

def UpdateClang(args):
  CheckoutRepos(args)

def main():
  parser = argparse.ArgumentParser(description='Build Clang.')
  args = parser.parse_args()

  return UpdateClang(args)