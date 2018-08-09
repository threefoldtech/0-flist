EXEC = zflist

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: CFLAGS = -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../src
# all: LDFLAGS = -g -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
all: LDFLAGS = -g -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3 -L../src -lflist
all: $(EXEC)

embedded: CFLAGS = -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../src
embedded: LDFLAGS = -g ../src/libflist.a -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3
embedded: $(EXEC)

release: CFLAGS = -std=c99 -W -Wall -O2 -I../src
# release: LDFLAGS = -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
release: LDFLAGS = -pthread -L../src -lflist -ltar -lrt -lb2 -lcapnp_c -lz -lsnappy -ljansson -l0stor -lhiredis -fopenmp -lsqlite3
release: $(EXEC)

static: CFLAGS = -std=c99 -W -Wall -O2 -I../src
# static: LDFLAGS = -Wl,-Bstatic -ltar -lrt -lrocksdb -lgflags -lz -lbz2 -llz4 -lb2 -lcapnp_c -ljemalloc -ljansson -l0stor -lcrypto -lsnappy -lhiredis -fopenmp -Wl,-Bdynamic -pthread -lrt
static: LDFLAGS = -Wl,-Bstatic -L../src -lflist -ltar -lz -lb2 -lcapnp_c -ljansson -lsnappy -lhiredis -fopenmp -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
static: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

