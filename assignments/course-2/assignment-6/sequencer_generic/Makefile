INCLUDE_DIRS = /home/bwaggle/Code/assignments/course-2/assignment-1/sequencer_generic
LIB_DIRS = 
CC=gcc

CDEFS=
CFLAGS= -O3 -I$(INCLUDE_DIRS) $(CDEFS)
LIBS= 

HFILES= sys_logger.h
CFILES= seqgenex0.c sys_logger.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	seqgenex0 

clean:
	-rm -f *.o *.d
	-rm -f seqgenex0 

seqgenex0: $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $(OBJS) -lpthread -lrt


depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
