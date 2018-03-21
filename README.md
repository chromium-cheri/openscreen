### Build dependencies

Although we plan to address this in the future, there are currently some build
dependencies that you will need to take care to provide yourself.

 - gcc
 - gn
   - If you have a chromium checkout, you can use //buildtools/linux64/gn or
   similar.  Otherwise, you can use the following script to download a gn
   binary:

   ``` bash
   sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/linux64/gn.sha1?format=TEXT" | base64 --decode)
   curl -Lo gn "https://storage.googleapis.com/chromium-gn/$sha1"
   ```
 - clang-format
   - clang-format may be provided by your OS, or found under a chromium checkout
   at //buildtools/linux64/clang-format, or downloaded in a similar manner to
   gn:
   ``` bash
   sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/linux64/clang-format.sha1?format=TEXT" | base64 --decode)
   curl -Lo clang-format "https://storage.googleapis.com/chromium-clang-format/$sha1"
   ```
