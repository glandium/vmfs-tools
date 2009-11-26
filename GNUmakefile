ifneq (clean,$(MAKECMDGOALS))
-include version
endif
-include config.cache
include utils.mk

PACKAGE := vmfs-tools

all:

SUBDIRS := $(subst /,,$(dir $(wildcard */manifest.mk)))
ifeq (,$(fuse/LDFLAGS))
ifneq (clean,$(MAKECMDGOALS))
SUBDIRS := $(filter-out vmfs-fuse,$(SUBDIRS))
endif
endif

ENV_CFLAGS := $(CFLAGS)
ENV_LDFLAGS := $(LDFLAGS)
CFLAGS :=
LDFLAGS :=
__NEW_VARS := CFLAGS LDFLAGS

define subdir_variables
__VARS := $$(filter-out $$(__NEW_VARS),$$(.VARIABLES))
include $(1)/manifest.mk
__NEW_VARS := $$(filter-out $$(__VARS) __VARS,$$(.VARIABLES))
$$(foreach var,$$(__NEW_VARS), $$(if $$($$(var)),$$(eval $(1)/$$(var) := $$($$(var)))$$(eval $$(var) := )))
$(1)/SRC := $(wildcard $(1)/*.c)
$(1)/HEADERS := $(wildcard $(1)/*.h)
$(1)/OBJS := $$($(1)/SRC:%.c=%.o)
ifeq (,$(filter lib%,$(1)))
$(1)/TARGET := $(1)/$(1)
$(1)/MANSRC := $(wildcard $(1)/$(1).txt)
else
$(1)/TARGET := $(1)/$(1).a
endif
$(1)/CFLAGS += -I$(1)
endef
$(foreach subdir,$(SUBDIRS), $(eval $(call subdir_variables,$(subdir))))

define order_by_requires
$(eval __result := $(foreach subdir,$(1),$(if $(filter $(1),$($(subdir)/REQUIRES)),,$(subdir))))
$(eval __tmp := $(filter-out $(__result),$(1)))
$(if $(filter-out $(__tmp),$(1)),,$(error Dependency loop between subdirectories))
$(if $(__tmp),$(call order_by_requires,$(__tmp),$(2) $(__result)),$(2) $(__result))
endef

define subdir_rules
$(1)/CFLAGS += $$(foreach require,$$($(1)/REQUIRES),$$($$(require)/CFLAGS))
$(1)/LDFLAGS += $$(foreach require,$$($(1)/REQUIRES),$$($$(require)/LDFLAGS))
$$($(1)/TARGET): LDFLAGS += $$($(1)/LDFLAGS)
$$($(1)/TARGET): $$($(1)/OBJS) $$(foreach require,$$($(1)/REQUIRES),$$($$(require)/TARGET))
$$(foreach obj,$$($(1)/OBJS), $$(eval $$(obj): CFLAGS += $$($(1)/CFLAGS) $$($$(obj)_CFLAGS)))
endef
$(foreach subdir,$(strip $(call order_by_requires,$(SUBDIRS))),$(eval $(call subdir_rules,$(subdir))))

CC := gcc
OPTIMFLAGS := $(if $(filter -O%,$(CFLAGS)),,-O2)
CFLAGS := $(ENV_CFLAGS) $(filter-out $(ENV_CFLAGS),-Wall $(OPTIMFLAGS) -g -D_FILE_OFFSET_BITS=64 $(EXTRA_CFLAGS))
CFLAGS += $(if $(HAS_STRNDUP),,-DNO_STRNDUP=1)
LDFLAGS := $(ENV_LDFLAGS) $(filter-out $(ENV_LDFLAGS),$(EXTRA_LDFLAGS))
SRC := $(wildcard *.c) $(foreach subdir,$(SUBDIRS),$($(subdir)/SRC))
HEADERS := $(wildcard *.h) $(foreach subdir,$(SUBDIRS),$($(subdir)/HEADERS))
OBJS := $(SRC:%.c=%.o)
BUILD_PROGRAMS := $(foreach subdir,$(filter-out lib%,$(SUBDIRS)),$($(subdir)/TARGET))
BUILD_LIBS := $(foreach subdir,$(filter lib%,$(SUBDIRS)),$($(subdir)/TARGET))
MANSRCS := $(foreach subdir,$(SUBDIRS),$($(subdir)/MANSRC))
MANDOCBOOK := $(MANSRCS:%.txt=%.xml)
MANPAGES := $(foreach man,$(MANSRCS),$(man:%.txt=%.$(shell sed '1{s/^.*(//;s/)//;q;}' $(man))))

EXTRA_DIST := LICENSE README TODO AUTHORS test.img configure

all: $(BUILD_PROGRAMS) $(wildcard .gitignore) test.img

ALL_MAKEFILES = $(filter-out config.cache,$(MAKEFILE_LIST)) configure.mk

version: $(filter-out version, $(ALL_MAKEFILES)) $(SRC) $(HEADERS) $(wildcard .git/logs/HEAD .git/refs/tags)
	echo VERSION := $(GEN_VERSION) > $@

$(BUILD_LIBS):
	ar -r $@ $^
	ranlib $@

$(OBJS): %.o: %.c $(HEADERS)

$(BUILD_PROGRAMS):
	$(CC) -o $@ $^ $(LDFLAGS)

clean: CLEAN := $(wildcard $(LIB) $(BUILD_PROGRAMS) $(OBJS) $(PACKAGE)-*.tar.gz $(MANPAGES) $(MANDOCBOOK))
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

INSTALL_PROGRAMS := $(foreach prog,$(BUILD_PROGRAMS),$(if $(filter noinst,$($(prog)_OPTIONS)),,$(prog)))
INSTALLED_PROGRAMS := $(patsubst %,$(DESTDIR)$(sbindir)/%,$(notdir $(INSTALL_PROGRAMS)))
$(foreach prog, $(INSTALL_PROGRAMS), $(eval $(DESTDIR)$(sbindir)/$(notdir $(prog)): $(prog)))

$(INSTALLED_PROGRAMS): %: $(DESTDIR)$(sbindir)
	install $(if $(NO_STRIP),,-s )-m 0755 $(filter-out $<,$^) $(dir $@)

INSTALLED_MANPAGES := $(patsubst %.txt,$(DESTDIR)$(mandir)/man8/%.8,$(notdir $(foreach subdir,$(SUBDIRS),$($(subdir)/MANSRC))))
$(foreach man,$(foreach subdir,$(SUBDIRS),$($(subdir)/MANSRC:%.txt=%.8)), $(eval $(DESTDIR)$(mandir)/man8/$(notdir $(man)): $(man)))

$(INSTALLED_MANPAGES): %: $(DESTDIR)$(mandir)/man8
	install -m 0755 $(filter-out $<,$^) $(dir $@)

install: $(INSTALLED_PROGRAMS) $(INSTALLED_MANPAGES)

ifeq (,$(filter dist clean,$(MAKECMDGOALS)))
test.img: imager/imager.c | imager/imager
	./imager/imager -r $@ > $@.new
	diff $@ $@.new || ./imager/imager -v $@.new
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
	 $(foreach program, $(BUILD_PROGRAMS),echo $(program);) \
	) > $@

config.cache: configure.mk
	@$(MAKE) -s -f $^ $(if $(ENV_CFLAGS),CFLAGS='$(ENV_CFLAGS)') $(if $(ENV_LDFLAGS),LDFLAGS='$(ENV_LDFLAGS)')
