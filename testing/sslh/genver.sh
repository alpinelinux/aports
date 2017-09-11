#! /bin/sh

if [ ${#} -eq 1 ] && [ "x$1" = "x-r" ]; then
	# release text only
	QUIET=1
else
	QUIET=0
fi

        # If we don't have git, we can't work out what
        # version this is. It must have been downloaded as a
        # zip file. 
        
        # If downloaded from the release page, the directory
        # has the version number.
        release=`pwd | sed s/.*sslh-// | grep "[[:digit:]]"`
        
        if [ "x$release" = "x" ]; then
            # If downloaded from the head, Github creates the
            # zip file with all files dated from the last
            # change: use the Makefile's modification time as a
            # release number
            release=head-`perl -MPOSIX -e 'print strftime "%Y-%m-%d",localtime((stat "Makefile")[9])'`
        fi

if [ $QUIET -ne 1 ]; then
	printf "#ifndef VERSION_H \n"
	printf "#define VERSION_H \n\n"
	printf "#define VERSION \"$release\"\n"
	printf "#endif\n"
else
	printf "$release\n"
fi
