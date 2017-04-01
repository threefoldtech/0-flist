default: all

all:
	cd src && $(MAKE) $@
	cp src/flister .
	# strip -s flister

mrproper:
	cd src && $(MAKE) $@
	rm -f flister

.DEFAULT:
	cd src && $(MAKE) $@
