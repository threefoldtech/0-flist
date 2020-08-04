default: all

release:
	cd libflist && $(MAKE)
	cd zflist && $(MAKE) embedded
	cp zflist/zflist zflist-embedded
	strip -s zflist-embedded

all:
	cd libflist && $(MAKE) $@
	cd zflist && $(MAKE) embedded
	cp zflist/zflist zflist-embedded
	strip -s zflist-embedded

mrproper:
	cd libflist && $(MAKE) $@
	cd zflist && $(MAKE) $@
	rm -f zflist-embedded

.DEFAULT:
	cd libflist && $(MAKE) $@
