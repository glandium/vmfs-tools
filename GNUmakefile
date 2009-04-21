CC=gcc
CFLAGS=-Wall -O2 -g -D_FILE_OFFSET_BITS=64
LDFLAGS=$(shell pkg-config --libs uuid)
SRC=$(wildcard *.c)
OBJS=$(SRC:%.c=%.o)
PROGRAMS=cfvmfs

all: $(PROGRAMS)

define program_template
$(1): $(filter-out $(foreach program,$(filter-out $(1), $(PROGRAMS)),$(program).o), $(OBJS))
endef
$(foreach program, $(PROGRAMS), $(eval $(call program_template, $(program))))

$(OBJS): %.o: %.c $(wildcard *.h)

$(PROGRAMS):
	$(CC) -o $@ $^ $(LDFLAGS)

clean: CLEAN=$(filter $(PROGRAMS) $(OBJS),$(wildcard *))
clean:
	$(if $(CLEAN),rm $(CLEAN))

.PHONY: all clean
