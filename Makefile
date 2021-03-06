CFLAGS = -g -Wall -std=gnu99

TACHYON_OBJS=src/tachyon.o src/tty.o src/pal.o src/loop.o src/buffer.o \
	     src/controller.o src/predictor.o src/util.o src/vt.o
TOOLS=tools/delayed_echo

all: tachyon $(TOOLS)

tachyon: $(TACHYON_OBJS)
	$(CC) $(CFLAGS) -o tachyon $^

test: tachyon
	@lousy run

testd: tachyon
	@lousy run -d

run: tachyon
	./tachyon

clean:
	@-rm $(TACHYON_OBJS)
	@-rm tachyon
	@-rm $(TOOLS)
