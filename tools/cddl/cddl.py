# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

def main():
    if len(sys.argv) != 5:
        print("Error: 5 input variables expected and instead recieved " + len(sys.argv))
	sys.exit(1)

    header = sys.argv[1]
    cc = sys.argv[2]
    genDir = sys.argv[3]
    cddlFile = sys.argv[4]

    if not validatePathInput(genDir):
        print("Error: '" + genDir + "' is not a valid output directory")
	sys.exit(1)
    elif not validateHeaderInput(genDir, header):
        print("Error: '" + header + "' is not a valid .h file")
	sys.exit(1)
    elif not validateCodeInput(genDir, cc):
        print("Error: '" + cc + "' is not a valid .cc file")
	sys.exit(1)
    elif not validateCddlInput(cddlFile):
        print("Error: '" + cddlFile + "' is not a valid CDDL file")
	sys.exit(1)

    subprocess.Popen(['./cddl', "--header", header, "--cc", cc, "--gen-dir", genDir, cddlFile]).wait()
    subprocess.Popen(['clang-format', "-i", genDir + "/" + header]).wait()
    subprocess.Popen(['clang-format', "-i", genDir + "/" + cc]).wait()

def validateHeaderInput(genDir, headerFile):
    filePath = genDir + '/' + headerFile
    return headerFile.endswith('.h') and os.path.isfile(filePath)

def validateCodeInput(genDir, ccFile):
    filePath = genDir + '/' + ccFile
    return ccFile.endswith('.cc') and os.path.isfile(filePath)

def validatePathInput(dirPath):
    return os.path.isdir(dirPath)

def validateCddlInput(cddlFile):
    return os.path.isfile(cddlFile)

main()
