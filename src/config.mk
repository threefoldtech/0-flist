EXEC = flister

# CFLAGS  = -std=c99 -W -Wall -O2 -I/opt/rocksdb/include
CFLAGS  = -std=c99 -W -Wall -O2 -g -I/opt/rocksdb/include
# LDFLAGS = -static -pthread -ltar -lrt -lgflags -lz -ljemalloc -lb2 -lcapnp_c -L/opt/rocksdb -lrocksdb -llz4 -lbz2 -lz -lsnappy
LDFLAGS = -pthread -ltar -lrt -lgflags -lz -ljemalloc -lb2 -lcapnp_c -L/opt/rocksdb -lrocksdb -llz4 -lbz2 -lz -lsnappy
