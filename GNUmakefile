PACKAGE := vmfs-tools

CC := gcc
OPTIMFLAGS := -O2
CFLAGS := -Wall $(OPTIMFLAGS) -g -D_FILE_OFFSET_BITS=64 $(EXTRA_CFLAGS)
LDFLAGS := $(shell pkg-config --libs uuid)
SRC := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := $(SRC:%.c=%.o)
PROGRAMS := debugvmfs vmfs-fuse
MANSRCS := $(wildcard $(PROGRAMS:%=%.txt))
MANDOCBOOK := $(MANSRCS:%.txt=%.xml)
MANPAGES := $(foreach man,$(MANSRCS),$(shell sed '1{s/(/./;s/)//;q}' $(man)))
EXTRA_DIST := LICENSE README TODO AUTHORS
LIB := libvmfs.a

prefix := /usr/local
exec_prefix := $(prefix)
sbindir := $(exec_prefix)/sbin
datarootdir := $(prefix)/share
mandir := $(datarootdir)/man

all: $(PROGRAMS) $(wildcard .gitignore)

version: $(MAKEFILE_LIST) $(SRC) $(HEADERS) $(wildcard .git/logs/HEAD .git/refs/tags)
	(if [ -d .git ]; then \
		VER=$$(git describe --match "v[0-9].*" --abbrev=0 HEAD); \
		git update-index -q --refresh; \
		if git diff-index --name-status $${VER} -- | grep -q -v '^A'; then \
			VER=$$(git describe --match "v[0-9].*" --abbrev=4 HEAD | sed s/-/./g); \
		fi ; \
		if [ -z "$${VER}" ]; then \
			VER="v0.0.0.$$(git rev-list HEAD | wc -l).$$(git rev-parse --short=4 HEAD)"; \
		fi; \
		test -z "$$(git diff-index --name-only HEAD --)" || VER=$${VER}-dirty; \
        else \
		VER=$(patsubst %,%-patched,$(or $(VERSION:%-patched=%),v0.0.0)); \
	fi; \
	echo VERSION := $${VER}) > $@ 2> /dev/null
-include version

vmfs-fuse: LDFLAGS+=$(shell pkg-config --libs fuse)
vmfs-fuse.o: CFLAGS+=$(shell pkg-config --cflags fuse)

debugvmfs_EXTRA_SRCS := readcmd.c
debugvmfs: LDFLAGS+=-ldl
debugvmfs.o: CFLAGS+=-DVERSION=\"$(VERSION)\"

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

$(PROGRAMS):
	$(CC) -o $@ $^ $(LDFLAGS)

clean: CLEAN := $(wildcard $(LIB) $(PROGRAMS) $(OBJS) $(PACKAGE)-*.tar.gz $(MANPAGES) $(MANDOCBOOK))
clean:
	$(if $(CLEAN),rm $(CLEAN))

ALL_DIST := $(SRC) $(HEADERS) $(MAKEFILE_LIST) $(MANSRCS) $(EXTRA_DIST)
DIST_DIR := $(PACKAGE)-$(VERSION:v%=%)
dist: $(ALL_DIST)
	@rm -rf "$(DIST_DIR)"
	@mkdir "$(DIST_DIR)"
	cp -p $(ALL_DIST) $(DIST_DIR)
	tar -zcf "$(DIST_DIR).tar.gz" "$(DIST_DIR)"
	@rm -rf "$(DIST_DIR)"

$(MANSRCS:%.txt=%.xml): %.xml: %.txt
	asciidoc -a manversion=$(VERSION:v%=%) -a manmanual=$(PACKAGE) -b docbook -d manpage -o $@ $<

$(MANPAGES): %.8: %.xml
	xsltproc -o $@ --nonet --novalid http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

doc: $(MANPAGES)

$(DESTDIR)/%:
	install -d -m 0755 $@

installPROGRAMS := $(PROGRAMS:%=$(DESTDIR)$(sbindir)/%)
installMANPAGES := $(MANPAGES:%=$(DESTDIR)$(mandir)/man8/%)

$(installPROGRAMS): $(DESTDIR)$(sbindir)/%: % $(DESTDIR)$(sbindir)
	install $(if $(NO_STRIP),,-s )-m 0755 $< $(dir $@)

$(installMANPAGES): $(DESTDIR)$(mandir)/man8/%: % $(DESTDIR)$(mandir)/man8
	install -m 0755 $< $(dir $@)

install: $(installPROGRAMS) $(installMANPAGES)

.PHONY: all clean dist install doc

.gitignore: $(MAKEFILE_LIST)
	(echo "*.tar.gz"; \
	 echo "*.[ao]"; \
	 echo "*.xml"; \
	 echo "*.8"; \
	 echo "version"; \
	 $(foreach program, $(PROGRAMS),echo $(program);) \
	) > $@
