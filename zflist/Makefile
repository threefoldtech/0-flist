EXEC = zflist

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: CFLAGS = -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
all: LDFLAGS = -g -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3 -L../libflist -lflist
all: $(EXEC)

embedded: CFLAGS = -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
embedded: LDFLAGS = -g ../libflist/libflist.a -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3
embedded: $(EXEC)

release: CFLAGS = -std=c99 -W -Wall -O2 -I../libflist
release: LDFLAGS = -pthread -L../libflist -lflist -ltar -lrt -lb2 -lcapnp_c -lz -lsnappy -ljansson -l0stor -lhiredis -fopenmp -lsqlite3
release: $(EXEC)

static: CFLAGS = -std=c99 -W -Wall -O2 -I../libflist
static: LDFLAGS = -Wl,-Bstatic -L../libflist -lflist -ltar -lz -lb2 -lcapnp_c -ljansson -lsnappy -lhiredis -fopenmp -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
static: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

