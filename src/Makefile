CFLAGS=-g

OBJS=tachyon.o tty.o pal.o

tachyon: $(OBJS)
	$(CC) $(CFLAGS) -o tachyon $(OBJS)

run: tachyon
	./tachyon
