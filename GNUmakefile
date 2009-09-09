include utils.mk
include configure.mk

PACKAGE := vmfs-tools

CC := gcc
OPTIMFLAGS := -O2
CFLAGS := -Wall $(OPTIMFLAGS) -g -D_FILE_OFFSET_BITS=64 $(EXTRA_CFLAGS)
CFLAGS += $(UUID_CFLAGS)
LDFLAGS := $(UUID_LDFLAGS)
SRC := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := $(SRC:%.c=%.o)
PROGRAMS := debugvmfs vmfs_fsck vmfs-fuse
buildPROGRAMS := $(PROGRAMS)
ifeq (,$(FUSE_LDFLAGS))
ifneq (clean,$(MAKECMDGOALS))
buildPROGRAMS := $(filter-out vmfs-fuse,$(buildPROGRAMS))
endif
endif
MANSRCS := $(wildcard $(buildPROGRAMS:%=%.txt))
MANDOCBOOK := $(MANSRCS:%.txt=%.xml)
MANPAGES := $(foreach man,$(MANSRCS),$(shell sed '1{s/(/./;s/)//;q;}' $(man)))

EXTRA_DIST := LICENSE README TODO AUTHORS
LIB := libvmfs.a

prefix := /usr/local
exec_prefix := $(prefix)
sbindir := $(exec_prefix)/sbin
datarootdir := $(prefix)/share
mandir := $(datarootdir)/man

all: $(buildPROGRAMS) $(wildcard .gitignore)

ALL_MAKEFILES = $(filter-out config.cache,$(MAKEFILE_LIST))

ifneq (clean,$(MAKECMDGOALS))
version: $(ALL_MAKEFILES) $(SRC) $(HEADERS) $(wildcard .git/logs/HEAD .git/refs/tags)
	echo VERSION := $(GEN_VERSION) > $@
-include version
endif

vmfs-fuse: LDFLAGS+=$(FUSE_LDFLAGS)
vmfs-fuse.o: CFLAGS+=$(FUSE_CFLAGS)

debugvmfs_EXTRA_SRCS := readcmd.c
debugvmfs: LDFLAGS+= $(DLOPEN_LDFLAGS)
debugvmfs.o: CFLAGS+=-DVERSION=\"$(VERSION)\"

vmfs_fsck.o: CFLAGS+=-DVERSION=\"$(VERSION)\"

define program_template
$(strip $(1))_EXTRA_OBJS := $$($(strip $(1))_EXTRA_SRCS:%.c=%.o)
LIBVMFS_EXCLUDE_OBJS += $(1).o $$($(strip $(1))_EXTRA_OBJS)
$(1): $(1).o $$($(strip $(1))_EXTRA_OBJS) $(LIB)
endef
$(foreach program, $(PROGRAMS), $(eval $(call program_template,$(program))))

$(LIB): $(filter-out $(LIBVMFS_EXCLUDE_OBJS),$(OBJS))
	ar -r $@ $^
	ranlib $@

$(OBJS): %.o: %.c $(HEADERS)

$(buildPROGRAMS):
	$(CC) -o $@ $^ $(LDFLAGS)

clean: CLEAN := $(wildcard $(LIB) $(PROGRAMS) $(OBJS) $(PACKAGE)-*.tar.gz $(MANPAGES) $(MANDOCBOOK))
clean:
	$(if $(CLEAN),rm $(CLEAN))

ALL_DIST := $(SRC) $(HEADERS) $(ALL_MAKEFILES) $(MANSRCS) $(EXTRA_DIST)
DIST_DIR := $(PACKAGE)-$(VERSION:v%=%)
dist: $(ALL_DIST)
	@rm -rf "$(DIST_DIR)"
	@mkdir "$(DIST_DIR)"
	cp -p $(ALL_DIST) $(DIST_DIR)
	tar -zcf "$(DIST_DIR).tar.gz" "$(DIST_DIR)"
	@rm -rf "$(DIST_DIR)"

ifneq (,$(ASCIIDOC))
ifneq (,$(XSLTPROC))
ifneq (,$(DOCBOOK_XSL))
$(MANDOCBOOK): %.xml: %.txt
	$(ASCIIDOC) -a manversion=$(VERSION:v%=%) -a manmanual=$(PACKAGE) -b docbook -d manpage -o $@ $<

$(MANPAGES): %.8: %.xml
	$(XSLTPROC) -o $@ --nonet --novalid $(DOCBOOK_XSL) $<

doc: $(MANPAGES)
endif
endif
endif

$(DESTDIR)/%:
	install -d -m 0755 $@

installPROGRAMS := $(buildPROGRAMS:%=$(DESTDIR)$(sbindir)/%)
installMANPAGES := $(MANPAGES:%=$(DESTDIR)$(mandir)/man8/%)

$(installPROGRAMS): $(DESTDIR)$(sbindir)/%: % $(DESTDIR)$(sbindir)
	install $(if $(NO_STRIP),,-s )-m 0755 $< $(dir $@)

$(installMANPAGES): $(DESTDIR)$(mandir)/man8/%: % $(DESTDIR)$(mandir)/man8
	install -m 0755 $< $(dir $@)

install: $(installPROGRAMS) $(installMANPAGES)

.PHONY: all clean dist install doc

.gitignore: $(ALL_MAKEFILES)
	(echo "*.tar.gz"; \
	 echo "*.[ao]"; \
	 echo "*.xml"; \
	 echo "*.8"; \
	 echo "version"; \
	 echo "config.cache"; \
	 $(foreach program, $(PROGRAMS),echo $(program);) \
	) > $@
