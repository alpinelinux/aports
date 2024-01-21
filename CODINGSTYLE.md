# Policy for APKBUILDs

This documents defines a policy for writing APKBUILDs.

# Standard selection

APKBUILDs are POSIX shell scripts as defined in [POSIX.1-2017 Volume 3]
[POSIX.1-2017 volume 3]. Additionally, the following extensions are
supported:

1. The `local` keyword for introducing variables local to a function is
   supported, it is briefly documented in the [bash manual][bash functions].
2. Non-POSIX [parameter extensions][POSIX.1-2017 parameter expansion]
   are supported.  This includes: Substring expansions (e.g.
   `${var:offset:length}`) and Replacement expansions (e.g.
   `${var/pattern/string}`). The [bash manual][bash expansion]
   contains further information on these two expansions.

**NOTE:** `busybox ash` is currently used to evaluate APKBUILDs since it
supports additional POSIX shell extensions your APKBUILD might be
evaluated correctly even if it is not confirming to this policy
document.

# Shell Style Considerations

<!--
This should be in conformance with most existing APKBUILDs
Structure and content inspired by Google Shell Style Guide.
-->

## Formatting

### Indention

Indent with tabs, don't use spaces. Avoid whitespaces.

### Line Length

Maximum line length is 80 characters, this should be considered a
recommendation and not a strict requirement which must be followed at
all costs. Most notably, automatically generated parts (e.g. by `abuild
checksum`) are except from this rule.

### Compound Statements

Put `; do` and `; then` on the same line as the `while`, `for` or `if`.

### Function Declarations

* Always put the function name, the parenthesis and the curly brackets
  on the same line.
* Don't use spacing between function name and parenthesis.
* Do use spacing between function parenthesis and curly brackets.

### Case statement

* Don't indent alternatives.
* A one-line alternative needs a space after the close parenthesis of the pattern and before the `;;`.
* End the last case with `;;`.

### Variable expansion

* Use `${var}` over `$var` only when it is strictly necessary. Meaning:
  Only if the character following the [variable name][POSIX.1-2017 definition name]
  is an underscore or an alphanumeric character.

### Quoting

* Always quote string literals (exceptions are assigning `pkgname` and
  `pkgver`, more on this below).
* Always quote variables, command substitutions or shell meta characters
  when used in strings. Prefer `"$var"/foo/bar` over `"$var/foo/bar"`.
* Never quote literal integers.

## Features

### Command Substitution

* Always use `$()` instead of backticks.

### Test and [

* Prefer `[` over `test(1)`.

## Naming Conventions

### Function Names

* Lower-case, with underscores to separate words. Prefix all helper
  functions with an underscore character.

### Variable Names

* Lower-case, with underscores to separate words. Prefix all
  non-metadata variables with an underscore character.

### Use Local Variables

*  Declare function-specific variables with the `local` keyword.

## Calling Commands

### Command Substitutions and Global Variables

* Avoid command Substitutions in global variables, use parameter
  expansions instead.

### Return Values

* APKBUILDs are executed with `set -e`, explicit `|| return 1`
  statements must not be used.

## Comments

### Spacing

* All comments begin with an ASCII space character, e.g. `# foo`.

### TODO Comments

* Use TODO comments for code that is temporary, a short-term solution,
  or good-enough but not perfect.

# APKBUILD style considerations

<!--
This section attempts to document policies enforced by the linter from
atools <https://github.com/maxice8/atools>, newapkbuild and existing
APKBUILDs.
-->

## Comments

### Contributor Comment

* All APKBUILDs may begin with one or more contributor comments (one per
  line) containing a valid [RFC 5322][RFC 5322] address. For example,
  `# Contributor: Max Mustermann <max@example.org>`.

### Maintainer Comment

* All APKBUILDs contain exactly one maintainer comment containing a
  valid RFC 5322 address. For example, `# Maintainer: Max Mustermann
  <max@example.org>`.
* The Maintainer comment should immediately follow the Contributor comment(s).
* In the case of package being abandoned, the comment should still be present,
  but left empty: `# Maintainer:`.

## Metadata Variables

Metadata Variables are variables used directly by abuild itself, e.g. `pkgname` or `depends`.

### Metadata Values

* `pkgname` must only contain lowercase characters.
* `pkgname` must match the name of the directory the `APKBUILD` file is located in.

### Variable Assignments

* Empty Metadata Variable assignments, e.g. `install=""` must be removed.
* The Metadata Variable `builddir` defaults to `$srcdir/$pkgname-$pkgver`
  and should only be assigned if the default values is not appropriate.

### Assignment Order

* All Metadata Variables (except checksums) must be declared before the
  first function declaration.
* Checksum Metadata Variables must be declared after the last function
  declaration, `abuild checksum` will automatically take care of this.

## Build Functions

### Function Declaration

* Functions should be declared in the order they are called by abuild.
* All functions are executed in `"$builddir"` explicit directory changes
  to `$builddir` must be avoided (if possible).
* Variables local to functions must always be declared with the `local`
  keyword.

### Function Overwriting

* If the `prepare` function is overwritten it should always call
  `default_prepare`.

[POSIX.1-2017 volume 3]: https://pubs.opengroup.org/onlinepubs/9699919799/idx/xcu.html
[POSIX.1-2017 parameter expansion]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06_02
[POSIX.1-2017 definition name]: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_235
[bash functions]: https://www.gnu.org/software/bash/manual/bash.html#Shell-Functions
[bash expansion]: https://www.gnu.org/software/bash/manual/bash.html#Shell-Parameter-Expansion
[RFC 5322]: https://tools.ietf.org/html/rfc5322
