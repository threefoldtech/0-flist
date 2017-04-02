EXEC = flister

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

debug: CFLAGS  = -std=c99 -W -Wall -O2 -g
debug: LDFLAGS = -pthread -ltar -lrt -lgflags -lz -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy
debug: $(EXEC)

all: CFLAGS  = -std=c99 -W -Wall -O2
all: LDFLAGS = -static -pthread -ltar -lrt -lgflags -lz -ljemalloc -lb2 -lcapnp_c -lrocksdb -llz4 -lbz2 -lz -lsnappy
all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(EXEC)

