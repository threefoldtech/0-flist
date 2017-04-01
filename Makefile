default: all

all:
	cd src && $(MAKE) $@
	cp src/flister .

mrproper:
	cd src && $(MAKE) $@
	rm -f flister

.DEFAULT:
	cd src && $(MAKE) $@
