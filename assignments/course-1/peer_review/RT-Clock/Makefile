INCLUDE_DIRS = /home/bwaggle/Code/assignments/peer_review/RT-Clock
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 -I$(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= sys_logger.h
CFILES= posix_clock.c sys_logger.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	posix_clock

clean:
	-rm -f *.o *.d
	-rm -f perfmon posix_clock

distclean:
	-rm -f *.o *.d
	-rm -f posix_clock

posix_clock: $(OBJS)  # Depend on object files
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) -lpthread

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
