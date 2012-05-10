# Now this is still minimalistic ;-)

.DEFAULT_GOAL := thinkfan

CC ?= gcc

thinkfan:	system.o parser.o config.o thinkfan.o message.o
		$(CC) $(LDFLAGS) -Wall -o $@ $^

%.o:	%.c
		$(CC) $(CFLAGS) -Wall -c -o $@ $<

clean:
		rm -rf *.o thinkfan
