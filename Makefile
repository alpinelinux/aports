.PHONY: main testing non-free unstable

rootdir := $(shell pwd)

all: main testing non-free unstable

apkbuilds := $(shell find . -maxdepth 3 -name APKBUILD -print)

all-pkgs := $(sort $(subst ./,,$(patsubst %/,%,$(dir $(apkbuilds)))))

main-pkgs :=  $(shell ./aport.lua deplist $(rootdir) main)

testing-pkgs :=  $(shell ./aport.lua deplist $(rootdir) testing)

non-free-pkgs :=  $(shell ./aport.lua deplist $(rootdir) non-free)

unstable-pkgs :=  $(shell ./aport.lua deplist $(rootdir) unstable)

main:
	for p in $(main-pkgs) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

testing:
	for p in $(testing-pkgs) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

non-free:
	for p in $(non-free-pkgs) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

unstable:
	for p in $(unstable-pkgs) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

clean:
	for p in $(all-pkgs) ; do \
		cd $(rootdir)/$$p; \
		abuild clean; \
		abuild cleanpkg; \
	done

cleanold:
	for p in $(all-pkgs) ; do \
		cd $(rootdir)/$$p; \
		abuild cleanoldpkg; \
	done

fetch:
	for p in $(all-pkgs) ; do \
		cd $(rootdir)/$$p; \
		abuild fetch; \
	done

distclean:
	for p in $(all-pkgs) ; \
	do \
		cd $(rootdir)/$$p; \
		abuild clean; \
		abuild cleanoldpkg; \
		abuild cleanpkg; \
		abuild cleancache; \
	done

