CFLAGS=-g

OBJS=tachyon.o tty.o pal.o loop.o

all: tachyon

tachyon: $(OBJS)
	$(CC) $(CFLAGS) -o tachyon $(OBJS)

run: tachyon
	./tachyon
