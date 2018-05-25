default: all

release:
	cd src && $(MAKE) $@
	cp src/flister .
	strip -s flister

all:
	cd src && $(MAKE) $@
	cp src/flister .

mrproper:
	cd src && $(MAKE) $@
	rm -f flister

.DEFAULT:
	cd src && $(MAKE) $@
