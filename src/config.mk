EXEC = flister

CFLAGS  = -std=c99 -W -Wall -O2 -g -I/opt/rocksdb/include
LDFLAGS = -ltar -lz -lpthread -lrt -lsnappy -lgflags -lz -lbz2 -llz4 -lrocksdb -L/opt/rocksdb -ljemalloc -lb2 -lcapnp_c
