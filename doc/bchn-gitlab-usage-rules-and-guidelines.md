# BCHN GitLab development working rules & guidelines

This document describes the working rules, workflow, terminology and guidelines
that developers and testers should be familiar with while working on the Bitcoin
Cash Node repository and issue tracker at

[https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/)

## BCHN GitLab workflow

Below are common workflows users will encounter:

- Raise an Issue if you experience a problem or have a suggestion for improving
  the product

- Fork the BCHN Repository

- Create a branch for your change

- Commit your changes

- Create a Merge Request (MR)

You will need to create a GitLab account if you don't have one (it's free to do so).

## General rules for contributors

The following are rules and should always be followed.

### Rule 0

If it seems wrong to follow a rule in some instance, query back with
the maintainer about whether the process needs adjustment.

### General Rule 1

Any exceptions to following the rules shall always be documented
by a comment on GitLab. If you see a rule violation, ask the person (ping them on
GL) to ask them to comment and explain why they did what they did.

### General Rule 2

For developers, when approving a merge request ("Approve" button),
a comment must be left on what basis the approval is made. It can be in the terse
form (ref. "Code review lingo") or in a more descriptive comment.

### General Rule 3

Every MR shall have a test plan accompanying it its description.
Example of a (very simple) test plan:

> "Added unittest. Run `ninja check-all`."

### General Rule 4

The contributor must execute his own test plan, or if unable
to do so for whatever reason, explicitly transfer this responsibility to the maintainer.

## Rules for maintainers

### Maintainer Rule 1

Maintainers shall only merge requests which have been code
reviewed.

### Maintainer Rule 2

Consensus changes shall be manually tested and verified
before merge.

### Maintainer Rule 3

Maintainers shall not merge their own changes (unless with
exceptions)

### Maintainer Rule 4

All threads on the merge request must be marked as resolved
prior to merging.

### Maintainer Rule 5

MRs with "HODL" label set shall not be merged - maintainers
must follow up status

### Maintainer Rule 6

Any change to consensus code shall be treated as non-trivial
and trigger the minimum review period in Guideline 4 before merging (unless it is
a noted emergency)

### Maintainer Rule 7

A non-trivial change to critical code must be approved by at
least two maintainers. Critical code is code where a bug could have severe
consequences for users if it is not spotted during review, including but not
limited to consensus rules (risk of acceptance or creation of invalid blocks)
and cryptographic functions (risk of loss of funds).
Maintainers may decide as a group whether a set of non-consensus changes meets the
criticality bar, with the lead maintainer breaking a possible tie. Such
decisions shall be reflected on the merge request in GitLab, most preferably
via explicit comments but definitely via raising the Approval threshold of the
merge request in its configuration to 2.

## Guidelines

The following are guidelines and should be followed if possible.

### Guideline 1

Assign issues to yourself when starting to work on them, if they
are unassigned. You don't need anyone's permission, but if it's an issue with past
history, it is polite to query first if there is a chance your action might get
in the way of someone.

### Guideline 2

If an issue is assigned to someone else but you can do some task
that you think is helpful, you can do so without assigning it to yourself, or co-assigning
it to yourself to indicate that you are also working on something related to that
issue. You can also query (via GitLab or otherwise) with the current Assignee whether
they want to hand it over to you, in case they are currently inconvenienced or blocked
from progressing it.

### Guideline 3

Use appropriate project checklists when reviewing anything. If
you don't find a checklist, bug the maintainers to provide you one.

### Guideline 4

For non-trivial contributions, merge requests that are not
work-in-progress should sit for at least 36 hours to ensure that contributors in
other timezones have time to review. Consideration should be given to weekends
and other holiday periods to ensure active committers all have reasonable time to
become involved in the discussion and review process if they wish.

### Guideline 5

It is asked that maintainers (active committers) indicate their
unavailability when they know they will be absent from the project for longer periods
of time. This helps keep momentum going, allowing others to step in quicker to help
land merge requests.

### Code review lingo

Commonly used lingo when reviewing a MR

- "Concept ACK" - "I have not reviewed the code, but I approve of this change
  conceptually"

- ACK - "I have reviewed and tested the code"

- utACK - "(untested ACK) I have reviewed the code"

- tACK - "(test-only ACK) I have only run the tests on the code, not able to review
  it"

- NACK - "I don't approve of this change because of review or verification issues
  with it"

### Use of labels, commit tags and "Draft"

#### Labels

GitLab has a facility for decorating issues and Merge Requests with labels. Please
see below for a list of known labels.

All labels should a clearly explained meaning and purpose. It is suggested that
if a label is not documented, that an issue is opened to notify the maintainers
of update the documentation.

labels on issues may change during lifetime of an issue / MR.

Anyone can set labels (add new, change, remove) at any time, the GitLab system
records all these label changes so it remains traceable who did what, when.

#### Tags

Committers can add prefix tags to their commit messages and MR titles to provide
brief visual summary of their committed or proposed changes.

These tags have no special meaning to GitLab (at least not yet) and are simply for
human consumption at this stage.They are not well standardized, but below you will
find a section documenting some better known commit tags.

#### Draft

When a merge requested is prefixed by "Draft:", GitLab blocks merging of the MR.

