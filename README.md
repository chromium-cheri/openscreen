## Open Screen Library

This library implements the Open Screen Protocol.  Information about the
protocol can be found on the [github
page](https://github.com/webscreens/openscreenprotocol).

### Build dependencies

Although we plan to address this in the future, there are currently some build
dependencies that you will need to take care to provide yourself.

 - `gcc`
 - `gn`

   If you have a chromium checkout, you can use `//buildtools/<system>/gn`.
   Otherwise, you can use the following script to download a `gn` binary:
   ``` bash
     sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/linux64/gn.sha1?format=TEXT" | base64 --decode)
     curl -Lo gn "https://storage.googleapis.com/chromium-gn/$sha1"
   ```
 - `clang-format`

   `clang-format` may be provided by your OS, found under a chromium checkout at
   `//buildtools/<system>/clang-format`, or downloaded in a similar manner to
   `gn`:
   ``` bash
     sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/linux64/clang-format.sha1?format=TEXT" | base64 --decode)
     curl -Lo clang-format "https://storage.googleapis.com/chromium-clang-format/$sha1"
   ```
 - `ninja`

### Gerrit

The following sections contain some tips about dealing with Gerrit for code
reviews, specifically when pushing patches for review.

#### Uploading a new change

There is official [Gerrit
documentation](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-upload.html#push_create)
for this which essentially amounts to:
```
  git push origin HEAD:refs/for/master
```

The important thing to note, however, is that each commit in
`origin/master..HEAD` will get its own Gerrit change.  If you only want one
change for your current branch, which has multiple commits, you will need to
squash them.  If you wish to maintain your local git history, you can do
something like this:
```
  git branch squash
  git checkout squash
  git rebase -i origin/master # squash into one commit here
  git push origin HEAD:refs/for/master
```

#### Adding a new patchset to an existing change

Gerrit keeps track of changes using a
[Change-Id
line](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-changeid.html)
in each commit.  When there is no Change-Id line, Gerrit creates a new Change-Id
for the commit, and therefore a new change.  Gerrit's documentation for
[replacing a
change](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/user-upload.html#push_replace)
describes this.  So if you want to upload a new patchset to an existing review,
it should contain the appropriate Change-Id line in the commit message.  Of
course, the same note above about commits and changes being 1:1 still applies
here.

#### Uploading a new dependent change

If you wish to work on multiple related changes without waiting for them to
land, you can do so in Gerrit using dependent changes.  There doesn't appear to
be any official documentation for this but the rule seems to be: if the parent
of the commit you are pushing has a Change-Id line, that change will also be the
current change's parent.  This is useful so you can look at only the relative
diff between changes instead of looking at all of them relative to master.

To put this into an example, let's say you have a
commit for feature A with Change-Id: aaa and a commit for feature B with
Change-Id: bbb.  The git history then looks something like this:
```
  ... ----  master
          \
           A
            \
             B <- HEAD
```
In Gerrit, there would then be a "relation chain" shown where the feature A
change is the parent of the feature B change.  If A introduces a new file which
B changes, the review for B will only show the diff from A.

#### Examples for maintaining local git history

```
                  D-E --- feature B
                 /  ^N
            A-B-C-F-G-H --- feature A
           /    ^M  ^O
          /
  ... ----  master
        /|
       M O
       |
       N
```
Consider a local repo with a master branch and two feature branches.  Commits M,
N, and O are squash commits that were pushed to Gerrit.  The arrow/caret (`^`)
indicates whence those were created.  M, N, and O should all have Change-Id
lines in them (this can be done with the [commit-msg
hook](https://gerrit-documentation.storage.googleapis.com/Documentation/2.14.7/cmd-hook-commit-msg.html)).

```
                        D-E --- feature B
                       /  ^Q
            A-B-C-F-G-H --- feature A
           /          ^P
          /
  ... ----  master
         /|\
        M O P
        |   |
        N   Q
```
The next example shows an additional patchset being uploaded for feature A
(commit P) and feature B being rebased onto A, then uploaded to Gerrit as commit
Q.
