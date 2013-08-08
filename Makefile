CFLAGS = -g

TACHYON_OBJS=src/tachyon.o src/tty.o src/pal.o src/loop.o
TOOLS=tools/delayed_echo

all: tachyon $(TOOLS)

tachyon: $(TACHYON_OBJS)
	$(CC) $(CFLAGS) -o tachyon $^

run: tachyon
	./tachyon
