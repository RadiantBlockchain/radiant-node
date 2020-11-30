Contributing to Bitcoin Cash Node
=================================

The Bitcoin Cash Node project welcomes contributors!

This guide is intended to help developers and others contribute effectively
to Bitcoin Cash Node.


Communicating with the project
------------------------------

To get in contact with the Bitcoin Cash Node project, we monitor a number
of resources.

Our main development repository is currently located at

[https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node)

This features the project code, an issue tracker and facilities to see
project progress and activities, even in detailed form such as individual
change requests.

Users are free to submit issues or comment on existing ones - all that is
needed is a GitLab account which can be freely registered (use the 'Register'
button on the GitLab page).

In addition to the project repository, we have various other channels where
project contributors can be reached.

Our main chat is at https://bitcoincashnode.slack.com, where we conduct
our main development and interactive support for users of our node.

Other social media resources such as our Telegram and Twitter are linked
from the project website at

[https://bitcoincashnode.org](https://bitcoincashnode.org)

On all our channels, we seek to facilitate development of Bitcoin Cash Node,
and to welcome and support people who wish to participate.

Please visit our channels to

* Introduce yourself to other Bitcoin Cash Node contributors
* Get help with your development environment
* Discuss how to complete a patch.

It is not for:

* Market discussion
* Non-constructive criticism

Bitcoin Cash Node Development Philosophy
----------------------------------------

Bitcoin Cash Node aims for fast iteration and continuous integration.

This means that there should be quick turnaround for patches to be proposed,
reviewed, and committed. Changes should not sit in a queue for long.

Here are some tips to help keep the development working as intended. These
are guidelines for the normal and expected development process. Developers
can use their judgement to deviate from these guidelines when they have a
good reason to do so.

- Keep each change small and self-contained.
- Reach out for a 1-on-1 review so things move quickly.
- Merge accepted changes quickly after they are accepted.
- Don't amend a Merge Request after it has been accepted for merging unless with coordination with the maintainer(s)
- Review merge requests from other developers as quickly as possible.
- Large changes should be broken into logical chunks that are easy to review,
and keep the code in a functional state.
- Do not mix moving stuff around with changing stuff. Do changes with renames
on their own.
- Sometimes you want to replace one subsystem by another implementation,
in which case it is not possible to do things incrementally. In such cases,
you keep both implementations in the codebase for a while, as described
[here](https://www.gamasutra.com/view/news/128325/Opinion_Parallel_Implementations.php)
- There are no "development" branches, all merge requests apply to the master
branch, and should always improve it (no regressions).
- As soon as you see a bug, you fix it. Do not continue on. Fixing the bug becomes the
top priority, more important than completing other tasks.
- Do not break the build, it is important to keep master green as much as possible.
If a merge has been done and breaks the build, fix it quickly. If it cannot be fixed
quickly, it should be reverted, and re-applied later when it no longer breaks the build.
- Automate as much as possible, and spend time on things only humans can do.

Here are some handy links for development practices aligned with Bitcoin Cash Node:

- [BCHN GitLab development working rules and guidelines](doc/bchn-gitlab-usage-rules-and-guidelines.md)
- [Developer Notes](doc/developer-notes.md)
- [How to Do Code Reviews Like a Human - Part 1](https://mtlynch.io/human-code-reviews-1/)
- [How to Do Code Reviews Like a Human - Part 2](https://mtlynch.io/human-code-reviews-2/)
- [Large Diffs Are Hurting Your Ability To Ship](https://medium.com/@kurtisnusbaum/large-diffs-are-hurting-your-ability-to-ship-e0b2b41e8acf)
- [Parallel Implementations](https://www.gamasutra.com/view/news/128325/Opinion_Parallel_Implementations.php)
- [The Pragmatic Programmer: From Journeyman to Master](https://www.amazon.com/Pragmatic-Programmer-Journeyman-Master/dp/020161622X)
- [Advantages of monolithic version control](https://danluu.com/monorepo/)
- [The importance of fixing bugs immediately](https://youtu.be/E2MIpi8pIvY?t=16m0s)
- [Good Work, Great Work, and Right Work](https://forum.dlang.org/post/q7u6g1$94p$1@digitalmars.com)
- [Accelerate: The Science of Lean Software and DevOps](https://www.amazon.com/Accelerate-Software-Performing-Technology-Organizations/dp/1942788339)


Getting set up with the Bitcoin Cash Node Repository
----------------------------------------------

1. Create an account at [https://gitlab.com](https://gitlab.com) if you don't have one yet
2. Install Git on your machine
    - Git documentation can be found at: [https://git-scm.com](https://git-scm.com)
    - To install these packages on Debian or Ubuntu, type: `sudo apt-get install git`
3. If you do not already have an SSH key set up, follow these steps:
    - Type: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
    - Enter a file in which to save the key (/home/*username*/.ssh/id_rsa): [Press enter]
    - *NOTE: the path in the message shown above is specific to UNIX-like systems and may differ if you run on other platforms.*
4. Upload your SSH public key to GitLab
    - Go to: [https://gitlab.com](https://gitlab.com), log in
    - Under "User Settings", "SSH Keys", add your public key
    - Paste contents from: `$HOME/.ssh/id_rsa.pub`
5. Create a personal fork of the Bitcoin Cash Node repository for your work
    - Sign into GitLab under your account, then visit the project at [https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node)
    - Click the 'Fork' button on the top right, and choose to fork the project to your personal GitLab space.
6. Clone your personal work repository to your local machine:

    ```
    git clone git@gitlab.com:username/bitcoin-cash-node.git
    ```

7. Set your checked out copy's upstream to our main project:

    ```
    git remote add upstream https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node.git
    ```

8. You may want to add the `mreq` alias to your `.git/config`:

        [alias]
        mreq = !sh -c 'git fetch $1 merge-requests/$2/head:mr-$1-$2 && git checkout mr-$1-$2' -

    This `mreq` alias can be used to easily check out Merge Requests from our
    main repository if you intend to test them or work on them.
    Example:

    ```
    $ git mreq upstream 93
    ```

    This will checkout `merge-requests/93/head` and put you in a branch called `mr-origin-93`.
    You can then proceed to test the changes proposed by that merge request.

9. Code formatting tools

    During submission of patches, our GitLab process may refused code that
is not styled according to our coding guidelines.

    To enforce Bitcoin Cash Node codeformatting standards, you may need to
install `clang-format-8`, `clang-tidy` (version >=8), `autopep8`, `flake8`,
`phpcs` and `shellcheck` on your system to format your code before submission
as a Merge Request.

    To install `clang-format-8` and `clang-tidy` on Ubuntu (>= 18.04+updates) or Debian (>= 10):

    ```
    sudo apt-get install clang-format-8 clang-tidy-8 clang-tools-8
    ```

    If not available in the distribution, `clang-format-8` and `clang-tidy` can be
installed from [https://releases.llvm.org/download.html](https://releases.llvm.org/download.html) or [https://apt.llvm.org](https://apt.llvm.org).

    For example, for macOS:

        curl http://releases.llvm.org/8.0.0/clang+llvm-8.0.0-x86_64-apple-darwin.tar.xz | tar -xJv
        ln -s $PWD/clang+llvm-8.0.0-x86_64-apple-darwin/bin/clang-format /usr/local/bin/clang-format
        ln -s $PWD/clang+llvm-8.0.0-x86_64-apple-darwin/bin/clang-tidy /usr/local/bin/clang-tidy

    To install `autopep8`, `flake8` and `phpcs` on Ubuntu:

    ```
    sudo apt-get install python-autopep8 flake8 php-codesniffer shellcheck
    ```


Working with The Bitcoin Cash Node Repository
---------------------------------------------

A typical workflow would be:

- Create a topic branch in Git for your changes

    git checkout -b 'my-topic-branch'

- Make your changes, and commit them

    git commit -a -m 'my-commit'

- Push the topic branch to your GitLab repository

    git push -u origin my-topic-branch

- Then create a Merge Request (the GitLab equivalent of a Pull Request)
  from that branch in your personal repository. To do this, you need to
  sign in to GitLab, go to the branch and click the button which lets you
  create a Merge Request (you need to fill out at least title and description
  fields).

- Work with us on GitLab to receive review comments, going back to the
  'Make your changes' step above if needed to make further changes on your
  branch, and push them upstream as above. They will automatically appear
  in your Merge Request.

All Merge Requests should contain a commit with a test plan that details
how to test the changes. In all normal circumstances, you should build and
test your changes locally before creating a Merge Request.
A merge request should feature a specific test plan where possible, and
indicate which regression tests may need to be run.

If you have doubts about the scope of testing needed for your change,
please contact our developers and they will help you decide.

- For large changes, break them into several Merge Requests.

- If you are making numerous changes and rebuilding often, it's highly
  recommended to install `ccache` (re-run cmake if you install it
  later), as this will help cut your re-build times from several minutes to under
  a minute, in many cases.

What to work on
---------------

If you are looking for a useful task to contribute to the project, a good place
to start is the list of issues at [https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/issues](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/issues)

Look for issues marked with a label 'good-first-issue'.

Copyright
---------

By contributing to this repository, you agree to license your work under the
MIT license unless specified otherwise in `contrib/debian/copyright` or at
the top of the file itself. Any work contributed where you are not the original
author must contain its license header with the original author(s) and source.

Disclosure Policy
-----------------

See [DISCLOSURE_POLICY](DISCLOSURE_POLICY.md).
