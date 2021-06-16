# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import subprocess
import shutil
import tempfile

_REPO_PATH = os.path.dirname(os.path.realpath('__file__'))
_IMPORT_SUBFOLDERS = ['tools', os.path.join('buildtools', 'checkdeps')]

# GN_ERROR_RE matches the summary of an error output by `gn check`.
# Matches "ERROR" and following lines until it sees an empty line or a line
# containing just underscores.
GN_ERROR_RE = re.compile(
    r'ERROR at ([^:]+:\d+:\d+): ([^\n]*)([\s\S]+?)((?=\n_+\n)|(?=$))')

# git-cl upload is not compatible with __init__.py based subfolder imports, so
# we extend the system path instead.
sys.path.extend(os.path.join(_REPO_PATH, p) for p in _IMPORT_SUBFOLDERS)

import licenses
from checkdeps import DepsChecker

# Opt-in to using Python3 instead of Python2, as part of the ongoing Python2
# deprecation. For more information, see
# https://issuetracker.google.com/173766869.
USE_PYTHON3 = True

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


def _IsRealDepotTools(path):
  return os.path.isfile(os.path.join(path, 'gclient.py'))


def _GetDepotToolsPath():
  """Search for depot_tools and add it to sys.path."""
  # First look if depot_tools is already in PYTHONPATH.
  for i in sys.path:
    if i.rstrip(os.sep).endswith('depot_tools') and _IsRealDepotTools(i):
      return i
  # Then look if depot_tools is in PATH, common case.
  for i in os.environ['PATH'].split(os.pathsep):
    if _IsRealDepotTools(i):
      return i
  # Rare case, it's not even in PATH, look upward up to root.
  root_dir = os.path.dirname(os.path.abspath(__file__))
  previous_dir = os.path.abspath(__file__)
  while root_dir and root_dir != previous_dir:
    i = os.path.join(root_dir, 'depot_tools')
    if _IsRealDepotTools(i):
      return i
    previous_dir = root_dir
    root_dir = os.path.dirname(root_dir)
  print >> sys.stderr, 'Failed to find depot_tools'
  return None

DEPOT_TOOLS_PATH = _GetDepotToolsPath()

def _CheckLicenses(input_api, output_api):
    """Checks third party licenses and returns a list of violations."""
    return [
        output_api.PresubmitError(v) for v in licenses.ScanThirdPartyDirs()
    ]


def _CheckDeps(input_api, output_api):
    """Checks DEPS rules and returns a list of violations."""
    deps_checker = DepsChecker(input_api.PresubmitLocalPath())
    deps_checker.CheckDirectory(input_api.PresubmitLocalPath())
    deps_results = deps_checker.results_formatter.GetResults()
    return [output_api.PresubmitError(v) for v in deps_results]


# Matches OSP_CHECK(foo.is_value()) or OSP_DCHECK(foo.is_value())
_RE_PATTERN_VALUE_CHECK = re.compile(
    r'\s*OSP_D?CHECK\([^)]*\.is_value\(\)\);\s*')

# Matches Foo(Foo&&) when not followed by noexcept.
_RE_PATTERN_MOVE_WITHOUT_NOEXCEPT = re.compile(
    r'\s*(?P<classname>\w+)\((?P=classname)&&[^)]*\)\s*(?!noexcept)\s*[{;=]')


def _CheckNoRegexMatches(regex,
                         filename,
                         clean_lines,
                         linenum,
                         error,
                         error_type,
                         error_msg,
                         include_cpp_files=True):
    """Checks that there are no matches for a specific regex.

  Args:
    regex: regex to use for matching.
    filename: The name of the current file.
    clean_lines: A CleansedLines instance containing the file.
    linenum: The number of the line to check.
    error: The function to call with any errors found.
    error_type: type of error, e.g. runtime/noexcept
    error_msg: Specific message to prepend when regex match is found.
  """
    if not include_cpp_files and not filename.endswith('.h'):
        return

    line = clean_lines.elided[linenum]
    matched = regex.match(line)
    if matched:
        error(filename, linenum, error_type, 4,
              'Error: {} at {}'.format(error_msg,
                                       matched.group(0).strip()))


def _CheckNoValueDchecks(filename, clean_lines, linenum, error):
    """Checks that there are no OSP_DCHECK(foo.is_value()) instances.

    filename: The name of the current file.
    clean_lines: A CleansedLines instance containing the file.
    linenum: The number of the line to check.
    error: The function to call with any errors found.
    """
    _CheckNoRegexMatches(_RE_PATTERN_VALUE_CHECK, filename, clean_lines,
                         linenum, error, 'runtime/is_value_dchecks',
                         'Unnecessary CHECK for ErrorOr::is_value()')


def _CheckNoexceptOnMove(filename, clean_lines, linenum, error):
    """Checks that move constructors are declared with 'noexcept'.

    filename: The name of the current file.
    clean_lines: A CleansedLines instance containing the file.
    linenum: The number of the line to check.
    error: The function to call with any errors found.
  """
    # We only check headers as noexcept is meaningful on declarations, not
    # definitions.  This may skip some definitions in .cc files though.
    _CheckNoRegexMatches(_RE_PATTERN_MOVE_WITHOUT_NOEXCEPT, filename,
                         clean_lines, linenum, error, 'runtime/noexcept',
                         'Move constructor not declared \'noexcept\'', False)

