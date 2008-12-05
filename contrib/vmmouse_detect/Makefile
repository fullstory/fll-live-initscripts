COPTS  := -W -Wall -Werror
CFLAGS := -g -O2

vmmouse_detect_OBJECTS=vmmouse.o vmmouse_proto.o vmmouse_client.o
fll_vmmouse_detect: $(vmmouse_detect_OBJECTS)
	$(CC) $(COPTS) $(CFLAGS) $(LDFLAGS) -o $@ $(vmmouse_detect_OBJECTS)
	sed	-e "s/vmmouse_detect/fll_&/g"		\
		-e "s/\(vmmouse_detect\ \)1/\18/"	\
			vmmouse_detect.man > fll_vmmouse_detect.8

install: fll_vmmouse_detect
	install -m 755 fll_vmmouse_detect $(PREFIX)/usr/sbin/

clean:
	$(RM) -f fll_vmmouse_detect $(vmmouse_detect_OBJECTS) core fll_vmmouse_detect.8

distclean: clean
