EXEC = flister

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: CFLAGS = -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG
all: LDFLAGS = -g -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
all: $(EXEC)

release: CFLAGS = -std=c99 -W -Wall -O2
release: LDFLAGS = -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
release: $(EXEC)

static: CFLAGS = -std=c99 -W -Wall -O2
static: LDFLAGS = -static -pthread -ltar -lrt -lrocksdb -lgflags -lz -lbz2 -llz4 -lzstd -lb2 -lcapnp_c -ljemalloc -ljansson -l0stor -lcrypto -lsnappy -lhiredis -fopenmp
static: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

