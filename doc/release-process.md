Bitcoin Cash Node Release Process
=================================


## Before Release

1. Check configuration
    - Check features planned for the release are implemented and documented
      (or more informally, that the Release Manager agrees it is feature complete)
    - Check that finished tasks / tickets are marked as resolved

2. Verify tests passed
    - Any known issues or limitations should be documented in release notes
    - Known bugs should have tickets
    - Run `arc lint --everything` and check there is no linter error
    - Ensure that bitcoind and bitcoin-qt run with no issue on all supported platforms.
      Manually test bitcoin-qt by sending some transactions and navigating through the menus.

3. Update the documents / code which needs to be updated every release
    - Check that [release-notes.md](doc/release-notes.md) is complete, and fill in any missing items.
    - Update [bips.md](/doc/bips.md) to account for changes since the last release.
    - (major releases) Update [`BLOCK_CHAIN_SIZE`](/src/qt/intro.cpp) to the current size plus
      some overhead.
    - Regenerate manpages (run `contrib/devtools/gen-manpages.sh`, or for out-of-tree builds run
      `BUILDDIR=$PWD/build contrib/devtools/gen-manpages.sh`).
    - Update seeds as per [contrib/seeds/README.md](/contrib/seeds/README.md).

4. Add git tag for release
    a. Create the tag: `git tag vM.m.r` (M = major version, m = minor version, r = revision)
    b. Push the tag to GitLab:
        ```
        git push <gitlab remote> master
        git push <gitlab remote> vM.m.r
        ```

5. Increment version number for the next release in:
    - `doc/release-notes.md` (and copy existing one to versioned `doc/release-notes/*.md`)
    - `configure.ac`
    - `CMakeLists.txt`
    - `contrib/seeds/makeseeds.py` (only after a new major release)

## Release

6. Create Gitian Builds (see [gitian-building.md](/doc/gitian-building.md))

7. Verify matching Gitian Builds, gather signatures

8. Verify IBD bith with and without `-checkpoints=0 -assumevalid=0`

9. Upload Gitian Builds to [bitcoincashnode.org](https://bitcoincashnode.org/)

10. Create a [release](https://github.com/bitcoin-cash-node/bitcoin-cash-node) on our GitHub mirror:
    `contrib/release/github-release.sh -a <path to release binaries> -t <release tag> -o <file containing your Github OAuth token>`

11. Notify maintainers of Ubuntu PPA, AUR, and Docker images to build their packages.
    They should be given 1-day advance notice if possible.

## After Release

12. Update version number on www.bitcoincashnode.org

13. Publish signed checksums (various places, e.g. blog, reddit, etc. etc.)

14. Announce Release:
    - [Reddit](https://www.reddit.com/r/bitcoincashnode/)
    - Twitter @bitcoincashnode
    - Public slack channels friendly to Bitcoin Cash Node announcements

