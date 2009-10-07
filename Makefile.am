SUBDIRS = src

## We dist autogen.sh since this is an example program
## Real-world programs do not need to distribute autogen.sh
## But there is no harm in it, and you probably should.

EXTRA_DIST = \
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	po/POTFILES.in		\
	po/Makefile.in.in	

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update \
  po/POTFILES \
  po/Makefile.in \
  po/Makefile \
  po/stamp-it

-include $(top_srcdir)/git.mk
