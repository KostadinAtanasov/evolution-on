/* Evoution On plugin
 * Copyright (C) 2008-2012 Lucian Langa <cooly@gnome.eu.org>
 * Copyright (C) 2012-2013 Kostadin Atanasov <pranayama111@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef EVOLUTION_ON_ON_PROPERTIES_H
#define EVOLUTION_ON_ON_PROPERTIES_H

#define GCONF_KEY_NOTIF_ROOT			"/apps/evolution/eplugin/mail-notification/"
#define GCONF_KEY_TRAY_ROOT				"/apps/evolution/eplugin/evolution-on/"

#define NOTIF_SCHEMA					"org.gnome.evolution.plugin.mail-notification"
#define TRAY_SCHEMA						"org.gnome.evolution.plugin.evolution-on"
#define CONF_KEY_HIDDEN_ON_STARTUP		"hidden-on-startup"
#define CONF_KEY_HIDE_ON_MINIMIZE		"hide-on-minimize"
#define CONF_KEY_HIDE_ON_CLOSE			"hide-on-close"
#define CONF_KEY_NOTIFY_ONLY_INBOX		"notify-only-inbox"
#define CONF_KEY_ENABLED_DBUS			"notify-dbus-enabled"
#define CONF_KEY_ENABLED_STATUS			"notify-status-enabled"
#define CONF_KEY_ENABLED_SOUND			"notify-sound-enabled"
#define CONF_KEY_STATUS_NOTIFICATION	"notify-status-notification"

/******************************************************************************
 * Query dconf
 *****************************************************************************/
static gboolean
is_part_enabled(gchar *schema, const gchar *key)
{
	GSettings *settings = g_settings_new(schema);
	gboolean res = g_settings_get_boolean(settings, key);
	g_object_unref(settings);
	return res;
}

static void
set_part_enabled(gchar *schema, const gchar *key, gboolean enable)
{
	GSettings *settings = g_settings_new (schema);
	g_settings_set_boolean (settings, key, enable);
	g_object_unref (settings);
}

/******************************************************************************
 * Callback for configuration widget
 *****************************************************************************/
static void
toggled_hidden_on_startup_cb(GtkWidget *widget, gpointer data)
{
	g_return_if_fail(widget != NULL);
	set_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDDEN_ON_STARTUP,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

static void
toggled_hidde_on_minimize_cb(GtkWidget *widget, gpointer data)
{
	g_return_if_fail(widget != NULL);
	set_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDE_ON_MINIMIZE,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

static void
toggle_hidden_on_close_cb(GtkWidget *widget, gpointer data)
{
	g_return_if_fail(widget != NULL);
	set_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDE_ON_CLOSE,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

/******************************************************************************
 * Sound
 *****************************************************************************/
#define CONF_KEY_SOUND_BEEP			"notify-sound-beep"
#define CONF_KEY_SOUND_USE_THEME	"notify-sound-use-theme"
#define CONF_KEY_SOUND_PLAY_FILE	"notify-sound-play-file"
#define CONF_KEY_SOUND_FILE			"notify-sound-file"

struct _SoundConfigureWidgets {
	GtkWidget	*enable;
	GtkWidget	*beep;
	GtkWidget	*use_theme;
	GtkWidget	*file;
	GtkWidget	*filechooser;
	GtkWidget	*play;
}; /* struct _SoundConfigureWidgets */

struct _SoundNotifyData {
	time_t	last_notify;
	guint	notify_idle_id;
}; /* struct _SoundNotifyData */

static void
do_play_sound(gboolean beep, gboolean use_theme, const gchar *file)
{
	if (!beep) {
#ifdef HAVE_CANBERRA
		if (!use_theme && file && *file) {
			ca_context_play(mailnotification, 0,
					CA_PROP_MEDIA_FILENAME, file,
					NULL);
		} else {
			ca_context_play(mailnotification, 0,
					CA_PROP_EVENT_ID,"message-new-email",
					NULL);
		}
#endif
	} else {
		gdk_beep();
	}
}

static void
sound_file_set_cb(GtkWidget *widget, gpointer data)
{
	gchar *file;
	GSettings *settings;
	g_return_if_fail(widget != NULL);
	settings = g_settings_new("org.gnome.evolution.plugin.mail-notification");
	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
	g_settings_set_string(settings, CONF_KEY_SOUND_FILE, file ? file : "");
	g_object_unref(settings);
	g_free(file);
}

static void
sound_play_cb(GtkWidget *widget, gpointer data)
{
	struct _SoundConfigureWidgets *scw = (struct _SoundConfigureWidgets*)data;
	gchar *file;
	g_return_if_fail(data != NULL);
	if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scw->enable)))
		return;
	file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(scw->filechooser));
	do_play_sound(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scw->beep)),
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(scw->use_theme)),
			file);
	g_free (file);
}

