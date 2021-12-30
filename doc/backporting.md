BACKPORTING
===========

The official guide to backporting code between Bitcoin Cash Node and the Core or
ABC clients. When searching for items to backport, especially be on the lookout
for bug fixes, code cleanup, and important refactors, as these help improve
Bitcoin Cash Node despite consensus-level differences between Bitcoin Cash and
Bitcoin Core.

Identifying commits
-------------------

1. Check out a copy of a Satoshi Bitcoin client somewhere on your machine.
2. Identify the subsystem you'd like to work on.
3. Tag the fork commit as `fork-commit`. Bitcoin ABC was forked from Bitcoin Core
   at commit `964a185cc83af34587194a6ecda3ed9cf6b49263`.
   `> git tag -a fork-commit 964a185 -m 'Where the fun started'`
4. `git log --topo-order --graph fork-commit..HEAD -- <file or folder of interest>`
5. Find the commit, and if applicable, the associated merge commit that
   are worth backporting.  The merge commit may indicate that there were other
   commits associated with this change that you will need to backport. E.g:

```
commit d083bd9b9c5249f21b8b7e4abd7aee48a25806b1
Merge: b3eb0d648 279fde58e
Author: Wladimir J. van der Laan <laanwj@gmail.com>
Date:   Wed Jun 21 14:26:10 2017 +0200

    Merge #10533: [tests] Use cookie auth instead of rpcuser and rpcpassword

    279fde5 Check for rpcuser/rpcpassword first then for cookie (Andrew Chow)
    3ec5ad8 Add test for rpcuser/rpcpassword (Andrew Chow)
    c53c983 Replace cookie auth in tests (Andrew Chow)

    Tree-SHA512: 21efb84c87080a895cac8a7fe4766738c34eebe9686c7d10af1bf91ed4ae422e2d5dbbebffd00d34744eb6bb2d0195ea3aca86deebf085bbdeeb1d8b474241ed
```

If you saw that commit `c53c983` was a good idea to backport, this merge
commit would indicate that there are two other commits that are associated
with this PR and likely to be needed.

When trying to find a patch worth backporting, it's generally a good idea to
backport significant refactors or bugfixes.  This will help clean up the code
in the Bitcoin Cash Node repository, fix bugs, and make future backports significantly
easier. Backports are easiest done in topological order of commits.

Adding remotes for backporting
------------------------------

Before you begin backporting commits, you will need to add an additional remote
to your Bitcoin Cash Node repo.

For Bitcoin ABC, this repository would be added as:

```
git remote add abc https://reviews.bitcoinabc.org/source/bitcoin-abc.git
git fetch abc
```

For Bitcoin Core, this repository would be added as:

```
git remote add core git@github.com:bitcoin/bitcoin.git
git fetch core
```

(Assuming you have github ssh auth setup. The second command is required to
obtain refs for cherry-picking.)

Backporting one or more commits
-------------------------------

1. `git checkout -b <name-of-branch>`
2. `git cherry-pick <commit-of-interest>`
3. Run `git status` and fix conflicts.
4. `git add -u && git cherry-pick --continue`
5. Run `git show` side-by-side with `git show <commit-of-interest>` and verify
   that the changes are reasonable. Resolve any merge conflicts you encounter.
6. Run the build, and the rpc test suite and verify completion.
7. Repeat steps 2 through 6 if you want to backport more than one commit. (You
   can choose to squash commits later in step 9.)
8. Commit and push the staged backport to a feature/fix branch on your clone of
   the Bitcoin Cash Node repository.
9. Create a Merge Request from that branch in GitLab. In the overview of your
   Merge Request, mention "Backport of Bitcoin ABC Dxxxxx" and/or "Backport of
   Bitcoin Core PR#xxxxx". The differential number (ABC) or pull request number
   (Core) can be obtained by searching [Phabricator (ABC)](https://reviews.bitcoinabc.org/differential/query/all/)
   or [GitHub (Core)](https://github.com/bitcoin/bitcoin/pulls/) for the commit
   you are backporting. If you are backporting a commit which depends on another
   commit from the same D/PR, note that you are backporting "Part 1 of X".
