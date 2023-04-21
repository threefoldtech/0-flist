LIBRARY = libflist

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: CFLAGS += -fPIC -std=c99 -W -Wall -O2 -g 
all: LDFLAGS += -g -pthread -ltar -lb2 -lz -lcapnp_c -lsnappy -lhiredis -fopenmp -lsqlite3
all: $(LIBRARY).so

# include musl-fts on alpine
alpine: LDFLAGS += -lfts
alpine: all

$(LIBRARY).so: $(OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	ar rcs $(LIBRARY).a $(OBJ)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(LIBRARY).so $(LIBRARY).a

