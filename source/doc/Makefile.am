if WITH_DOXYGEN

doxyfile.stamp:
	$(DOXYGEN) Doxyfile
	: > doxyfile.stamp

all-local: doxyfile.stamp

clean-local:
	-rm -rf html

distclean-local: clean-local

.PHONY: all-local clean-local distclean-local

CLEANFILES = doxyfile.stamp

endif
