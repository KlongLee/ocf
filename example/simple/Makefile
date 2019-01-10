OCFDIR=../../
SRCDIR=src/
INCDIR=include/

SRC=$(shell find ${SRCDIR} -name \*.c)
OBJS = $(patsubst %.c, %.o, $(SRC))
PROGRAM=simple

CC = gcc
CFLAGS = -g -Wall -I${INCDIR} -I${SRCDIR}/ocf/env/
LDFLAGS = -lm -lz -pthread

all: sync
	$(MAKE) $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

sync:
	@$(MAKE) -C ${OCFDIR} inc O=example/simple
	@$(MAKE) -C ${OCFDIR} src O=example/simple
	@$(MAKE) -C ${OCFDIR} env O=example/simple ENV=posix

clean:
	@rm -rf $(PROGRAM) $(OBJS)

distclean:
	@rm -rf $(PROGRAM) $(OBJS)
	@rm -rf src/ocf
	@rm -rf include/ocf

.PHONY: all clean
