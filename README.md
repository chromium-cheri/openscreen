# Open Screen Library

The openscreen library implements the Open Screen Protocol.  Information about
the protocol and its specification can be found [on
GitHub](https://github.com/webscreens/openscreenprotocol).

## Getting the code

### Install depot_tools

openscreen library dependencies are managed using `gclient`, from the
[depot_tools](https://www.chromium.org/developers/how-tos/depottools) repo.

To get gclient, run the following command in your terminal:
```bash
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Then add the `depot_tools` folder to your `PATH` environment variable.

Note that openscreen does not use other features of `depot_tools` like `git-cl`,
`repo`, or `drover`.

### Check out code

From the parent directory of where you want the openscreen checkout, configure
`gclient` and check out openscreen with the following commands:

```bash
    gclient config https://chromium.googlesource.com/openscreen
    gclient sync
```

Now, you should have `openscreen/` checked out, with all submodule dependencies
checked out to the appropriate revisions.

## Building

### Install build dependencies

Run `./tools/install-build-tools.sh` from the root source directory to obtain a
copy of the following build tools:

 - Build file generator: `gn`
 - Code formatter: `clang-format`
 - Builder: `ninja` ([GitHub releases](https://github.com/ninja-build/ninja/releases))


You also need to ensure that you have the compiler toolchain dependencies.

Currently, both Linux and Mac OS X build configurations use clang by default.

### Linux

On Linux, we download the Clang compiler from the Google storage cache, the same
way that Chromium does it.

On Linux, ensure that libstdc++ 8 is installed, as clang depends on the system
instance of it. On Debian flavors, you can run:

```bash
   sudo apt-get install libstdc++-8-dev
```

### Mac

On Mac OS X, we just use the clang provided by XCode, which must be installed.

### gcc support

Setting the gn argument "is_gcc=true" on Linux enables building using gcc
instead.  Note that g++ must be installed.

### Example

The following commands will build a sample executable and run it.

``` bash
  mkdir out/Default
  gn gen out/Default          # Creates the build directory and necessary ninja files
  ninja -C out/Default hello  # Builds the executable with ninja
  ./out/Default/hello         # Runs the executable
```

The `-C` argument to `ninja` works just like it does for GNU Make: it specifies
the working directory for the build.  So the same could be done as follows:

``` bash
  ./gn gen out/Default
  cd out/Default
  ninja
  ./hello
```

After editing a file, only `ninja` needs to be rerun, not `gn`.  If you have
edited a `BUILD.gn` file, `ninja` will re-run `gn` for you.

TODO: Build the demo instead, once it has some documentation.

### Building other targets

Running `ninja -C out/Default gn_all` will build all non-test targets in the
repository.

`gn ls --type=executable out/Default/` will list all of the executable targets
that can be built.

If you want to customize the build further, you can run `gn args out/Default` to
pull up an editor for build flags. `gn args --list out/Default` prints all of
the build flags available.

## Running unit tests

```bash
  ninja -C out/Default unittests
  ./out/Default/unittests
```

## Continuous build and try jobs 

openscreen uses [LUCI builders](https://ci.chromium.org/p/openscreen/builders)
to monitor the build and test health of the library.  Current builders include:

| Arch | OS | Toolchain | Build type | Notes |
|------|----|-----------|------------|-------|
| x86-64 | Linux Ubuntu 16.04 | clang | debug | ASAN enabled |
| x86-64 | Linux Ubuntu 16.04 | gcc | debug | ASAN enabled |
| x86-64 | Mac OS X/Xcode ??? | clang | debug | |

You can run a patch through the try job queue (which tests it on all builders)
using `git cl try`, or through Gerrit (details below).

## Submitting changes

openscreen library code should follow the [Open Screen Library Style
Guide](docs/style_guide.md).

openscreen uses [Chromium Gerrit](https://chromium-review.googlesource.com/) for
patch management and code review (for better or worse).

The following sections contain some tips about dealing with Gerrit for code
reviews, specifically when pushing patches for review, getting patches reviewed,
and committing patches.

### Preliminaries

You should install the
[Change-Id commit-msg hook](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/cmd-hook-commit-msg.html).
This adds a `Change-Id` line to each commit message locally, which Gerrit uses
to track changes.  Once installed, this can be toggled with `git config
gerrit.createChangeId <true|false>`.

To download the commit-msg hook for the Open Screen repository, use the
following command:

```bash
  curl -Lo .git/hooks/commit-msg https://chromium-review.googlesource.com/tools/hooks/commit-msg
  chmod a+x .git/hooks/commit-msg
```

### Uploading a patch for review

You should run `PRESUBMIT.sh` in the root of the repository before uploading
patches for review (which primarily checks formatting).

There is official [Gerrit
documentation](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-upload.html#push_create)
for this which essentially amounts to:

``` bash
  git push origin HEAD:refs/for/master
```

Gerrit keeps track of changes using a [Change-Id
line](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-changeid.html)
in each commit.

When there is no `Change-Id` line, Gerrit creates a new `Change-Id` for the
commit, and therefore a new change.  Gerrit's documentation for
[replacing a change](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-upload.html#push_replace)
describes this.  So if you want to upload a new patchset to an existing review,
it should contain the matching `Change-Id` line in the commit message.

### Adding a new patchset to an existing change

By default, each commit to your local branch will get its own Gerrit change when
pushed, unless it has a `Change-Id` corresponding to an existing review.

If you need to modify commits on your local branch to ensure they have the
correct `Change-Id`, you can do one of two things:

After committing to the local branch, run:

```bash
  git commit --amend
  git show
```

to attach the current `Change-Id` to the most recent commit. Check that the
correct one was inserted by comparing it with the one shown on
`chromium-review.googlesource.com` for the existing review.

If you have made multiple local commits, you can squash them all into a single
commit with the correct Change-Id:

```bash
  git rebase -i HEAD~4
  git show
```

where '4' means that you want to squash three additional commits onto an
existing commit that has been uploaded for review.

### Tryjobs

Clicking the `CQ DRY RUN` button (also, confusingly, labeled `COMMIT QUEUE +1`)
will run the current patchset through all LUCI builders and report the results.
It is always a good idea get a green tryjob on a patch before sending it for
review to avoid extra back-and-forth.

You can also run `git cl try` from the commandline to submit a tryjob.

### Code reviews

Send your patch to one or more committers in the
[COMMITTERS](https://chromium.googlesource.com/openscreen/+/refs/heads/master/COMMITTERS)
file for code review.  All patches must receive at least one LGTM by a committer
before it can be submitted.

### Submission

After your patch has received one or more LGTM commit it by clicking the
`SUBMIT` button (or, confusingly, `COMMIT QUEUE +2`) in Gerrit.  This will run
your patch through the builders again before committing to the main openscreen
repository.








