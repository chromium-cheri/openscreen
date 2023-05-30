# Updating Dependencies

The Open Screen Library uses several external libraries that are compiled from
source and fetched from additional git repositories into folders below
`third_party/`.  These libraries are listed in the `DEPS` file in the root of
the repository.  (`DEPS` listed with `dep_type: cipd` are not managed through git.)

Periodically the versions of these libraries are updated to pick up bug and
security fixes from upstream repositories. This process is called "rolling DEPS."

Note that depot_tools is in the process of migrating dependency management from
`DEPS` to git submodules.  This means during the migration period the update has
to happen in two places.

The process is roughly as follows:

1. Identify the git commit for the new version of the library that you want to roll.
   * For libraries that are also used by Chromium, prefer rolling to the same version
     listed in [`Chromium DEPS`](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/DEPS).
     These libraries are listed below.
   * For other libraries, prefer the most recent stable or general availablity release.
2. Update the git hash in the Open Screen `DEPS`.
3. Update the git link for the folder, replacing `<git hash>` and `<path>`
   with the same values from `DEPS`:

```
$ git update-index --add --cacheinfo 160000,<git hash>,<path>
```

4. Update the `BUILD.gn` file for the corresponding library and make any source
  code changes to handle header files moving around, API adjustments, compiler
  flag adjustments, etc.
5. Upload your patch and use a tryjob to verify that nothing is broken.

Note, some repositories require extra steps, please check if there is a
README.openscreen.md file in the source folder for any additional instructions.

## Libraries also used by Chromium

* `third_party/protobuf/src` (soft-forked by Chromium, so not kept in sync)
* `third_party/libprotobuf-mutator/src`
* `third_party/jsoncpp/src`
* `third_party/googletest/src` (not kept in sync with Chromium, see comment)
* `third_party/quiche/src`
* `third_party/abseil/src` (not kept in sync with Chromium, see comment)
* `third_party/libfuzzer/src`



