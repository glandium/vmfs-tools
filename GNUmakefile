PACKAGE := vmfs-tools
VERSION := $(shell (cat version || ( [ -d .git ] && git describe --tags --abbrev=4 HEAD | sed s/-/./g ) || echo v0.0.0) 2> /dev/null)

CC := gcc
CFLAGS := -Wall -O2 -g -D_FILE_OFFSET_BITS=64 $(EXTRA_CFLAGS)
LDFLAGS := $(shell pkg-config --libs uuid)
SRC := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJS := $(SRC:%.c=%.o)
PROGRAMS := debugvmfs vmfs-fuse
prefix := /usr

all: $(PROGRAMS) $(wildcard .gitignore)

vmfs-fuse: LDFLAGS+=$(shell pkg-config --libs fuse)
vmfs-fuse.o: CFLAGS+=$(shell pkg-config --cflags fuse)

debugvmfs_EXTRA_SRCS := readcmd.c
debugvmfs: LDFLAGS+=-lreadline

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

clean: CLEAN := $(filter libvmfs.a $(PROGRAMS) $(OBJS) $(PACKAGE)-%.tar.gz,$(wildcard *))
clean:
	$(if $(CLEAN),rm $(CLEAN))

ALL_DIST := $(SRC) $(HEADERS) $(MAKEFILE_LIST) LICENSE README
DIST_DIR := $(PACKAGE)-$(VERSION:v%=%)
dist: $(ALL_DIST)
	@rm -rf "$(DIST_DIR)"
	@mkdir "$(DIST_DIR)"
	cp -p $(ALL_DIST) $(DIST_DIR)
	echo $(VERSION) > $(DIST_DIR)/version
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
	) > .gitignore
