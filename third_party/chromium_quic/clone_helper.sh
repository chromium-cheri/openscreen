#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script does two checks on the state of the copied chromium directories:
#  1. Ensure all files present are in their appropriate BUILD.gn.
#  2. Ensure all .h files with a corresponding .cc file in chromium have their
#     .cc copied here.
#
# This should be run with your working directory at the root of the chromium
# clone.
# Usage: clone_helper.sh <ninja build dir> <chromium src directory>
#                        [run existence check]

# Both of the loops below will insert any files they want to add to a BUILD.gn
# at the end of the first 'sources =' block in the file.  It will insert it with
# only two spaces instead of four, so you can find files inserted by the script
# and adjust them as necessary.
function add_source_file() {
  sed ":a;/sources = \[/bb;n;ba; :b;/^  \]/bc;n;bb; :c;s/^  \]/  \"$1\",\n  \]/; :d;n;bd" -i "$2"
}

# The following check can be skipped by not passing any arguments to the script,
# since this operation is slow and can be considered more of a "clean up" than
# really iterative.
if [ $# -eq 3 ]; then
  # Ensure all .h,.cc, and .c files under the directory xyz are in xyz/BUILD.gn.
  # This helps ensure we don't have extra files laying around in directories
  # that aren't tracked in the build files.  It doesn't check that they are
  # actually built or used though, so if a file needs to be in the directory
  # structure (e.g. to bulk copy a subdirectory but only build some of it) but
  # not be built, it can be put in a comment.
  for f in $(find base crypto net testing url -name '*.h' \
               -o -name '*.cc' -o -name '*.c'); do
    d=$(echo $f | cut -f1 -d/)
    rel_file=$(echo $f | cut -f2- -d/)
    if ! grep -q "$rel_file" $d/BUILD.gn; then
      add_source_file "$rel_file" $d/BUILD.gn
      echo $d/BUILD.gn
    fi
  done
fi

ninja_dir=$1
chromium_dir=$2

# This loops tries to catch the simplest class of build errors: missing includes
# relative to the src directory and .cc files named identically to .h files.
# This will not catch things like:
#  - Third-party dependencies which include things relative to their own source
#    directory.
#  - Platform-specific implementation files (e.g. xyz.h and xyz_posix.cc).
#  - Generated files (e.g. out/Default/gen/...)
while :; do
  # Ensure all .h files with a matching .cc in chromium are copied here and
  # placed in xyz/BUILD.gn.  This helps eliminate some obvious linking errors.
  for f in $(find base crypto net testing url -name '*.h'); do
    cc_file=$(echo $f | cut -f1 -d.).cc
    [ ! -e $cc_file ] || continue
    [ -e $chromium_dir/$cc_file ] || continue
    mkdir -p $(dirname $cc_file)
    d=$(echo $f | cut -f1 -d/)
    cp $chromium_dir/$cc_file $(dirname $cc_file)
    if [ -e $cc_file ]; then
      echo $cc_file
      rel_file=$(echo $cc_file | cut -f2- -d/)
      add_source_file "$rel_file" $d/BUILD.gn
    fi
  done

  # Try to build what we have so far and fix any obvious include errors.  If we
  # were able to add an include to fix the error, we loop again to potentially
  # add a new .cc file and try again.
  output=$(ninja -C $ninja_dir)
  if echo "$output" | grep -q "#include \""; then
    f=$(echo "$output" | grep -m1 "#include" |\
        sed 's/^.*#include "\([^"]\+\.h\)".*$/\1/')
    mkdir -p $(dirname $f)
    echo $f
    if cp $chromium_dir/$f $(dirname $f); then
      b=$(echo $f | cut -f1 -d/)/BUILD.gn
      add_source_file "$(echo $f | cut -f2- -d/)" $b
    else
      echo -e "$output"
      break
    fi
  else
    echo -e "$output"
    break
  fi
done
