# Now this is still minimalistic ;-)

.DEFAULT_GOAL := thinkfan

thinkfan:	system.o parser.o config.o thinkfan.o message.o
			gcc $(CFLAGS) -Wall -o thinkfan system.o config.o parser.o \
			thinkfan.o message.o

message.o:	message.c globaldefs.h
			gcc $(CFLAGS) -Wall -c message.c

system.o:	system.c system.h message.h globaldefs.h parser.h
			gcc $(CFLAGS) -Wall -c system.c

config.o:	config.c config.h message.h globaldefs.h system.h parser.h
			gcc $(CFLAGS) -Wall -c config.c

thinkfan.o:	thinkfan.c thinkfan.h message.h globaldefs.h config.h
			gcc $(CFLAGS) -Wall -c thinkfan.c

clean:
		rm -rf *.o thinkfan
