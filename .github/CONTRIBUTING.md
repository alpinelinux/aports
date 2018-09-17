# How to contribute

We are really glad that you are reading this contribution guide.
This means you care about the quality of your contributions.

## Submitting patches via Github

### Prerequisite for submitting patches/PR's

1. Basic knowledge about how to use Git(hub)
  * [Properly](https://git-scm.com/book/en/v2/Getting-Started-First-Time-Git-Setup) setup your git client.
  * Know how to create a commit (for details see below).
  * Know how to work with git history (rebasing your commits).
  * Know how to create Pull Requests.
2. Have access to an Alpine Linux [development environment](https://wiki.alpinelinux.org/wiki/Developer_Documentation#Development).
3. Please __do not__ submit PR's via Github's web interface. You should have a working [development environment](https://wiki.alpinelinux.org/wiki/Developer_Documentation#Development) available and submit your commits from your git client.

### Before you start making changes

1. Check whether the change you want to introduce is not already present in the aports repository.  
   Main aports repository can be cloned from: git://git.alpinelinux.org/aports  
   There is also web frontend available at:  http://git.alpinelinux.org/cgit/aports  
   GitHub mirror of aports repository:      https://github.com/alpinelinux/aports
2. Check whether the change you want to introduce has not been already submitted by someone else.
   - https://patchwork.alpinelinux.org/project/aports/list/
   - https://github.com/alpinelinux/aports/pulls
3. If you've found that someone already did what you wanted to do, then be patient. The change should be applied soon in the main repository, unless maintainers are busy or the patch needs to be improved.  
   If it seems that your change won't be duplicating already done work, then please continue.

### Creating a Pull Request (PR)

1. [Fork](https://help.github.com/articles/fork-a-repo/) our Alpine Linux aports repository.
2. Clone your copy of aports `git clone https://github.com/username/aports`.
3. Create a feature branch `git checkout -b my_new_feature`.
4. Make your desired changes and commit them with a [correct commit message](https://git-scm.com/book/ch5-2.html#Commit-Guidelines).
  * Begin the commit message with a single short line summarizing the changes, followed by a blank line and then a more thorough description. Examples of correctly formated summary lines are:
    * When adding a new aport: testing/apkname: new aport
    * When modifying an aport: testing/apkname: short description about changes
  * If needed provide a proper formatted (line wrapped) description of what your patch will do. You can provide a description in the PR, but you must include a message for this specific commit in the commit description. If in the future we would like to distance ourselves from Github the PR information could be lost.
5. Open your copy of the aports repository at github.com and switch to your feature branch. You should now see an option to create your PR. [More info](https://help.github.com/articles/creating-a-pull-request/)
6. Wait for an Alpine Linux developer to review your changes.
7. If all is ok your PR will be merged but if a developer asks for changes please do as follows:
  * Make the requested changes.
  * Add your file(s) to git and commit (we will squash your commits if needed).
  * Push your changes `git push origin my_new_feature`.
8. Goto #6.

### Submitting a package with new dependencies

When you want to submit a package including its new dependencies to our repository, you should bundle these commits into a single PR.
This is needed so our [CI](https://en.wikipedia.org/wiki/Continuous_integration) will first build the dependencies after which it will build the parent package.
Failing to include __new__ dependencies will fail the CI tests.

### Clean-up a Pull Request (PR)

If by some mistake you end up with multiple commits in your PR and one of our developers asks you to squash your commits please do __NOT__ create a new pull request.
Instead please follow [this rebase tutorial](https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History#Changing-Multiple-Commit-Messages).

### Pull Request (PR) max age

Pull Requests that have not been updated in the last 180 days will automatically be labeled S-stale. After 7 days of additional inactivity the PR will automatically be closed except if one of the Alpine developers will label it with S-WIP (Work In Progress).
