LIBRARY = g8storclient.so
PACKAGE = g8storclient

SRC = lib0stor-python.c
OBJ = lib0stor-python.o

CFLAGS  = -pthread -g -fPIC -I/usr/include/python3.5m -W -Wall -O2 -Wno-missing-field-initializers -I../src/ 
LDFLAGS = ../src/lib0stor.a -lssl -lcrypto -lsnappy -lz -lm -lpython3.5m

all:
	@echo "This Makefile is here only for legacy purpose"
	@echo "Please use setup.py to compile and install this module"
	@echo "If you still want to compile the library manually, please 'make g8storclient.so'"

distutils:
	python3 setup.py build

$(LIBRARY): $(OBJ)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -fv *.o

mrproper: clean
	rm -rfv $(LIBRARY) build

dist: $(LIBRARY)
	mkdir -p $(PACKAGE)
	rm -rf $(PACKAGE)/*
	cp $(LIBRARY) $(PACKAGE)
	@echo "You can now install package: python3 setup.py install --user"
