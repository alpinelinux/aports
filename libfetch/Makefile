libs-y			+= libfetch.a
libfetch.a-objs		:= common.o fetch.o file.o ftp.o http.o
generate-y		+= ftperr.h httperr.h

quiet_cmd_generr = GENERR  $@
      cmd_generr = $(obj)/errlist.sh $(basename $(<F))_errlist $(shell echo $(basename $(<F)) | tr a-z A-Z) $< > $@

$(obj)/%err.h: $(obj)/%.errors
	@$(call echo-cmd,generr) $(cmd_generr); $(cmd_generr)
