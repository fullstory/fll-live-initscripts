MAN_TXT = $(wildcard *.txt)
MAN = $(patsubst %.txt,%,$(MAN_TXT))

man: $(MAN)

%.1 %.8 : %.*.txt
	a2x -d manpage -f manpage $<
	rm -f $@.xml

clean:
	rm -f *.1 *.1.xml *.8 *.8.xml
