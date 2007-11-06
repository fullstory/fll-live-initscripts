MAN1_TXT = $(wildcard *.1.txt)
MAN8_TXT = $(wildcard *.8.txt)
MAN1 = $(patsubst %.1.txt,%.1,$(MAN1_TXT))
MAN8 = $(patsubst %.8.txt,%.8,$(MAN8_TXT))

man: man1 man8

man1: $(MAN1)
man8: $(MAN8)

%.1 : %.1.txt
	a2x -d manpage -f manpage $<
	rm -f $@.xml

%.8 : %.8.txt
	a2x -d manpage -f manpage $<
	rm -f $@.xml

clean:
	rm -f *.1 *.1.xml
	rm -f *.8 *.8.xml
