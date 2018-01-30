#!/bin/sh

echo "Regenerating /usr/share/emacs/site-lisp/site-start.el"
echo "" > /usr/share/emacs/site-lisp/site-start.el
for f in $(ls /usr/share/emacs/site-lisp/*-init-*.el | sort); do
	f=$(basename $f)
	echo "Adding $f"
	echo "(load \"/usr/share/emacs/site-lisp/$f\" nil t)" >> /usr/share/emacs/site-lisp/site-start.el || exit 1
done
exit 0
