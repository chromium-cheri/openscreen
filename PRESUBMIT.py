# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import

import os
import re
import sys


_REPO_PATH = os.path.dirname(os.path.realpath('__file__'))

_IMPORT_SUBFOLDERS = ['tools', os.path.join('buildtools', 'checkdeps')]

# git-cl upload is not compatible with __init__.py based subfolder imports, so
# we instead extended the system path
sys.path.extend(os.path.join(_REPO_PATH, p) for p in _IMPORT_SUBFOLDERS)

import licenses
from checkdeps import DepsChecker
from cpp_checker import CppChecker
from rules import Rule

# Rather than pass this to all of the checks, we override the global excluded
# list with this one.
_EXCLUDED_PATHS = (
  # Exclude all of third_party/ except for BUILD.gns that we maintain.
  r'third_party[\\\/].*(?<!BUILD.gn)$',
  # Exclude everything under third_party/chromium_quic/{src|build}
  r'third_party/chromium_quic/(src|build)/.*',
  # Output directories (just in case)
  r'.*\bDebug[\\\/].*',
  r'.*\bRelease[\\\/].*',
  r'.*\bxcodebuild[\\\/].*',
  r'.*\bout[\\\/].*',
  # There is no point in processing a patch file.
  r'.+\.diff$',
  r'.+\.patch$',
)


def _CheckLicenses(input_api, output_api):
    """Checks third party licenses"""
    return [
        output_api.PresubmitError(violation)
        for violation in licenses.ScanThirdPartyDirs()
    ]


def _CheckDeps(input_api, output_api):
    """Checks DEPS rules"""
    results = []
    deps_checker = DepsChecker(input_api.PresubmitLocalPath())
    deps_checker.CheckDirectory(input_api.PresubmitLocalPath())
    deps_results = deps_checker.results_formatter.GetResults()
    for violation in deps_results:
        results.append(output_api.PresubmitError(violation))
    return results


# Matches Foo(Foo&&) when not followed by noexcept.
_RE_PATTERN_MOVE_WITHOUT_NOEXCEPT = re.compile(
    r'\s*(?P<classname>\w+)\((?P=classname)&&[^)]*\)\s*(?!noexcept)\s*[{;=]')


def _CheckNoexceptOnMove(filename, clean_lines, linenum, error):
  """Checks that move constructors are declared with 'noexcept'.

  Args:
    filename: The name of the current file.
    clean_lines: A CleansedLines instance containing the file.
    linenum: The number of the line to check.
    error: The function to call with any errors found.
  """
  # We only check headers as noexcept is meaningful on declarations, not
  # definitions.  This may skip some definitions in .cc files though.
  if not filename.endswith('.h'):
    return

  line = clean_lines.elided[linenum]
  matched = _RE_PATTERN_MOVE_WITHOUT_NOEXCEPT.match(line)
  if matched:
    error(filename, linenum, 'runtime/noexcept', 4,
          'Move constructor of %s not declared \'noexcept\' in %s' %
          (matched.group('classname'), matched.group(0).strip()))

# - We disable c++11 header checks since Open Screen allows them.
# - We disable whitespace/braces because of various false positives.
# - There are some false positives with 'explicit' checks, but it's useful
#   enough to keep.
# - We add a custom check for 'noexcept' usage.
def _CheckChangeLintsClean(input_api, output_api):
  """Checks that all '.cc' and '.h' files pass cpplint.py."""
  result = []

  cpplint = input_api.cpplint
  # Access to a protected member _XX of a client class
  # pylint: disable=protected-access
  cpplint._cpplint_state.ResetErrorCounts()

  cpplint._SetFilters('-build/c++11,-whitespace/braces')
  files = [f.AbsoluteLocalPath() for f in input_api.AffectedSourceFiles(None)]
  for file_name in files:
    # 4 = verbose_level
    cpplint.ProcessFile(file_name, 4, [_CheckNoexceptOnMove])

  if cpplint._cpplint_state.error_count > 0:
    if input_api.is_committing:
      res_type = output_api.PresubmitError
    else:
      res_type = output_api.PresubmitPromptWarning
    result = [res_type('Changelist failed cpplint.py check.')]

  return result


def _CommonChecks(input_api, output_api):
  results = []
  # PanProjectChecks include:
  #   CheckLongLines (@ 80 cols)
  #   CheckChangeHasNoTabs
  #   CheckChangeHasNoStrayWhitespace
  #   CheckLicense
  #   CheckChangeWasUploaded (if committing)
  #   CheckChangeHasDescription
  #   CheckDoNotSubmitInDescription
  #   CheckDoNotSubmitInFiles
  results.extend(input_api.canned_checks.PanProjectChecks(
    input_api, output_api, owners_check=False));

  # No carriage return characters, files end with one EOL (\n).
  results.extend(input_api.canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol(
    input_api, output_api));

  # Gender inclusivity
  results.extend(input_api.canned_checks.CheckGenderNeutral(
    input_api, output_api))

  # TODO(bug) format required
  results.extend(input_api.canned_checks.CheckChangeTodoHasOwner(
    input_api, output_api))

  # Linter.
  results.extend(_CheckChangeLintsClean(input_api, output_api))

  # clang-format
  results.extend(input_api.canned_checks.CheckPatchFormatted(
    input_api, output_api, bypass_warnings=False))

  # GN formatting
  results.extend(input_api.canned_checks.CheckGNFormatted(
    input_api, output_api))

  # buildtools/checkdeps
  results.extend(_CheckDeps(input_api, output_api))

  # tools/licenses
  results.extend(_CheckLicenses(input_api, output_api))

  return results


def CheckChangeOnUpload(input_api, output_api):
    input_api.DEFAULT_FILES_TO_SKIP = _EXCLUDED_PATHS
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    results.extend(
        input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    input_api.DEFAULT_FILES_TO_SKIP = _EXCLUDED_PATHS
    results = []
    results.extend(_CommonChecks(input_api, output_api))
    return results
