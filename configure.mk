ifeq (,$(filter dist clean,$(MAKECMDGOALS)))
$(call PKG_CONFIG_CHK,uuid,,-luuid)
$(call PKG_CONFIG_CHK,fuse)
ASCIIDOC := $(call PATH_LOOKUP,asciidoc)
XSLTPROC := $(call PATH_LOOKUP,xsltproc)

__DOCBOOK_XSL := http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
ifneq (,$(shell $(XSLTPROC) --nonet --noout $(__DOCBOOK_XSL) && echo ok))
DOCBOOK_XSL := $(__DOCBOOK_XSL)
endif

endif
