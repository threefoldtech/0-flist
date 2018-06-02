EXEC = flister

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: CFLAGS  = -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG
all: LDFLAGS = -g -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis
all: $(EXEC)

release: CFLAGS  = -std=c99 -W -Wall -O2
release: LDFLAGS = -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis
release: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

