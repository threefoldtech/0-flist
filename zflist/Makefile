EXEC = zflist

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

# fully shared with debug
all: CFLAGS += -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
all: LDFLAGS += -g -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3 -L../libflist -lflist
all: $(EXEC)

# embedded with debug
embedded: CFLAGS += -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
embedded: LDFLAGS += -g ../libflist/libflist.a -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3
embedded: $(EXEC)

# embeded, static without debug
production: CFLAGS += -std=c99 -W -Wall -O2 -I../libflist
production: LDFLAGS += -Wl,-Bstatic -L../libflist -lflist -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
production: $(EXEC)

# shared without debug
release: CFLAGS += -std=c99 -W -Wall -O2 -I../libflist
release: LDFLAGS += -pthread -L../libflist -lflist -ltar -lrt -lb2 -lcapnp_c -lz -lsnappy -ljansson -l0stor -lhiredis -fopenmp -lsqlite3
release: $(EXEC)

# embedded with libraries static linked (except libc)
s-embedded: CFLAGS += -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
s-embedded: LDFLAGS += -Wl,-Bstatic -L../libflist -lflist -ltar -lz -lb2 -lcapnp_c -ljansson -lsnappy -ljansson -lhiredis -fopenmp -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
s-embedded: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

