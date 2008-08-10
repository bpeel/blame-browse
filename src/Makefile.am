bin_PROGRAMS = blame-browse

MARSHALFILES = git-marshal.c git-marshal.h
STAMPFILES = stamp-git-marshal.h

BUILT_SOURCES = $(MARSHALFILES)

blame_browse_SOURCES = \
	git-annotated-source.c \
	git-annotated-source.h \
	git-commit.c \
	git-commit.h \
	git-common.c \
	git-common.h \
	git-reader.h \
	git-reader.c \
	main.c \
	$(BUILT_SOURCES)

INCLUDES = @DEPS_CFLAGS@

blame_browse_LDFLAGS = @DEPS_LIBS@

git-marshal.h: stamp-git-marshal.h
	@true
stamp-git-marshal.h: git-marshal.list
	$(GLIB_GENMARSHAL) \
		--prefix=_git_marshal \
		--header \
	  $(top_srcdir)/src/git-marshal.list > xgen-cmh \
	&& (cmp -s xgen-cmh git-marshal.h || cp xgen-cmh git-marshal.h ) \
	&& rm -f xgen-cmh \
	&& echo timestamp > $(@F)

git-marshal.c: git-marshal.h Makefile
	( echo "#include \"git-marshal.h\"" ; \
	  $(GLIB_GENMARSHAL) \
	  	--prefix=_git_marshal \
		--body \
	  $(top_srcdir)/src/git-marshal.list --body ) >> xgen-cmc \
	&& cp xgen-cmc git-marshal.c \
	&& rm -f xgen-cmc

DISTCLEANFILES = \
	$(MARSHALFILES)

EXTRA_DIST = \
	git-marshal.list
