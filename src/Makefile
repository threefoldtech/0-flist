EXEC = flister

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: CFLAGS = -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG
# all: LDFLAGS = -g -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
all: LDFLAGS = -g -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -l0stor -lhiredis -fopenmp -lsqlite3
all: $(EXEC)

release: CFLAGS = -std=c99 -W -Wall -O2
# release: LDFLAGS = -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
release: LDFLAGS = -pthread -ltar -lrt -lb2 -lcapnp_c -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp -lsqlite3
release: $(EXEC)

static: CFLAGS = -std=c99 -W -Wall -O2
# static: LDFLAGS = -Wl,-Bstatic -ltar -lrt -lrocksdb -lgflags -lz -lbz2 -llz4 -lb2 -lcapnp_c -ljemalloc -ljansson -l0stor -lcrypto -lsnappy -lhiredis -fopenmp -Wl,-Bdynamic -pthread -lrt
static: LDFLAGS = -Wl,-Bstatic -ltar -lz -lb2 -lcapnp_c -ljansson -l0stor -lcrypto -lsnappy -lhiredis -fopenmp -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
static: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

