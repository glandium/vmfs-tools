include utils.mk

__VARS := $(.VARIABLES)

# configure rules really start here
$(call PKG_CONFIG_CHK,uuid,-I/usr/include/uuid,-luuid)
$(call PKG_CONFIG_CHK,fuse)
$(call PATH_LOOKUP,asciidoc)
$(call PATH_LOOKUP,xsltproc)

$(call checking,docbook.xsl)
__DOCBOOK_XSL := http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
ifneq (,$(XSLTPROC))
ifneq (,$(shell $(XSLTPROC) --nonet --noout $(__DOCBOOK_XSL) 2> /dev/null && echo ok))
DOCBOOK_XSL := $(__DOCBOOK_XSL)
endif
endif
$(call result,$(DOCBOOK_XSL))
$(call LINK_CHECK,strndup)
$(call LINK_CHECK,dlopen,-ldl)
ifeq (,$(HAS_DLOPEN))
$(call LINK_CHECK,dlopen)
endif
$(call LINK_CHECK,posix_memalign)

# Generate cache file
$(shell ($(foreach var,$(filter-out $(__VARS) __%,$(.VARIABLES)),echo $(var) := $($(var));)) > config.cache)

configure:
