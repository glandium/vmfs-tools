LDFLAGS := $(DLOPEN_LDFLAGS)
vmfs-lvm.o_CFLAGS := -DVERSION=\"$(VERSION)\"
REQUIRES := libvmfs libreadcmd
