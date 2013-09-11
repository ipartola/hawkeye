
all:
	$(MAKE) -C src

install: all
	mkdir -p $(DESTDIR)/usr/bin
	install -m 755 src/hawkeye $(DESTDIR)/usr/bin/hawkeye

clean:
	$(MAKE) -C src clean

.PHONY: clean
