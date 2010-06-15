
repos	:= repo1 repo2

tmproot := /tmp/apk-test
testroot = $(tmproot)/$(basename $@)

SUDO	:= sudo
APK	:= ../src/apk --keys-dir /etc/apk/keys
#SYSREPO ?= http://alpinelinux.org/cgi-bin/dl.cgi/edge/main
SYSREPO ?= http://192.168.8.4/main

LD_LIBRARY_PATH = ../src
export LD_LIBRARY_PATH SYSREPO

all:	tests

$(repos):
	@echo "Building $@"
	@rm -f repos.out
	@cd $@ &&\
		for i in */APKBUILD; do\
			( cd $${i%/*} && \
			PKGDEST="$$PWD/.." abuild -R ) >> ../repos.out 2>&1 || exit 1;\
		done
	@cd $@ &&\
		( apk index -o APKINDEX.tar.gz *.apk &&\
		abuild-sign APKINDEX.tar.gz ) >> ../repos.out 2>&1


clean:
	$(SUDO) rm -rf $(addsuffix /*.apk,$(repos)) $(tmproot) *.ok *.out *.stamp


repos: repos.stamp
	
repos.stamp: $(repos)
	@touch $@
	

%.ok: %.sh
	@echo -n "$<"; \
		rm -rf "$(testroot)" ;\
		mkdir -p "$(testroot)"; \
		if ! APK="$(APK)" ROOT="$(testroot)" SYSREPO="$(SYSREPO)" sh -e $< > $(basename $@).out 2>&1; then\
			echo " FAIL";\
			exit 1;\
		fi ;\
		echo " OK" ;\
		$(SUDO) rm -rf $(testroot); \
		touch $@

tests: repos.stamp
	@echo "== Testing `$(APK) --version` =="
	@for i in test*.sh; do \
		rm -f $${i%.sh}.ok ;\
		$(SUDO) $(MAKE) --no-print-directory $${i%.sh}.ok SYSREPO=$(SYSREPO); \
	done

.PHONY:	$(repos)