# - We disable c++11 header checks since Open Screen allows them.
# - We disable whitespace/braces because of various false positives.
# - There are some false positives with 'explicit' checks, but it's useful
#   enough to keep.
# - We have additional custom checks for 'noexcept' and DCHECK usage.
def _CheckChangeLintsClean(input_api, output_api):
    """Checks that all '.cc' and '.h' files pass cpplint.py."""
    cpplint = input_api.cpplint
    # Directive that allows access to a protected member _XX of a client class.
    # pylint: disable=protected-access
    cpplint._cpplint_state.ResetErrorCounts()

    cpplint._SetFilters('-build/c++11,-whitespace/braces')
    files = [
        f.AbsoluteLocalPath() for f in input_api.AffectedSourceFiles(None)
    ]
    CPPLINT_VERBOSE_LEVEL = 4
    for file_name in files:
        cpplint.ProcessFile(file_name, CPPLINT_VERBOSE_LEVEL,
                            [_CheckNoexceptOnMove, _CheckNoValueDchecks])

    if cpplint._cpplint_state.error_count:
        if input_api.is_committing:
            res_type = output_api.PresubmitError
        else:
            res_type = output_api.PresubmitPromptWarning
        return [res_type('Changelist failed cpplint.py check.')]

    return []


def _RunGnCommand(args, root_dir=None):
  """Runs `gn` with provided args and return error if any."""
  try:
    command = [
      sys.executable,
      os.path.join(DEPOT_TOOLS_PATH, 'gn.py')
    ] + args
    subprocess.check_output(command, cwd=root_dir)
  except subprocess.CalledProcessError as err:
    return err.output.decode('utf-8')
  return None

def _GnCheck(output_api, root_dir=None):
  """Runs `gn gen --check` with default args to detect mismatches between
  #includes and dependencies in the BUILD.gn files, as well as general build
  errors.

  Returns a list of error summary strings.
  """
  out_dir = tempfile.mkdtemp('gn')
  try:
    output = _RunGnCommand(['gen', '--check', out_dir], root_dir)
  finally:
    shutil.rmtree(out_dir, ignore_errors=True)

  if not output:
      return []
  return [output_api.PresubmitError(e[1], [e[0]], e[2])
            for e in GN_ERROR_RE.findall(output)]


def _CommonChecks(input_api, output_api):
    # PanProjectChecks include:
    #   CheckLongLines (@ 80 cols)
    #   CheckChangeHasNoTabs
    #   CheckChangeHasNoStrayWhitespace
    #   CheckLicense
    #   CheckChangeWasUploaded (if committing)
    #   CheckChangeHasDescription
    #   CheckDoNotSubmitInDescription
    #   CheckDoNotSubmitInFiles
    results = input_api.canned_checks.PanProjectChecks(input_api,
                                                       output_api,
                                                       owners_check=False)

    # No carriage return characters, files end with one EOL (\n).
    results.extend(
        input_api.canned_checks.CheckChangeHasNoCrAndHasOnlyOneEol(
            input_api, output_api))

    # Ensure code change is gender inclusive.
    results.extend(
        input_api.canned_checks.CheckGenderNeutral(input_api, output_api))

    # Ensure code change to do items uses TODO(bug) or TODO(user) format.
    #  TODO(bug) is generally preferred.
    results.extend(
        input_api.canned_checks.CheckChangeTodoHasOwner(input_api, output_api))

    # Ensure code change passes linter cleanly.
    results.extend(_CheckChangeLintsClean(input_api, output_api))

    # Ensure code change has already had clang-format ran.
    results.extend(
        input_api.canned_checks.CheckPatchFormatted(input_api,
                                                    output_api,
                                                    bypass_warnings=False))

    # Ensure code change has had GN formatting ran.
    results.extend(
        input_api.canned_checks.CheckGNFormatted(input_api, output_api))

    # Run buildtools/checkdeps on code change.
    results.extend(_CheckDeps(input_api, output_api))

    # Run tools/licenses on code change.
    # TODO(https://crbug.com/1215335): licenses check is confused by our
    # buildtools checkout that doesn't actually check out the libraries.
    licenses.PRUNE_PATHS.add(os.path.join('buildtools', 'third_party'));
    results.extend(_CheckLicenses(input_api, output_api))

    # Run `gn check` on code change.
    results.extend(_GnCheck(output_api))

    return results


def CheckChangeOnUpload(input_api, output_api):
    input_api.DEFAULT_FILES_TO_SKIP = _EXCLUDED_PATHS
    # We always run the OnCommit checks, as well as some additional checks.
    results = CheckChangeOnCommit(input_api, output_api)

    # TODO(crbug.com/1220846): Open Screen needs a `main` config_set.
    #results.extend(
    #    input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api))
    return results


def CheckChangeOnCommit(input_api, output_api):
    input_api.DEFAULT_FILES_TO_SKIP = _EXCLUDED_PATHS
    return _CommonChecks(input_api, output_api)
