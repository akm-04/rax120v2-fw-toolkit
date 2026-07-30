#
#	Copyright (C) 2009 Delta Networks Inc.
#

CC = gcc

FILES_MAPCALC = mapcalc.o

CFLAGS += -Wall -O2 -D_FILE_OFFSET_BITS=64

all : mapcalc

mapcalc : $(FILES_MAPCALC)
	$(CC) $(CFLAGS) $(FILES_MAPCALC) -o $@ $(EXTRA_LDFLAGS) $(LDFLAGS) -lubox -lubus
	@echo "make $@ finished on `date`"

%.o : %.c %.h 
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o 
	rm -f mapcalc
