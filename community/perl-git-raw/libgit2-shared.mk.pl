use strict;
use warnings;
use ExtUtils::MakeMaker 0;
use ExtUtils::Constant qw (WriteConstants);

my %WriteMakefileArgs = (
  "ABSTRACT" => "Perl bindings to the Git linkable library (libgit2)",
  "AUTHOR" => "Jacques Germishuys <jacquesg\@cpan.org>, Alessandro Ghedini <alexbio\@cpan.org>",
  "CONFIGURE_REQUIRES" => {
    "ExtUtils::Constant" => "0.23",
    "ExtUtils::MakeMaker" => "6.63_03",
    "File::Basename" => "2.74",
    "Getopt::Long" => "2.35"
  },
  "DEFINE" => "-DGIT_SSH",
  "DISTNAME" => "Git-Raw",
  "LIBS" => "-lgit2",
  "LICENSE" => "perl",
  "MIN_PERL_VERSION" => "5.8.8",
  "NAME" => "Git::Raw",
  "OBJECT" => "\$(O_FILES)",
  "PREREQ_PM" => {
    "Carp" => 0,
    "XSLoader" => 0,
    "overload" => 0,
    "strict" => 0,
    "warnings" => 0
  },
  "TEST_REQUIRES" => {
    "Capture::Tiny" => 0,
    "Cwd" => 0,
    "File::Basename" => 0,
    "File::Copy" => 0,
    "File::Path" => 0,
    "File::Slurp::Tiny" => "0.001",
    "File::Spec" => 0,
    "File::Spec::Functions" => 0,
    "File::Spec::Unix" => 0,
    "IO::Handle" => 0,
    "IPC::Open3" => 0,
    "Test::Deep" => 0,
    "Test::More" => 0,
    "Time::Local" => 0
  },
  "VERSION_FROM" => "lib/Git/Raw.pm",
  "test" => {
    "TESTS" => "t/*.t"
  }
);

my @error_constants = (qw(
	OK
	ERROR
	ENOTFOUND
	EEXISTS
	EAMBIGUOUS
	EBUFS
	EBAREREPO
	EUNBORNBRANCH
	EUNMERGED
	ENONFASTFORWARD
	EINVALIDSPEC
	ECONFLICT
	ELOCKED
	EMODIFIED
	EAUTH
	ECERTIFICATE
	EAPPLIED
	EPEEL
	EEOF
	EINVALID
	EUNCOMMITTED
	EDIRECTORY
	EMERGECONFLICT
	PASSTHROUGH

	ASSERT
	USAGE
	RESOLVE
));

my @category_constants = (qw(
	NONE
	NOMEMORY
	OS
	INVALID
	REFERENCE
	ZLIB
	REPOSITORY
	CONFIG
	REGEX
	ODB
	INDEX
	OBJECT
	NET
	TAG
	TREE
	INDEXER
	SSL
	SUBMODULE
	THREAD
	STASH
	CHECKOUT
	FETCHHEAD
	MERGE
	SSH
	FILTER
	REVERT
	CALLBACK
	CHERRYPICK
	DESCRIBE
	REBASE
	FILESYSTEM

	INTERNAL
));

my @packbuilder_constants = (qw(
	ADDING_OBJECTS
	DELTAFICATION
));

my @stash_progress_constants = (qw(
	NONE
	LOADING_STASH
	ANALYZE_INDEX
	ANALYZE_MODIFIED
	ANALYZE_UNTRACKED
	CHECKOUT_UNTRACKED
	CHECKOUT_MODIFIED
	DONE
));

my @rebase_operation_constants = (qw(
	PICK
	REWORD
	EDIT
	SQUASH
	FIXUP
	EXEC
));

my @object_constants = (qw(
	ANY
	BAD
	COMMIT
	TREE
	BLOB
	TAG
));

ExtUtils::Constant::WriteConstants(
	NAME         => 'Git::Raw::Error',
	NAMES        => [@error_constants],
	DEFAULT_TYPE => 'IV',
	C_FILE       => 'const-c-error.inc',
	XS_FILE      => 'const-xs-error.inc',
	XS_SUBNAME   => '_constant',
	C_SUBNAME    => '_error_constant',
);

ExtUtils::Constant::WriteConstants(
	NAME         => 'Git::Raw::Error::Category',
	NAMES        => [@category_constants],
	DEFAULT_TYPE => 'IV',
	C_FILE       => 'const-c-category.inc',
	XS_FILE      => 'const-xs-category.inc',
	XS_SUBNAME   => '_constant',
	C_SUBNAME    => '_category_constant',
);

ExtUtils::Constant::WriteConstants(
	NAME         => 'Git::Raw::Packbuilder',
	NAMES        => [@packbuilder_constants],
	DEFAULT_TYPE => 'IV',
	C_FILE       => 'const-c-packbuilder.inc',
	XS_FILE      => 'const-xs-packbuilder.inc',
	XS_SUBNAME   => '_constant',
	C_SUBNAME    => '_packbuilder_constant',
);

ExtUtils::Constant::WriteConstants(
	NAME         => 'Git::Raw::Stash::Progress',
	NAMES        => [@stash_progress_constants],
	DEFAULT_TYPE => 'IV',
	C_FILE       => 'const-c-stash-progress.inc',
	XS_FILE      => 'const-xs-stash-progress.inc',
	XS_SUBNAME   => '_constant',
	C_SUBNAME    => '_stash_progress_constant',
);

ExtUtils::Constant::WriteConstants(
	NAME         => 'Git::Raw::Rebase::Operation',
	NAMES        => [@rebase_operation_constants],
	DEFAULT_TYPE => 'IV',
	C_FILE       => 'const-c-rebase-operation.inc',
	XS_FILE      => 'const-xs-rebase-operation.inc',
	XS_SUBNAME   => '_constant',
	C_SUBNAME    => '_rebase_operation_constant',
);

ExtUtils::Constant::WriteConstants(
	NAME         => 'Git::Raw::Object',
	NAMES        => [@object_constants],
	DEFAULT_TYPE => 'IV',
	C_FILE       => 'const-c-object.inc',
	XS_FILE      => 'const-xs-object.inc',
	XS_SUBNAME   => '_constant',
	C_SUBNAME    => '_object_constant',
);

WriteMakefile(%WriteMakefileArgs);
exit(0);
