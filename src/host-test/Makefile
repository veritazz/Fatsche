CFLAGS  = -std=c++11 -DHOST_TEST
CFLAGS += -I../
CFLAGS += -I$(HOME)/sketchbook/libraries/Arduboy/src/

target=host_test

objs = \
	main.o \
	../images.o \
	../Fatsche.o \
	../VeritazzExtra.o \
	../ArduboyVeritazz.o \
	../ArduboyCoreVeritazz.o \

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

%.o: %.cpp
	g++ $(CFLAGS) -c -o $@ $<

%.c: Makefile
