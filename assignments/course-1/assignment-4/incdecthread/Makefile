INCLUDE_DIRS =/home/bwaggle/Code/assignments/course-1/assignment-4/incdecthread
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 -I$(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= sys_logger.h
CFILES= pthread.c sys_logger.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	pthread

clean:
	-rm -f *.o *.d
	-rm -f perfmon pthread

distclean:
	-rm -f *.o *.d
	-rm -f pthread

pthread: $(OBJS)  # Depend on object files
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) -lpthread

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
