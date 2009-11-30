LDFLAGS := $(DLOPEN_LDFLAGS)
vmfs-lvm.o_CFLAGS := -include version
REQUIRES := libvmfs libreadcmd
