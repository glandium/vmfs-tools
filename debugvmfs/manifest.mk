LDFLAGS := $(DLOPEN_LDFLAGS)
debugvmfs.o_CFLAGS := -include version
REQUIRES := libvmfs libreadcmd
