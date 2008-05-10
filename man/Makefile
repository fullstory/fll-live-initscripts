MAN_TXT = $(wildcard *.txt)
MAN = $(patsubst %.txt,%,$(MAN_TXT))
XML = $(patsubst %,%.xml,$(MAN))

all: $(MAN)

%.1 %.2 %.3 %.4 %.5 %.6 %.7 %.8 : %.*.txt
	a2x -d manpage -f manpage $<

clean:
	$(RM) $(MAN) $(XML)
