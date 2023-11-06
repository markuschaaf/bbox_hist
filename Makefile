prefix = /usr/local
exes = bbox_hist
.PHONY: all install uninstall clean
all: $(exes)

%: %.c
	gcc $(CFLAGS) $(CPPFLAGS) -Wall -Os $(LDFLAGS) $(TARGET_ARCH) -o $@ $<

%: %.cpp
	g++ $(CXXFLAGS) $(CPPFLAGS) -Wall -Os $(LDFLAGS) $(TARGET_ARCH) -o $@ $<

install: $(exes)
	install -D -t $(DESTDIR)$(prefix)/bin $^

uninstall:
	cd $(DESTDIR)$(prefix)/bin
	rm -f $(exes)

clean:
	rm -f $(exes)