static gboolean
sound_notify_idle_cb(gpointer user_data)
{
	gchar *file;
	GSettings *settings;
	struct _SoundNotifyData *data =(struct _SoundNotifyData*)user_data;
	g_return_val_if_fail(data != NULL, FALSE);
	settings = g_settings_new("org.gnome.evolution.plugin.mail-notification");
	file = g_settings_get_string(settings, CONF_KEY_SOUND_FILE);
	do_play_sound(is_part_enabled(NOTIF_SCHEMA, CONF_KEY_SOUND_BEEP),
			is_part_enabled(NOTIF_SCHEMA, CONF_KEY_SOUND_USE_THEME),
			file);
	g_object_unref(settings);
	g_free(file);
	time(&data->last_notify);
	data->notify_idle_id = 0;
	return FALSE;
}

/******************************************************************************
 * Properties widget
 *****************************************************************************/
static GtkWidget *
get_config_widget_status()
{
#ifdef HAVE_LIBNOTIFY
	GtkWidget *widget;
	GSettings *settings;
	const gchar *text;
	settings = g_settings_new("org.gnome.evolution.plugin.mail-notification");
	text = _("Popup _message together with the icon");
	widget = gtk_check_button_new_with_mnemonic(text);
	gtk_widget_show(widget);
	g_settings_bind(settings, CONF_KEY_STATUS_NOTIFICATION,
			G_OBJECT(widget),
			"active", G_SETTINGS_BIND_DEFAULT);
	return widget;
#endif
	return NULL;
}

