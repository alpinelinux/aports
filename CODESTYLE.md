# Alpine Linux coding style

Thank you for taking interest in contributing to our aports repository.
As consistency and readability are so important for the quality of our APKBUILD
and thus the quality of Alpine Linux, we kindly ask to follow these rules.

## Language
Alpine Linux APKBUILD files are inherently just shell scripts. As such, they
should pass the shellcheck linter.

## Naming convention
APKBUILD files uses snake_case. For example functions, variable names etc. are
all lower-cased with using an underscore as a separator/space. Local 'private'
variables and functions are pre-fixed with a single underscore. Double
underscores are reserved and should not be used.
```sh
_my_variable="data"
```

### Bracing
Curly braces for functions are on the next line, to indicate a function.

Markers to indicate a compound statement, are on the same line.

```sh
prepare()
{
	if [ true ]; then
		echo "It is so"
	fi
}
```

### Spacing
All keywords and operators are separated by a space.

For cleanliness sake, make sure there is however never any trailing whitespace.

## Identation
Indentation is one tab character, alignment is done using spaces. For example
using the >------- characters to indicate a tab:
```sh
prepare()
{
>-------make DESTDIR="${pkgdir}" \
>-------     PREFIX="/usr"
}
```

This ensures code is always neatly aligned and properly indented.

## Line length
A line should not be longer then 80 lines. While this is not a hard limit, it
is strongly recommended to avoid having longer lines, as long lines reduce
readability and invite deep nesting.

## Variables
### Quoting
Always quote strings. As in shell there is no concept of data types (other then
strings). There is no such thing as characters, integers or booleans. All data
should thus be quoted.
```sh
pkgver="0"
```

### Bracing
Always use curly braces around variables. While shells do not require this,
being consistent in variable usage helps. Also there is no need to try to
determine what the actual variable is and if it may not end up being empty.

```sh
foo_bar="foobar"
foo="bar"
```
In the above, it could be argued it is immediately visible that ***$foo_bar***
is the string 'foobar'. However if the variable is defined var away from the
invocation, this is not clear any longer. To avoid these amigo ties, always
use curly braces.
```sh
foo="${foo}_bar"
foo_bar="${foo}"
```

## Subshell usage
Always use `$()` syntax, not backticks, as backticks are hard to spot.

## Sorting
Some items tend to benefit from being sorted. A list of sources, dependencies
etc. Always try to find a reasonable sort order, where alphabetically tends to
be the most useful one.
