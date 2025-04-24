CC = gcc
LIBS = 
CFLAGS = -O2 -Wall

OBJS = output.o output_c.o output_i386.o as68k.o dict.o

default: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o as68k $(LIBS)