People who submit or work on a merge request should set the "Draft" prefix as long
as they know the MR will still receive extensive revision and is not yet in a state
ready for merge review.

"Draft" is a signal to others that they need not yet review an MR or issue - it is
not ready for review yet.

If you have worked an issue/MR and feel it is now ready for further processing
(review and line-up for merging) then please remove the "Draft:" prefix.
The issue/MR will attract intention soon, but you can also invite others for
review explicitly either through assignment or letting them know via comments or
on developer chat channels.

Occasionally, developers may ask for review of Draft items in the understanding that
such review is preliminary and intended to gather input for revision which is believed
to be needed still.

### List of labels currently used on BCHN GitLab

NOTE: MR = "Merge Request"

- "benchmark" : issue/MR is classified as relating to product benchmarks

- "bug" : issue/MR is classified as relating to some product deficiency
  (either w.r.t. a specification or design, or as experienced by a user)

- "consensus" : issue/MR is classified as relating to consensus change

- "consensus-sensitive" : issue/MR is classified as relating to consensus relevant
  functionality, but not intending a change to consensus

- "enhancement" : issue/MR is classified as relating to an improvement of the
  product which is not yet covered by a specification or design

- "backport" : issue/MR is classified as relating to an backport of a fix or enhancement
  from some upstream source (e.g. Bitcoin Core or Bitcoin ABC)

- "build": issue/MR is classified as relating to the software build system

- "devenv": issue/MR is classified as relating to development environment

- "qa": issue/MR is classified as relating to the product quality assurance (QA)
  process or supporting tools

- "documentation": issue/MR is classified as relating to the product documentation

- "code-quality": issue/MR is classified as relating to fixing code quality problems
  (compiler warnings, cleanups, refactorings, nits)

- "for-review": issue/MR is classified as needing review

- "good-first-issue": issue/MR is classified as a suitable entry-level issue/MR
  for those wanting to help the project

- "in_progress": issue/MR is classified as being in progress

- "needs-testing": issue/MR is finished with implementation but needs someone else
  to take up testing

- "non-blocking": issue/MR is classified as not blocking for configuration management
  of an upcoming release

- "release-blocking": this issue or MR relates to something that may block an
  upcoming release

- "performance": issue/MR is related to performance aspects of the software. This
  label can be used in conjunction with others like "bug" or "enhancement". NOTE:
  The non-functional requirements of the product, such as performance requirements,
  are currently not well specified.

- "tests" : issue/MR is related to testing only (test framework or some set of tests)

- "too_big": issue/MR has been assessed and deemed too big, i.e. needs to be split
  up

- "splitting_up": issue/MR is the result of a splitting up action on a "too_big"
  issue/MR (which should be referenced by linking in the description or comments)

- "user-interface": issue/MR relates to a user interface (not only graphical!)

- "mining": issue/MR relates to mining functionality

- "wallet": issue/MR relates to wallet functionality

- "bitcoin-seeder": issue/MR relates to bitcoin-seeder component

- "HODL" : issue/MR is on hold until further notice, usually because of a technical
  problem

- "HODL GANG" : issue/MR needs ecosystem feedback for further decision

- "community" : issue/MR is classified as relating to some changes that touch on
  the social aspects of the projects, such as expectations of maintainers to
  contributors and vice versa

- "gitian": issue/MR that touches on the gitian build system, or requires a gitian
  build as part of its test plan

- "bounty": issue/MR has an associated open or in-progress bounty. NOTE: The label
  should be removed once a bounty is awarded.

- "engineering-change-proposal": issue is a proposal for an engineering change to
  the BCHN software or infrastructure

- "bitcoin-tx": issue/MR is classified as relating to bitcoin-tx tool

- "SECURITY": issue/MR is considered security relevant

### Historical labels considered to be deprecated until further notice

These labels below are historical - you might see them when examining closed issues
or MRs, but which should not be used for now in new ones without consulting maintainers:

- "teamcity": related to removal of Teamcity CI during BCHN v0.21.0 software creation
  in 2020

- "needs_tag": used to mark issues/MRs that need labeling, but deemed superfluous

- "fix-warnings": replace by more general "code-quality" label

### Known commit tags

Commit tags are added in square brackets in front of the usual commit message.

The commit tags below are used reasonably frequently.

- "[doc]": this commit changes only documentation files

- "[ci]": this commit changes files which steer the Continuous Integration process

- "[qa]" or "QA": this commit relates to files guiding Quality Assurance process

- "[backport]" or "[port]": this commit is related to a (back)port of a change
  from external source

- "[contrib]": this commit is related to contributed files (those in contrib/ folder)

- "[build]": this commit is related to the build system (unspecified aspects)

- "[cmake]": this commit is related to the build system (cmake aspects)

- "[rebrand]": this commit is related to rebranding changes

There are some more tags in older history, from ABC project and others, which are
not used by BCHN developers currently, but may enter the recently history through
backports.

## Notes

Note 1. [test plans] For now, it can be relatively free form. Best test plans are
"execute this new automated test which I added in my MR". Automation will not be
possible yet in all cases, so less formal test plans are acceptable too, as long
as they properly cover the changed functionality.
