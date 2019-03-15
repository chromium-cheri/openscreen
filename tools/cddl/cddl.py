# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

def main():
  header = ''
  cc = ''
  genDir = ''
  cddlFile = ''

  # Parse all setting values from the input parameters.
  settingCount = 0
  i = 1
  while i < len(sys.argv):
    settingCount += 1;
    arg = sys.argv[i]
    if arg == '--header':
      i += 1
      header = sys.argv[i]
    elif arg == '--cc':
      i += 1
      cc = sys.argv[i]
    elif arg == '--gen-dir':
      i += 1
      genDir = sys.argv[i]
    else:
      cddlFile = sys.argv[i]
    i += 1

  # Validate recieved parameters.
  if settingCount != 4:
    print("Error: 5 input parameters expected and instead received " +
          str(settingCount))
    sys.exit(1)

  if not validateHeaderInput(header):
    print("Error: '" + header + "' is not a valid .h file")
    sys.exit(1)
  elif not validateCodeInput(cc):
    print("Error: '" + cc + "' is not a valid .cc file")
    sys.exit(1)
  elif not validatePathInput(genDir):
    print("Error: '" + genDir + "' is not a valid output directory")
    sys.exit(1)
  elif not validateCddlInput(cddlFile):
    print("Error: '" + cddlFile + "' is not a valid CDDL file")
    sys.exit(1)

  # Execute command line commands with these variables.
  print('Creating C++ files from provided CDDL file...')
  print('\tExecuting command: ./cddl --header ' + header + ' --cc ' +
         cc + ' --gen-dir ' + genDir + ' cddlFile')
  subprocess.Popen(['./cddl', "--header", header, "--cc", cc, "--gen-dir",
                    genDir, cddlFile]).wait()
  print('\tExecuting command: clang-format -i ' + genDir + '/' + header)
  subprocess.Popen(['clang-format', "-i", genDir + "/" + header]).wait()
  print('\tExecuting command: clang-format -i ' + genDir + '/' + cc)
  subprocess.Popen(['clang-format', "-i", genDir + "/" + cc]).wait()

def validateHeaderInput(headerFile):
  return headerFile != '' and headerFile.endswith('.h')

def validateCodeInput(ccFile):
  return ccFile != '' and ccFile.endswith('.cc')

def validatePathInput(dirPath):
  return dirPath != '' and os.path.isdir(dirPath)

def validateCddlInput(cddlFile):
  return cddlFile != '' and os.path.isfile(cddlFile)

main()
