EXEC = zflist

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

# fully shared with debug
all: CFLAGS += -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
all: LDFLAGS += -g -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -lcurl -fopenmp -lsqlite3 -L../libflist -lflist
all: $(EXEC)

# embedded with debug
embedded: CFLAGS += -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
embedded: LDFLAGS += -g ../libflist/libflist.a -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lcurl -lssl -lcrypto -lhiredis -fopenmp -lsqlite3
embedded: $(EXEC)

# embeded, static without debug
production: CFLAGS += -std=c99 -W -Wall -O2 -I../libflist
production: LDFLAGS += -static-libstdc++ -static-libgcc -Wl,-Bstatic -L../libflist -lflist -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lcurl -lssl -lcrypto -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
production: $(EXEC)

# embeded, static without debug
production-mbed: CFLAGS += -std=c99 -W -Wall -O2 -I../libflist
production-mbed: LDFLAGS += -static-libstdc++ -static-libgcc -Wl,-Bstatic -L../libflist -lflist -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -fopenmp -lcurl -lmbedcrypto -lmbedx509 -lmbedtls -lmbedcrypto -lmbedx509 -lmbedtls -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
production-mbed: $(EXEC)

# shared without debug
release: CFLAGS += -std=c99 -W -Wall -O2 -I../libflist
release: LDFLAGS += -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -lcurl -fopenmp -lsqlite3 -L../libflist -lflist
release: $(EXEC)

# static libflist, shared all others, without debug
sl-release: CFLAGS += -std=c99 -W -Wall -O2 -I../libflist
sl-release: LDFLAGS += -Wl,-Bstatic -L../libflist -lflist -Wl,-Bdynamic -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -ljansson -lhiredis -lcurl -fopenmp -lsqlite3
sl-release: $(EXEC)

# embedded with libraries static linked (except libc)
s-embedded: CFLAGS += -std=c99 -W -Wall -O2 -g -DFLIST_DEBUG -I../libflist
s-embedded: LDFLAGS += -Wl,-Bstatic -L../libflist -lflist -ltar -lz -lb2 -lcapnp_c -ljansson -lsnappy -ljansson -lhiredis -fopenmp -lcurl -lssl -lcrypto -lsqlite3 -Wl,-Bdynamic -pthread -lrt -ldl
s-embedded: $(EXEC)

# using CXX for snappy in static
$(EXEC): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