static GtkWidget *
get_config_widget_sound()
{
	GtkWidget *vbox;
	GtkWidget *container;
	GtkWidget *master;
	GtkWidget *widget;
	gchar *file;
	GSettings *settings;
	GSList *group = NULL;
	struct _SoundConfigureWidgets *scw;
	const gchar *text;

	settings = g_settings_new("org.gnome.evolution.plugin.mail-notification");
	scw = g_malloc0(sizeof(struct _SoundConfigureWidgets));
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show(vbox);
	container = vbox;

	text = _("_Play sound when new messages arrive");
	widget = gtk_check_button_new_with_mnemonic(text);
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_settings_bind(settings, CONF_KEY_ENABLED_SOUND,
			G_OBJECT(widget),
			"active", G_SETTINGS_BIND_DEFAULT);

	master = widget;
	scw->enable = widget;

	widget = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(widget), 0, 0, 12, 0);
	gtk_box_pack_start(GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show(widget);

	g_object_bind_property (master, "active", widget, "sensitive",
			G_BINDING_SYNC_CREATE);

	container = widget;
	widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add(GTK_CONTAINER (container), widget);
	gtk_widget_show(widget);

	container = widget;
	text = _("_Beep");
	widget = gtk_radio_button_new_with_mnemonic(group, text);
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	g_settings_bind(settings, CONF_KEY_SOUND_BEEP,
			G_OBJECT(widget),
			"active",G_SETTINGS_BIND_DEFAULT);

	scw->beep = widget;
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON (widget));
	text = _("Use sound _theme");
	widget = gtk_radio_button_new_with_mnemonic(group, text);
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	gtk_widget_show(widget);
	g_settings_bind(settings, CONF_KEY_SOUND_USE_THEME,
			G_OBJECT(widget),
			"active", G_SETTINGS_BIND_DEFAULT);

	scw->use_theme = widget;
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
	widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	gtk_widget_show(widget);

	container = widget;
	text = _("Play _file:");
	widget = gtk_radio_button_new_with_mnemonic(group, text);
	gtk_box_pack_start(GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show(widget);
	g_settings_bind(settings, CONF_KEY_SOUND_PLAY_FILE,
			G_OBJECT(widget),
			"active", G_SETTINGS_BIND_DEFAULT);

	scw->file = widget;
	text = _("Select sound file");
	widget = gtk_file_chooser_button_new(text, GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_box_pack_start(GTK_BOX(container), widget, TRUE, TRUE, 0);
	gtk_widget_show(widget);

	scw->filechooser = widget;
	widget = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON (widget),
			gtk_image_new_from_icon_name("media-playback-start",
					GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	gtk_widget_show(widget);

	scw->play = widget;
	file = g_settings_get_string(settings, CONF_KEY_SOUND_FILE);
	if (file && *file)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(scw->filechooser), file);

	g_object_unref (settings);

	g_free(file);

	g_signal_connect(scw->filechooser, "file-set",
			G_CALLBACK(sound_file_set_cb), scw);
	g_signal_connect(scw->play, "clicked",
			G_CALLBACK(sound_play_cb), scw);

	/* to let structure free properly */
	g_object_set_data_full(G_OBJECT(vbox), "scw-data", scw, g_free);

	return vbox;
}

static GtkWidget *
get_original_cfg_widget()
{
	GtkWidget *container;
	GtkWidget *widget;
	GSettings *settings;
	const gchar *text;

	settings = g_settings_new("org.gnome.evolution.plugin.mail-notification");
	widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

	gtk_widget_show(widget);

	container = widget;
	text = _("Notify new messages for _Inbox only");
	widget = gtk_check_button_new_with_mnemonic(text);
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	gtk_widget_show(widget);

	g_settings_bind(settings, CONF_KEY_NOTIFY_ONLY_INBOX,
			G_OBJECT(widget),
			"active", G_SETTINGS_BIND_DEFAULT);

	text = _("Generate a _D-Bus message");
	widget = gtk_check_button_new_with_mnemonic(text);
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);
	gtk_widget_show(widget);

	g_settings_bind(settings, CONF_KEY_ENABLED_DBUS,
			G_OBJECT(widget),
			"active", G_SETTINGS_BIND_DEFAULT);

	widget = get_config_widget_status();
	if (widget)
		gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);

	widget = get_config_widget_sound();
	gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 0);

	return container;
}

static GtkWidget *
get_cfg_widget()
{
	GtkWidget *container, *vbox, *check;
	vbox = gtk_box_new (FALSE, 6);
	gtk_widget_show (vbox);
	container = vbox;

	check = gtk_check_button_new_with_mnemonic(_("Hidden on startup"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
			is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDDEN_ON_STARTUP));
	g_signal_connect(G_OBJECT(check), "toggled",
			G_CALLBACK(toggled_hidden_on_startup_cb), NULL);
	gtk_widget_show(check);
	gtk_box_pack_start(GTK_BOX(container), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_mnemonic(_("Hide on minimize"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (check),
			is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDE_ON_MINIMIZE));
	g_signal_connect(G_OBJECT (check), "toggled",
			G_CALLBACK(toggled_hidde_on_minimize_cb), NULL);
	gtk_widget_show(check);
	gtk_box_pack_start(GTK_BOX(container), check, FALSE, FALSE, 0);

	check = gtk_check_button_new_with_mnemonic(_("Hide on close"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
			is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDE_ON_CLOSE));
	g_signal_connect(G_OBJECT(check), "toggled",
			G_CALLBACK(toggle_hidden_on_close_cb), NULL);
	gtk_widget_show(check);
	gtk_box_pack_start(GTK_BOX(container), check, FALSE, FALSE, 0);

	return container;
}

#endif /* EVOLUTION_ON_ON_PROPERTIES_H */
