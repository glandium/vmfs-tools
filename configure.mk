# Do dependency by hand so that we can use tricks to get the variables
# set from the configure rules.
ifeq (,$(filter dist clean,$(MAKECMDGOALS)))
ifneq (,$(shell test config.cache -nt configure.mk || echo no))
__VARS := $(.VARIABLES)

# configure rules really start here
$(call PKG_CONFIG_CHK,uuid,,-luuid)
$(call PKG_CONFIG_CHK,fuse)
$(call PATH_LOOKUP,asciidoc)
$(call PATH_LOOKUP,xsltproc)

$(call checking,docbook.xsl)
__DOCBOOK_XSL := http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
ifneq (,$(shell $(XSLTPROC) --nonet --noout $(__DOCBOOK_XSL) 2> /dev/null && echo ok))
DOCBOOK_XSL := $(__DOCBOOK_XSL)
endif
$(call result,$(DOCBOOK_XSL))

# Generate cache file
$(shell ($(foreach var,$(filter-out $(__VARS) __%,$(.VARIABLES)),echo $(var) := $($(var));)) > config.cache)
else
include config.cache
endif
endif
