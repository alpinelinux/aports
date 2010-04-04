.PHONY: main testing non-free unstable

rootdir := $(shell pwd)

all: main testing non-free unstable

apkbuilds := $(shell find . -maxdepth 3 -name APKBUILD -print)

all-modules := $(sort $(subst ./,,$(patsubst %/,%,$(dir $(apkbuilds)))))

main-modules :=  $(sort							\
		   $(notdir						\
		     $(patsubst %/,%,					\
		       $(dir						\
		         $(shell find main -maxdepth 2 -name APKBUILD -print)))))

testing-modules :=  $(sort						\
		      $(notdir						\
		        $(patsubst %/,%,				\
		          $(dir						\
		            $(shell find testing -maxdepth 2 -name APKBUILD -print)))))

non-free-modules :=  $(sort						\
		       $(notdir						\
		         $(patsubst %/,%,				\
		           $(dir					\
		             $(shell find non-free -maxdepth 2 -name APKBUILD -print)))))

unstable-modules :=  $(sort						\
		       $(notdir						\
		         $(patsubst %/,%,				\
		           $(dir					\
		             $(shell find unstable -maxdepth 2 -name APKBUILD -print)))))

main: 
	for p in $(main-modules) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

testing: 
	for p in $(testing-modules) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

non-free: 
	for p in $(non-free-modules) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

unstable: 
	for p in $(unstable-modules) ; \
	do \
		cd $(rootdir)/$@/$$p; \
		abuild -r; \
	done

clean:
	for p in $(all-modules) ; do \
		cd $(rootdir)/$$p; \
		abuild clean; \
		abuild cleanpkg; \
	done

distclean:
	for p in $(all-modules) ; \
	do \
		cd $(rootdir)/$$p; \
		abuild clean; \
		abuild cleanoldpkg; \
		abuild cleanpkg; \
		abuild cleancache; \
	done

