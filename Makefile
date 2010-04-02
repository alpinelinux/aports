.PHONY: main testing non-free unstable

rootdir := $(shell pwd)

all: main testing non-free unstable

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
	for p in $(main-modules) ; \
	do \
		cd $(rootdir)/main/$$p; \
		abuild clean; \
		abuild cleanoldpkg; \
		abuild cleanpkg; \
		abuild cleancache; \
	done
	for p in $(testing-modules) ; \
	do \
		cd $(rootdir)/testing/$$p; \
		abuild clean; \
		abuild cleanoldpkg; \
		abuild cleanpkg; \
		abuild cleancache; \
	done
	for p in $(non-free-modules) ; \
	do \
		cd $(rootdir)/non-free/$$p; \
		abuild clean; \
		abuild cleanoldpkg; \
		abuild cleanpkg; \
		abuild cleancache; \
	done
	for p in $(unstable-modules) ; \
	do \
		cd $(rootdir)/unstable/$$p; \
		abuild clean; \
		abuild cleanoldpkg; \
		abuild cleanpkg; \
		abuild cleancache; \
	done

