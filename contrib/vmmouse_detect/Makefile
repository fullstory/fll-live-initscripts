COPTS  := -W -Wall -Werror
CFLAGS := -g -O2

vmmouse_detect_OBJECTS=vmmouse.o vmmouse_proto.o vmmouse_client.o
fll_vmmouse_detect: $(vmmouse_detect_OBJECTS)
	$(CC) $(COPTS) $(CFLAGS) $(LDFLAGS) -o $@ $(vmmouse_detect_OBJECTS)

install: fll_vmmouse_detect
	install -m 755 fll_vmmouse_detect $(PREFIX)/usr/sbin/

clean:
	$(RM) -f fll_vmmouse_detect $(vmmouse_detect_OBJECTS) core

distclean: clean
