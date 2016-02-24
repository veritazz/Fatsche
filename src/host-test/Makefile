CFLAGS  = -DHOST_TEST
CFLAGS += -I../

target=host_test

objs = \
	main.o \
	../images.o \
	../Fatsche.o

all: $(target) Makefile

$(target): $(objs)
	gcc $^ -o $@ -lncurses

clean:
	rm -f *.o
	rm -f $(target)

run: all
	xterm -maximized &

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

%.o: %.cc
	g++ $(CFLAGS) -c -o $@ $<

%.c: Makefile
