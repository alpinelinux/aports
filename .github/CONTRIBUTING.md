# How to contribute

We are really glad you are reading this contribution guide. This means you care about the quality of your contributions.

## Submitting patches via Github

### Prerequisite for submitting patches/PR's

1. Basic knowledge about how to us Git(hub)
  * Properly setting up git with `git config`
  * Know how to create a commit (for details see below).
  * Know how to work with git history (rebasing your commits).
  * Know how to create Pull Requests
2. Have access to an Alpine Linux [development envirorment](https://wiki.alpinelinux.org/wiki/Developer_Documentation#Development).
3. Please __do not__ submit PR's via Github's web interface. You should have a working [development envirorment](https://wiki.alpinelinux.org/wiki/Developer_Documentation#Development) available and submit your commits from the git interface.

### Creating a Pull Request (PR)

1. [Fork](https://help.github.com/articles/fork-a-repo/) our alpinelinux aports repository.
2. Clone your copy of aports `git clone https://github.com/username/aports`
3. Create a feature branch `git checkout -b my_new_feature`
4. Make your desired changes and commit them with a correct commit message.
  * Begin the commit message with a single short (less than 78 character) line summarizing the changes, followed by a blank line and then a more thorough description. Examples of correct formated summary lines are:
    * When adding a new aport: testing/apkname: new aport
    * When modifying an aport: testing/apkname: short description about changes
  * If needed provide a proper formatted (line wrapped) description of what your patch will do. You can provide a description in the PR, but you must include a message for this specific commit in the commit description. If in the future we would like to distance ourselfs from Github the PR information could be lost.
5. Open the aports repository in your github.com and switch to your feature branch. You should now see an option to create your PR. [More info](https://help.github.com/articles/creating-a-pull-request/)
6. Wait for an Alpine Linux developer to review your changes
7. If all is ok your PR will be merged but if a developer asks for changes please do as followed.
  * Make the requested changes.
  * add your file to git and `git commit --amend` (this keeps the commit log clean).
8. Goto #6.

### Submitting a package with new dependencies

When you want to submit a package including its new dependencies to our repository, you should bundle these commits into a single PR. This is needed so our [CI](https://en.wikipedia.org/wiki/Continuous_integration) will first build the dependencies after which it will build the parent package. Failing to include __new__ dependencies will fail the CI tests.

### Clean-up a Pull Request (PR)

If by some mistake you end up with multiple commits in your PR and one of our developers asks you to squash you commits please do __NOT__ create a new pull request instead please follow [this rebase tutorial](https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History#Changing-Multiple-Commit-Messages).
