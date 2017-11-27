## This is how are pull requests handled by FreeRDP
1. Every new pull request needs to build and pass the unit tests at https://ci.freerdp.com
1. At least 1 (better two) people need to review and test a pull request and agree to accept

## Preparations before creating a pull
* Rebase your branch to current master, no merges allowed!
* Try to clean up your commit history, group changes to commits
* Check your formatting! A _astyle_ script can be found at ```./scripts/format_code.sh```
* Optional (but higly recommended)
  * Run a clang scanbuild before and after your changes to avoid introducing new bugs
  * Run your compiler at pedantic level to check for new warnings

## To ease accepting your contribution
* Give the pull request a proper name so people looking at it have an basic idea what it is for
* Add at least a brief description what it does (or should do :) and what it's good for
* Give instructions on how to test your changes
* Ideally add unit tests if adding new features

## What you should be prepared for
* fix issues found during the review phase
* Joining IRC _#freerdp_ to talk to other developers or help them test your pull might accelerate acceptance
* Joining our mailing list <freerdp-devel@lists.sourceforge.net> may be helpful too.

## Please remove this text before submitting your pull!
