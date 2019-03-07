Run `git config -e` and add the block below then run `git fetch --all` to update all the GitHub pull request branches.

```
[remote "github"]
        url = https://github.com/alpinelinux/aports.git
        fetch = +refs/heads/*:refs/remotes/github/*
        fetch = +refs/pull/*:refs/remotes/github/pr/*
```

After you have done this you can merge / squash / rebase / cherry-pick from the command line.

For example, `git rebase master github/pr/XXX/head; git checkout master` would migrate all the commits from PR #XXX.

You can then push the results back to the "origin" remote which should be the official Alpine Linux aports repository.
