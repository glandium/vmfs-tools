debugvmfs_LDFLAGS := $(DLOPEN_LDFLAGS)
debugvmfs.o_CFLAGS := -DVERSION=\"$(VERSION)\"
debugvmfs_REQUIRES := libvmfs
