ifneq (clean,$(MAKECMDGOALS))
-include version
endif
-include config.cache
include utils.mk

PACKAGE := vmfs-tools

all:

SUBDIRS := $(subst /,,$(dir $(wildcard */manifest.mk)))

define subdir_template
__VARS := $$(.VARIABLES)
include $(1)/manifest.mk
$$(foreach var,$$(filter-out $$(__VARS) __%,$$(.VARIABLES)), $$(eval $(1)/$$(var) := $$($$(var))))
$(1)/SRC := $(wildcard $(1)/*.c)
$(1)/HEADERS := $(wildcard $(1)/*.h)
$(1)/OBJS := $$($(1)/SRC:%.c=%.o)
$(1)/PROGRAM := $(1)/$(1)
$(1)/$(1): $$($(1)/OBJS) libvmfs.a
$(1)/$(1): LDFLAGS += $$($(1)/$(1)_LDFLAGS)
$(1)/MANSRC := $(wildcard $(1)/$(1).txt)

$$(foreach obj,$$($(1)/OBJS), $$(eval $$(obj): CFLAGS += -I. $$($$(obj)_CFLAGS)))
endef
$(foreach subdir,$(SUBDIRS), $(eval $(call subdir_template,$(subdir))))

CC := gcc
OPTIMFLAGS := $(if $(filter -O%,$(CFLAGS)),,-O2)
ENV_CFLAGS := $(CFLAGS)
ENV_LDFLAGS := $(LDFLAGS)
CFLAGS := $(ENV_CFLAGS) $(filter-out $(ENV_CFLAGS),-Wall $(OPTIMFLAGS) -g -D_FILE_OFFSET_BITS=64 $(EXTRA_CFLAGS))
CFLAGS += $(UUID_CFLAGS) $(if $(HAS_STRNDUP),,-DNO_STRNDUP=1)
LDFLAGS := $(ENV_LDFLAGS) $(filter-out $(ENV_LDFLAGS),$(UUID_LDFLAGS) $(EXTRA_LDFLAGS))
SRC := $(wildcard *.c) $(foreach subdir,$(SUBDIRS),$($(subdir)/SRC))
HEADERS := $(wildcard *.h) $(foreach subdir,$(SUBDIRS),$($(subdir)/HEADERS))
OBJS := $(SRC:%.c=%.o)
PROGRAMS := debugvmfs vmfs-fuse imager
buildPROGRAMS := $(PROGRAMS)
ifeq (,$(FUSE_LDFLAGS))
ifneq (clean,$(MAKECMDGOALS))
buildPROGRAMS := $(filter-out vmfs-fuse,$(buildPROGRAMS))
endif
endif
BUILD_PROGRAMS := $(foreach subdir,$(SUBDIRS),$($(subdir)/PROGRAM))
MANSRCS := $(wildcard $(buildPROGRAMS:%=%.txt)) $(foreach subdir,$(SUBDIRS),$($(subdir)/MANSRC))
MANDOCBOOK := $(MANSRCS:%.txt=%.xml)
MANPAGES := $(foreach man,$(MANSRCS),$(man:%.txt=%.$(shell sed '1{s/^.*(//;s/)//;q;}' $(man))))

EXTRA_DIST := LICENSE README TODO AUTHORS test.img configure
LIB := libvmfs.a

all: $(buildPROGRAMS) $(BUILD_PROGRAMS) $(wildcard .gitignore) test.img

ALL_MAKEFILES = $(filter-out config.cache,$(MAKEFILE_LIST)) configure.mk

version: $(filter-out version, $(ALL_MAKEFILES)) $(SRC) $(HEADERS) $(wildcard .git/logs/HEAD .git/refs/tags)
	echo VERSION := $(GEN_VERSION) > $@

vmfs-fuse: LDFLAGS+=$(FUSE_LDFLAGS)
vmfs-fuse.o: CFLAGS+=$(FUSE_CFLAGS)

debugvmfs_EXTRA_SRCS := readcmd.c
debugvmfs: LDFLAGS+= $(DLOPEN_LDFLAGS)
debugvmfs.o: CFLAGS+=-DVERSION=\"$(VERSION)\"

utils.o: CFLAGS += $(if $(HAS_POSIX_MEMALIGN),,-DNO_POSIX_MEMALIGN=1)

define program_template
$(strip $(1))_EXTRA_OBJS := $$($(strip $(1))_EXTRA_SRCS:%.c=%.o)
LIBVMFS_EXCLUDE_OBJS += $(1).o $$($(strip $(1))_EXTRA_OBJS)
$(1): $(1).o $$($(strip $(1))_EXTRA_OBJS) $(LIB)
endef
$(foreach program, $(PROGRAMS), $(eval $(call program_template,$(program))))

$(LIB): $(filter-out $(LIBVMFS_EXCLUDE_OBJS) $(foreach subdir,$(SUBDIRS),$($(subdir)/OBJS)),$(OBJS))
	ar -r $@ $^
	ranlib $@

$(OBJS): %.o: %.c $(HEADERS)

$(buildPROGRAMS) $(BUILD_PROGRAMS):
	$(CC) -o $@ $^ $(LDFLAGS)

clean: CLEAN := $(wildcard $(LIB) $(BUILD_PROGRAMS) $(PROGRAMS) $(OBJS) $(PACKAGE)-*.tar.gz $(MANPAGES) $(MANDOCBOOK))
clean:
	$(if $(CLEAN),rm $(CLEAN))

ALL_DIST := $(SRC) $(HEADERS) $(ALL_MAKEFILES) $(MANSRCS) $(EXTRA_DIST)
DIST_DIR := $(PACKAGE)-$(VERSION:v%=%)
dist: $(ALL_DIST)
	@rm -rf "$(DIST_DIR)"
	@mkdir "$(DIST_DIR)" $(foreach subdir,$(SUBDIRS),"$(DIST_DIR)/$(subdir)")
	tar -cf - $(ALL_DIST) | tar -C "$(DIST_DIR)" -xf -
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

installPROGRAMS := $(filter-out %/imager,$(buildPROGRAMS:%=$(DESTDIR)$(sbindir)/%))
installMANPAGES := $(filter-out $(foreach subdir, $(SUBDIRS),$(subdir)/%),$(MANPAGES))
installMANPAGES := $(installMANPAGES:%=$(DESTDIR)$(mandir)/man8/%)

$(installPROGRAMS): $(DESTDIR)$(sbindir)/%: %

INSTALLED_PROGRAMS := $(patsubst %,$(DESTDIR)$(sbindir)/%,$(notdir $(BUILD_PROGRAMS)))
$(foreach prog, $(BUILD_PROGRAMS), $(eval $(DESTDIR)$(sbindir)/$(notdir $(prog)): $(prog)))

$(installPROGRAMS) $(INSTALLED_PROGRAMS): %: $(DESTDIR)$(sbindir)
	install $(if $(NO_STRIP),,-s )-m 0755 $(filter-out $<,$^) $(dir $@)

$(installMANPAGES): $(DESTDIR)$(mandir)/man8/%: %

INSTALLED_MANPAGES := $(patsubst %.txt,$(DESTDIR)$(mandir)/man8/%.8,$(notdir $(foreach subdir,$(SUBDIRS),$($(subdir)/MANSRC))))
$(foreach man,$(foreach subdir,$(SUBDIRS),$($(subdir)/MANSRC:%.txt=%.8)), $(eval $(DESTDIR)$(mandir)/man8/$(notdir $(man)): $(man)))

$(installMANPAGES) $(INSTALLED_MANPAGES): %: $(DESTDIR)$(mandir)/man8
	install -m 0755 $(filter-out $<,$^) $(dir $@)

install: $(installPROGRAMS) $(installMANPAGES) $(INSTALLED_PROGRAMS) $(INSTALLED_MANPAGES)

ifeq (,$(filter dist clean,$(MAKECMDGOALS)))
test.img: imager.c | imager
	./imager -r $@ > $@.new
	diff $@ $@.new || ./imager -v $@.new
	mv -f $@.new $@
endif

.PHONY: all clean dist install doc

.gitignore: $(ALL_MAKEFILES)
	(echo "*.tar.gz"; \
	 echo "*.[ao]"; \
	 echo "*.xml"; \
	 echo "*.8"; \
	 echo "version"; \
	 echo "config.cache"; \
	 $(foreach program, $(PROGRAMS) $(BUILD_PROGRAMS),echo $(program);) \
	) > $@

config.cache: configure.mk
	@$(MAKE) -s -f $^ $(if $(ENV_CFLAGS),CFLAGS='$(ENV_CFLAGS)') $(if $(ENV_LDFLAGS),LDFLAGS='$(ENV_LDFLAGS)')
