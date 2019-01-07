#
# Copyright(c) 2012-2018 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

PWD=$(shell pwd)
OCFDIR=../../
FRAMEWORKDIR=$(PWD)/framework
SRCDIR=$(FRAMEWORKDIR)/src
INCDIR=$(FRAMEWORKDIR)/include

CC = gcc
CFLAGS = -g -Wall -I$(INCDIR) -I$(SRCDIR)
LDFLAGS = -lm -pthread

SRC=$(shell find $(SRCDIR) -name \*.c)
OBJS = $(patsubst %.c, %.o, $(SRC))
OCFLIB=libocf.so

all: | sync $(OBJS) $(OCFLIB)

$(OCFLIB): $(OBJS)
	$(CC) -shared -o libocf.so $(CFLAGS) $^ -fPIC $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ -fPIC $^ $(LDFLAGS)

sync:
	$(MAKE) -C $(OCFDIR) inc O=$(FRAMEWORKDIR)
	$(MAKE) -C $(OCFDIR) src O=$(FRAMEWORKDIR)

clean:
	rm -rf $(OCFLIB) $(OBJS)

distclean:
	rm -rf $(OCFLIB) $(OBJS)
	rm -rf $(SRCDIR)/ocf
	rm -rf $(INCDIR)/ocf

.PHONY: all clean
