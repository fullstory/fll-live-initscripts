all:
	for i in bin contrib man; do $(MAKE) -C $$i $@; done

clean:
	for i in bin contrib man; do $(MAKE) -C $$i $@; done

distclean: clean

test:
	@for init in $(CURDIR)/debian/*.init; do \
		echo "   validating $${init##*/} ..." ; \
		case "`head -n1 $$init`" in \
			*/bin/bash*) \
				bash -n $$init || exit ; \
				;; \
			*) \
				dash -n $$init || exit ; \
				checkbashisms $$init || exit ; \
				;; \
		esac ; \
	done
