INCLUDES =                                              	\
	-I$(EVOLUTION_SOURCE)                           	\
	-I$(top_srcdir)                                 	\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""      	\
	-DLOCALEDIR="\"$(LOCALEDIR)\""                  	\
	-DEVOLUTION_VERSION_STRING=\"$(EVOLUTION_EXEC_VERSION)\"\
	-DEVOLUTION_VERSION=$(evolution_version_int)            \
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"                  \
	-DEVOLUTION_ICONDIR=\""$(ICON_DIR)"\"                   \
	$(TRAY_EPLUGIN_CFLAGS)					\
	$(GTKHTML_EDITOR_CFLAGS)


%.eplug.in: %.eplug.xml
	LC_ALL=C $(INTLTOOL_MERGE) -x -u /tmp $< $@

%.eplug: %.eplug.in
	sed -e "s%+PLUGIN_INSTALL_DIR+%$(PLUGIN_INSTALL_DIR)%" \
	-e "s%+GETTEXT_PACKAGE+%$(GETTEXT_PACKAGE)%" \
	-e "s%+LOCALEDIR+%$(LOCALEDIR)%" \
	-e "s%+ICON_DIR+%$(ICON_DIR)%" \
	-e "s%+VERSION+%$(VERSION)%" \
	$< > $@

plugindir = $(PLUGIN_INSTALL_DIR)
plugin_DATA = 					\
		org-gnome-evolution-tray.eplug

plugin_LTLIBRARIES = liborg-gnome-evolution-tray.la

liborg_gnome_evolution_tray_la_SOURCES = tray.c
liborg_gnome_evolution_tray_la_LIBADD = $(LIBGCONFBRIDGE_LIBS)
liborg_gnome_evolution_tray_la_LDFLAGS = -module -avoid-version -rpath $(PLUGIN_INSTALL_DIR)

EXTRA_DIST = 	org-gnome-evolution-tray.eplug.xml	\
		$(schema_in_files)

BUILT_SOURCES = org-gnome-evolution-tray.eplug

CLEANFILES = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)
