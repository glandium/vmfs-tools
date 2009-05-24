PACKAGE := vmfs-tools

CC := gcc
OPTIMFLAGS := -O2
CFLAGS := -Wall $(OPTIMFLAGS) -g -D_FILE_OFFSET_BITS=64 $(EXTRA_CFLAGS)
LDFLAGS := $(shell pkg-config --libs uuid)
SRC := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := $(SRC:%.c=%.o)
PROGRAMS := debugvmfs vmfs-fuse
EXTRA_DIST := LICENSE README TODO

prefix := /usr

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
debugvmfs: LDFLAGS+=-lreadline
debugvmfs.o: CFLAGS+=-DVERSION=\"$(VERSION)\"

define program_template
$(strip $(1))_EXTRA_OBJS := $$($(strip $(1))_EXTRA_SRCS:%.c=%.o)
LIBVMFS_EXCLUDE_OBJS += $(1).o $$($(strip $(1))_EXTRA_OBJS)
$(1): $(1).o $$($(strip $(1))_EXTRA_OBJS) libvmfs.a
endef
$(foreach program, $(PROGRAMS), $(eval $(call program_template,$(program))))

libvmfs.a: $(filter-out $(LIBVMFS_EXCLUDE_OBJS),$(OBJS))
	ar -r $@ $^
	ranlib $@

$(OBJS): %.o: %.c $(HEADERS)

$(PROGRAMS):
	$(CC) -o $@ $^ $(LDFLAGS)

clean: CLEAN := $(wildcard libvmfs.a $(PROGRAMS) $(OBJS) $(PACKAGE)-*.tar.gz)
clean:
	$(if $(CLEAN),rm $(CLEAN))

ALL_DIST := $(SRC) $(HEADERS) $(MAKEFILE_LIST) $(EXTRA_DIST)
DIST_DIR := $(PACKAGE)-$(VERSION:v%=%)
dist: $(ALL_DIST)
	@rm -rf "$(DIST_DIR)"
	@mkdir "$(DIST_DIR)"
	cp -p $(ALL_DIST) $(DIST_DIR)
	tar -zcf "$(DIST_DIR).tar.gz" "$(DIST_DIR)"
	@rm -rf "$(DIST_DIR)"

install:
	install -d -m 0755 $(DESTDIR)$(prefix)/sbin
	install -m 0755 $(PROGRAMS) $(DESTDIR)$(prefix)/sbin

.PHONY: all clean dist install

.gitignore: $(MAKEFILE_LIST)
	(echo "*.tar.gz"; \
	 echo "*.[ao]"; \
	 echo "version"; \
	 $(foreach program, $(PROGRAMS),echo $(program);) \
	) > $@
