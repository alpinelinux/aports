##
## Utilities for safely handling files and directories.
##
##


##
## General purpose filename/dirname tools
##

# Check if filename glob matches against string. (Based on example at Rich's sh tricks)
# Usage: fnmatch <glob> <string>
fnmatch() { case "$2" in $1 ) return 0 ;; * ) return 1 ;; esac ; }

# Prints the parent directory by chopping off the last '/*?' ( '/*' will match a lone trailing '/' )
# Usage: dirname_parent <directory name>
dirname_parent() { [ "$1" = "/" ] && return 0 || printf '%s' "${1%/*?}" || return 1 ; }



##
## File tools
## 


## File Exists/Read/Write/Execute full tests:

# Check that the argument exists, and is a file.
# Usage: file_exists <file>
file_exists() { [ -e "$1" ] && [ -f "$1" ] && return 0 || return 1 ; }


# Check that the argument exists, is a file, and is readable [and writeable | and executable].
# Usage: 'file_is_(readable|writeable|executable) <file>
file_is_readable() { [ -e "$1" ] && [ -f "$1" ] && [ -r "$1" ] && return 0 || return 1 ; }
file_is_writable() { [ -e "$1" ] && [ -f "$1" ] && [ -r "$1" ] && [ -w "$1" ] && return 0 || return 1 ; }
file_is_executable() { [ -e "$1" ] && [ -f "$1" ] && [ -r "$1" ] && [ -x "$1" ] && return 0 || return 1 ; }


# Verify file is readable, then cat input file to wc with the appropriate flag.
# Usage: file_count_(bytes|characters|words|lines) <file>
file_count_bytes() { file_is_readable "$1" && cat "$1" | wc -c || return 1 ; }
file_count_characters() { file_is_readable "$1" && cat "$1" | wc -m || return 1 ; }
file_count_words() { file_is_readable "$1" && cat "$1" | wc -w || return 1 ; }
file_count_lines() { file_is_readable "$1" && cat "$1" | wc -l || return 1 ; }


# Get the size of a file in kilobytes, megabytes, or human readable format (##.##[KMG])
# Usage: file_size_(kilobytes|megabytes|human_readable) <file>
file_size_kilobytes() { file_is_readable "$1" && du -sLk "$1" | cut -f1 || return 1 ; }
file_size_megabytes() { file_is_readable "$1" && du -sLm "$1" | cut -f1 || return 1 ; }
file_size_human_readable() { file_is_readable "$1" && du -sLh "$1" | cut -f1 || return 1 ; }



##
## Directory tools
##


## Directory Read(+Execute)/Write full tests:

# Check that the argument exists, is a directory, is readable and is searchable (executable) [ and writeable ].
# Usage: 'dir_is_(readable|writeable) <directory>
dir_is_readable() { [ -e "$1" ] && [ -d "$1" ] && [ -r "$1" ] && [ -x "$1" ] && return 0 || return 1 ; }
dir_is_writable() { [ -e "$1" ] && [ -d "$1" ] && [ -r "$1" ] && [ -w "$1" ] && [ -x "$1" ] && return 0 || return 1 ; }


## Directory info

# Print count of entrys in directory, ignoring . and ..
# Usage: dir_count <directory>
dir_count() { [ -d "$1" ] && ls -A1 "$1" | wc -l || return 1 ; }

# Return true if directory is [not] empty, false otherwise.
# Usage: dir_(is_empty|is_not_empty) <directory> 
dir_is_empty() { [ $(dir_count) -eq 0 ] && return 0 || return 1 ; }
dir_not_empty() { [ $(dir_count) -eq 0 ] && return 1 || return 0 ; }


# Get the total size of a directory in kilobytes, megabytes, or human readable format (##.##[KMG])
# Usage: dir_size_(kilobytes|megabytes|human_readable) <file>
dir_size_kilobytes() { dir_is_readable "$1" && du -sk "$1" | cut -f1 || return 1 ; }
dir_size_megabytes() { dir_is_readable "$1" && du -sm "$1" | cut -f1 || return 1 ; }
dir_size_human_readable() { dir_is_readable "$1" && du -sh "$1" | cut -f1 || return 1 ; }


# Print full path to users home directory.
# Usage: dir_home_user <user>
dir_user_home() {
	# Make sure we have something resembling a user name before we eval it.
	[ "$1" ] && [ "$(printf '%s' "$1" | sed -E -e 's/^[_[:alnum:]][-_[:alnum:]]*\$?$//')" = "" ] && [ $(echo "$1" | wc -c) -le 32 ] || ! warning "dir_user_home: Bad username passed: '$1'" || return 1
	eval "realpath ~$(echo "$1" | sed -e 's/./\\&/g')/"
}

## Directory creation

# Make a directory (and parents if needed), checking that the result is a writable directory.
# Usage: mkdir_is_writeable <directory name>
mkdir_is_writable() { mkdir -p "$1" && dir_is_writable "$1" && return 0 || return 1 ; }


## Directory copying

# Use cpio to make a full reproduction of the contents of one directory into another.
# Usage: dir_copy_contents <source dir> <dest dir>
dir_copy_contents_cpio() {
	dir_is_readable $1 || ! warning "dir_copy_contents: Can not change to source directory: '$1'" || return 1
	mkdir_is_writable "$2" && ( cd "$1" && find -depth -print0 | cpio --null -pmd "$2" )
}

# Make a copy of contents of one directory into another.
# Usage: dir_copy_symlink_contents() <source dir> <dest dir>
dir_copy_contents() {
	dir_is_readable $1 || ! warning "dir_copy_contents: Can not change to source directory: '$1'" || return 1
	mkdir_is_writeable "$2" && \
		( cd "$1" && find -maxdepth 1 ! -name . -depth -xdev -exec cp -a "{}" "$2/"  \; )
}

# Make hard-linked copy of contents of one directory into another where possible, normal copy if not.
# Since cp -al doesn't work across filesystems, try it first, then fall back on cp -a if it fails.
# Usage: dir_copy_link_contents() <source dir> <dest dir>
dir_copy_link_contents() {
	dir_is_readable $1 || ! warning "dir_copy_contents: Can not change to source directory: '$1'" || return 1
	mkdir_is_writeable "$2" && \
		( cd "$1" && find -maxdepth 1 ! -name . -depth -xdev \
			! -exec cp -al "{}" "$2/" 2> /dev/null \; -exec cp -a "{}" "$2/"  \; )
}


# Make sym-linked copy of contents of one directory into another.
# Usage: dir_copy_symlink_contents() <source dir> <dest dir>
dir_copy_symlink_contents() {
	dir_is_readable $1 || ! warning "dir_copy_contents: Can not change to source directory: '$1'" || return 1
	mkdir_is_writeable "$2" && \
		( cd "$1" && find -maxdepth 1 ! -name . -depth -xdev -exec cp -as "{}" "$2/"  \; )
}


