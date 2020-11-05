#!/usr/bin/env bash

# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if ! command -v "ajv" &> /dev/null
then
    echo "Could not find ajv, please install before running. See https://www.npmjs.com/package/ajv-cli."
    exit
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
for filename in $SCRIPT_DIR/streaming_examples/*.json; do
ajv validate -s "$SCRIPT_DIR/streaming_schema.json" -d "$filename"
done

for filename in $SCRIPT_DIR/app_examples/*.json; do
ajv validate -s "$SCRIPT_DIR/app_schema.json" -d "$filename"
done
