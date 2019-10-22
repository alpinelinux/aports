# Alpine Linux coding style

Thank you for taking interest in contributing to our aports repository.
As consistency and readability are so important for the quality of our APKBUILD
and thus the quality of Alpine Linux, we kindly ask to follow these
recommendations.

## Language
Alpine Linux APKBUILD files are inherently just POSIX shell scripts. Please avoid
extensions, even if they work or are accepted by busybox ash. (using keyword
`local` is an exception)

## Naming convention
Use snake_case. Functions, variables etc. should be lower-cased with
underscores to separate words.

Local 'private' variables and functions in global scope should be prefixed
with a single underscore to avoid name collisions with internal variables in
`abuild`.

Double underscores are reserved and should not be used.
```sh
_my_variable="data"
```

### Bracing
Curly braces for functions should be on the same line.

```sh
prepare() {
	...
}
```

Markers to indicate a compound statement should be on the same line.


#### if ...; then
```sh
	if [ true ]; then
		echo "It is so"
	fi
}
```

#### while ...; do
```sh
	while ...; do
		...
	done
```

#### for ...; do
```sh
	for x in foo bar baz; do
		...
	done
```

### Spacing
All keywords and operators are separated by a space.

For cleanliness sake, ensure there is no trailing whitespace.

## Identation
Indentation is one tab character per level, alignment is done using spaces.
For example (using the >------- characters to indicate a tab):
```sh
build()
{
>-------make DESTDIR="${pkgdir}" \
>-------     PREFIX="/usr"
}
```

This ensures code is always neatly aligned and properly indented.

Space before tab should be avoided.

## Line length
A line should not be longer than 80 characters. While this is not a hard limit, it
is strongly recommended to avoid having longer lines, as long lines reduce
readability and invite deep nesting.

## Variables

### Quoting
Quote strings containing variables, command substitutions, spaces or shell meta
characters, unless careful unquoted expansion is required.

Don't quote _literal_ integers.

### Bracing
Only use curly braces around variables when needed.

```sh
foo="${foo}_bar"
foo_bar="$foo"
```

## Subshell usage
Use `$()` syntax, not backticks.

## Sorting
Some items tend to benefit from being sorted. A list of sources, dependencies
etc. Always try to find a reasonable sort order, where alphabetically tends to
be the most useful one.

## Eval
`eval` is evil and should be avoided.

