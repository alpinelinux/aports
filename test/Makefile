-include conf.mk

repos	:= repo1 repo2

tmproot := /tmp/apk-test
testroot = $(tmproot)/$(basename $@)

SUDO	:= sudo
APK	:= ../src/apk --keys-dir /etc/apk/keys

SYSREPO ?= http://nl.alpinelinux.org/alpine/edge/main

LD_LIBRARY_PATH = ../src
export LD_LIBRARY_PATH SYSREPO

all: tests

$(repos):
	@echo "Building $@"
	@rm -f repos.out
	@cd $@ &&\
		for i in */APKBUILD; do \
			( cd $${i%/*} && REPODEST="$$PWD/../.." abuild -R ) \
				>> ../repos.out 2>&1 || { cat ../repos.out >&2; exit 1; } \
		done


clean:
	$(SUDO) rm -rf $(addsuffix /*.apk,$(repos)) $(tmproot) *.ok *.out *.stamp


repos: repos.stamp
	
repos.stamp: $(repos)
	@touch $@
	

%.ok: %.sh
	@echo -n "$<"; \
		rm -rf "$(testroot)" ;\
		mkdir -p "$(testroot)"; \
		if ! APK="$(APK)" ROOT="$(testroot)" SYSREPO="$(SYSREPO)" ./$< > $(basename $@).out 2>&1; then\
			echo " FAIL";\
			printf "------------- output -------------\n" >&2;\
			cat $(basename $@).out >&2;\
			printf "----------------------------------\n" >&2;\
			exit 1;\
		fi ;\
		echo " OK" ;\
		$(SUDO) rm -rf $(testroot); \
		touch $@

root-tests: repos.stamp
	@echo "== Testing `$(APK) --version` (tests that require root permission) =="
	@failed=0; for i in test*.sh; do \
		rm -f $${i%.sh}.ok ;\
		$(SUDO) $(MAKE) --no-print-directory $${i%.sh}.ok SYSREPO=$(SYSREPO) || failed=1; \
	done; \
	return $$failed

tests:
	@echo "== Testing `$(APK) --version` =="
	@for i in $(sort $(filter-out test%.sh,$(wildcard *.sh))); do \
		./$$i || exit 1 ; \
	done

.PHONY:	$(repos) tests
