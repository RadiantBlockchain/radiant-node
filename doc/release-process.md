# Release Process

## Before Release

1. Check configuration
    - Check features planned for the release are implemented and documented
      (or more informally, that the Release Manager agrees it is feature complete)
    - Check that finished tasks / tickets are marked as resolved
2. Verify tests passed
    - Any known issues or limitations should be documented in release notes
    - Known bugs should have tickets
    - Run `arc lint $(find contrib/)` and check there is no linter error
    - Run `arc lint *.md $(find doc/)` from top level folder and check there
      are no new linter error (except a suggestion to fix 'reenable' spelling
      in an URL in `doc/bch-upgrades.md`, a suggestion which should be ignored.
    - Run `arc lint` on any Markdown or other text documents within `src/`
      and `depends/` which are not from third-party sources.
      "Our" documents within `src/` and `depends/` should all be clean.
      NOTE: We do not run `arc lint` on the other source files anymore.
    - Ensure that bitcoind and bitcoin-qt run with no issue on all supported
      platforms.
      Manually test bitcoin-qt by sending some transactions and navigating
      through the menus.
3. Update the documents / code which needs to be updated every release
    - Update [bips.md](bips.md) and [bch-upgrades.md](bch-upgrades.md) to account
      for changes since the last release.
    - (major releases) Update [`BLOCK_CHAIN_SIZE`](../src/qt/intro.cpp) to the
      current size plus some overhead.
    - Update seeds as per [contrib/seeds/README.md](../contrib/seeds/README.md).
    - Update [`src/chainparams.cpp`](../src/chainparams.cpp) m_assumed_blockchain_size
      and m_assumed_chain_state_size with the current size plus some overhead.
    - Update the chain parameters (see `contrib/devtools/chainparams/README.md`)
    - Run the refresh procedure for documents that are automatically generated
    - Check that [release-notes.md](release-notes.md) is complete, and fill in
      any missing items.
4. Add git tag for release
    a. Create the tag: `git tag vM.m.r` (M = major version, m = minor version,
       r = revision)
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

1. Create Gitian Builds (see [gitian-building.md](gitian-building.md))
2. Verify matching Gitian Builds, gather signatures
3. Verify IBD bith with and without `-checkpoints=0 -assumevalid=0`
4. Upload Gitian Builds to [bitcoincashnode.org](https://bitcoincashnode.org/)
5. Create a [release](https://github.com/bitcoin-cash-node/bitcoin-cash-node)
   on our GitHub mirror: `contrib/release/github-release.sh -a <path to release binaries> -t <release tag> -o <file containing your Github OAuth token>`
6. Create [Ubuntu PPA packages](https://launchpad.net/~bitcoin-cash-node/+archive/ubuntu/ppa):
   Maintainers need to clone [packaging](https://gitlab.com/bitcoin-cash-node/bchn-sw/packaging)
   and follow instructions to run `debian-packaging.sh` in that repository.
7. Notify maintainers of AUR and Docker images to build their packages.
   They should be given 1-day advance notice if possible.

## After Release

1. Update version number on www.bitcoincashnode.org
2. Publish signed checksums (various places, e.g. blog, reddit, etc. etc.)
3. Announce Release:
    - [Reddit](https://www.reddit.com/r/bitcoincashnode/)
    - Twitter @bitcoincashnode
    - Public slack channels friendly to Bitcoin Cash Node announcements
