OBJS =  
CC = gcc
CFLAGS = -O3 -Wall 
LDFLAGS = -ltiff

# gcc only
CCMACHINE = $(shell $(CC) -dumpmachine)

# On windows + mingw, link winsock2 lib.
ifeq ($(findstring mingw32, $(CCMACHINE)), mingw32)
LDFLAGS += -lws2_32
endif

.PHONY: all
all: lfpsplitter illum2tiff

lfpsplitter: lfpsplitter.o $(OBJS)
	$(CC) -o $@ lfpsplitter.o $(OBJS) $(CFLAGS) $(LDFLAGS)

illum2tiff: illum2tiff.o $(OBJS)
	$(CC) -o $@ illum2tiff.o $(OBJS) $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) lfpsplitter.o lfpsplitter
