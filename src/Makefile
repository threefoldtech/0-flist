LIBRARY = libflist

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: CFLAGS = -fPIC -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG
# all: LDFLAGS = -g -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
all: LDFLAGS = -g -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3
all: $(LIBRARY).so

release: CFLAGS = -fPIC -std=c99 -W -Wall -O2
# release: LDFLAGS = -pthread -ltar -lrt -lgflags -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy -lcrypto -ljansson -l0stor -lhiredis -fopenmp
release: LDFLAGS = -pthread -ltar -lrt -lb2 -lcapnp_c -lz -lsnappy -ljansson -l0stor -lhiredis -fopenmp -lsqlite3
release: $(LIBRARY).so

$(LIBRARY).so: $(OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	ar rcs $(LIBRARY).a $(OBJ)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(LIBRARY).so $(LIBRARY).a

