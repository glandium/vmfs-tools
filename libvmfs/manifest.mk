utils.o_CFLAGS := $(if $(HAS_POSIX_MEMALIGN),,-DNO_POSIX_MEMALIGN=1)
libvmfs.a_REQUIRES := uuid
