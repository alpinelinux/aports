libs-y			+= libfetch.a
libfetch.a-objs		:= common.o fetch.o file.o ftp.o http.o openssl-compat.o
generate-y		+= ftperr.h httperr.h
CFLAGS_common.o		+= -DCA_CERT_FILE=\"$(CONFDIR)/ca.pem\" -DCA_CRL_FILE=\"$(CONFDIR)/crl.pem\"
CFLAGS_common.o		+= -DCLIENT_CERT_FILE=\"$(CONFDIR)/cert.pem\" -DCLIENT_KEY_FILE=\"$(CONFDIR)/cert.key\"

quiet_cmd_generr = GENERR  $@
      cmd_generr = $(obj)/errlist.sh $(basename $(<F))_errlist $(shell echo $(basename $(<F)) | tr a-z A-Z) $< > $@

$(obj)/%err.h: $(obj)/%.errors
	@$(call echo-cmd,generr) $(cmd_generr); $(cmd_generr)
