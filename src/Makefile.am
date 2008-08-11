bin_PROGRAMS = blame-browse

MARSHALFILES = git-marshal.c git-marshal.h
STAMPFILES = stamp-git-marshal.h stamp-git-enum-types.h
ENUMFILES = git-enum-types.c git-enum-types.h

BUILT_SOURCES = $(MARSHALFILES) $(ENUMFILES)

sources_public_h = \
	git-annotated-source.h \
	git-commit.h \
	git-commit-dialog.h \
	git-common.h \
	git-main-window.h \
	git-reader.h \
	git-source-view.h

sources_private_h = \
	intl.h

blame_browse_SOURCES = \
	git-annotated-source.c \
	git-commit.c \
	git-commit-dialog.c \
	git-common.c \
	git-main-window.c \
	git-reader.c \
	git-source-view.c \
	main.c \
	$(sources_public_h) \
	$(sources_private_h) \
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

git-enum-types.h: stamp-git-enum-types.h
	@true
stamp-git-enum-types.h: $(sources_public_h) Makefile
	( cd $(top_srcdir)/src && \
	  $(GLIB_MKENUMS) \
	  	--template $(top_srcdir)/src/git-enum-types.h.in \
	  $(sources_public_h) ) > xgen-ceth \
	&& (cmp xgen-ceth git-enum-types.h || cp xgen-ceth git-enum-types.h) \
	&& rm -f xgen-ceth \
	&& echo timestamp > $(@F)

git-enum-types.c: git-enum-types.h
	( cd $(top_srcdir)/src && \
	  $(GLIB_MKENUMS) \
	  	--template $(top_srcdir)/src/git-enum-types.c.in \
	  $(sources_public_h) ) > xgen-cetc \
	&& cp xgen-cetc git-enum-types.c \
	&& rm -f xgen-cetc

CLEANFILES = $(STAMPFILES) $(BUILT_SOURCES)

DISTCLEANFILES = \
	$(ENUMFILES) \
	$(MARSHALFILES)

EXTRA_DIST = \
	git-marshal.list
