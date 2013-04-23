/*  Evoution Tray Icon Plugin
 *  Copyright (C) 2008-2012 Lucian Langa <cooly@gnome.eu.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#if EVOLUTION_VERSION <= 30503
#include <gconf/gconf-client.h>
#endif

#if EVOLUTION_VERSION < 30304
#ifdef HAVE_LIBGCONFBRIDGE
#include <libgconf-bridge/gconf-bridge.h>
#else
#include <e-util/gconf-bridge.h>
#endif
#endif

#if EVOLUTION_VERSION < 30704
#include <e-util/e-config.h>
#include <e-util/e-plugin.h>
#include <e-util/e-icon-factory.h>
#else
#include <e-util/e-util.h>
#endif

#if EVOLUTION_VERSION < 22900
#include <mail/em-popup.h>
#else
#include <shell/e-shell.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>
#endif
#if EVOLUTION_VERSION <= 30501
#if EVOLUTION_VERSION >= 30305
#include <libemail-utils/e-account-utils.h>
#include <mail/e-mail.h>
#else
#if EVOLUTION_VERSION >= 29101
#include <e-util/e-account-utils.h>
#include <mail/e-mail.h>
#else
#include <mail/mail-config.h>
#include <mail/mail-session.h>
#endif
#endif
#endif
#if EVOLUTION_VERSION >= 30305
#include <libemail-engine/e-mail-folder-utils.h>
#include <libemail-engine/mail-ops.h>
#else
#include <mail/mail-ops.h>
#endif

#include <mail/em-event.h>
#include <mail/em-folder-tree.h>

#if EVOLUTION_VERSION > 30501
#include <mail/e-mail-reader.h>
#endif

#include <shell/es-event.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#define GCONF_KEY_NOTIF_ROOT                 "/apps/evolution/eplugin/mail-notification/"
#define GCONF_KEY_TRAY_ROOT                 "/apps/evolution/eplugin/evolution-tray/"
#if EVOLUTION_VERSION < 30304
#define GCONF_KEY_HIDDEN_ON_STARTUP	GCONF_KEY_TRAY_ROOT "hidden-on-startup"
#define GCONF_KEY_HIDE_ON_MINIMIZE	GCONF_KEY_TRAY_ROOT "hide-on-minimize"
#define GCONF_KEY_HIDE_ON_CLOSE	GCONF_KEY_TRAY_ROOT "hide-on-close"
#define GCONF_KEY_NOTIFY_ONLY_INBOX     GCONF_KEY_NOTIF_ROOT "notify-only-inbox"
#define GCONF_KEY_ENABLED_DBUS          GCONF_KEY_NOTIF_ROOT "dbus-enabled"
#define GCONF_KEY_ENABLED_STATUS        GCONF_KEY_NOTIF_ROOT "status-enabled"
#define GCONF_KEY_ENABLED_SOUND         GCONF_KEY_NOTIF_ROOT "sound-enabled"
#define GCONF_KEY_STATUS_NOTIFICATION	GCONF_KEY_NOTIF_ROOT "status-notification"
#else
#define NOTIF_SCHEMA			"org.gnome.evolution.plugin.mail-notification"
#define TRAY_SCHEMA			"org.gnome.evolution.plugin.evolution-tray"
#define CONF_KEY_HIDDEN_ON_STARTUP	"hidden-on-startup"
#define CONF_KEY_HIDE_ON_MINIMIZE	"hide-on-minimize"
#define CONF_KEY_HIDE_ON_CLOSE	"hide-on-close"
#define CONF_KEY_NOTIFY_ONLY_INBOX	"notify-only-inbox"
#define CONF_KEY_ENABLED_DBUS		"notify-dbus-enabled"
#define CONF_KEY_ENABLED_STATUS		"notify-status-enabled"
#define CONF_KEY_ENABLED_SOUND		"notify-sound-enabled"
#define CONF_KEY_STATUS_NOTIFICATION	"notify-status-notification"
#endif

static guint status_count = 0;
static gboolean winstatus;
static gboolean winnotify = FALSE;

static gulong shown_first_time_handle = 0;

#ifdef HAVE_LIBNOTIFY
static NotifyNotification *notify = NULL;
#endif

#if EVOLUTION_VERSION < 22900
struct __EShellPrivate {
	/* IID for registering the object on OAF.  */
	char *iid;

	GList *windows;
};
typedef struct __EShellPrivate EShellPrivate;

struct _EShell {
	BonoboObject parent;

	EShellPrivate *priv;
};
typedef struct _EShell EShell;
#endif

#if EVOLUTION_VERSION < 22900
GtkWidget *evo_window;
#else
EShellWindow *evo_window;
#endif
GtkStatusIcon *tray_icon = NULL;

static void remove_notification (void);
static void popup_menu_status (GtkStatusIcon *status_icon,
	guint button, guint activate_time, gpointer user_data);
static void status_icon_activate_cb (void);

gboolean
on_quit_requested(
	EShell *shell,
	EShellQuitReason reason,
	gpointer user_data);

/****************** Configuration *************************/


//Query GConf
static gboolean
#if EVOLUTION_VERSION < 30304
is_part_enabled (const gchar *gconf_key)
#else
is_part_enabled (gchar *schema, const gchar *key)
#endif
{
	/* the part is not enabled by default */
	gboolean res = FALSE;
#if EVOLUTION_VERSION < 30304
	GConfClient *client;
	GConfValue  *is_key;
#else
	GSettings *settings;
#endif

#if EVOLUTION_VERSION < 30304
	client = gconf_client_get_default ();
#else
	settings = g_settings_new (schema);
#endif

#if EVOLUTION_VERSION < 30304
	is_key = gconf_client_get (client, gconf_key, NULL);
	if (is_key) {
		res = gconf_client_get_bool (client, gconf_key, NULL);
		gconf_value_free (is_key);
	}
	g_object_unref (client);
#else
	res = g_settings_get_boolean (settings, key);
	g_object_unref (settings);
#endif


	return res;
}

static void
#if EVOLUTION_VERSION < 30304
//Set GConf
set_part_enabled (const gchar *gconf_key, gboolean enable)
{
	GConfClient *client = gconf_client_get_default ();

	gconf_client_set_bool (client, gconf_key, enable, NULL);
	g_object_unref (client);
}
#else
set_part_enabled (gchar *schema, const gchar *key, gboolean enable)
{
	GSettings *settings = g_settings_new (schema);
	g_settings_set_boolean (settings, key, enable);
	g_object_unref (settings);
}
#endif

//Callback for Configuration Widget
static void
toggled_hidden_on_startup_cb (GtkWidget *widget, gpointer data)
{
	g_return_if_fail (widget != NULL);
#if EVOLUTION_VERSION < 30304
	set_part_enabled (GCONF_KEY_HIDDEN_ON_STARTUP,
#else
	set_part_enabled (TRAY_SCHEMA, CONF_KEY_HIDDEN_ON_STARTUP,
#endif
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
toggled_hidde_on_minimize_cb (GtkWidget *widget, gpointer data)
{
	g_return_if_fail (widget != NULL);
#if EVOLUTION_VERSION < 30304
	set_part_enabled (GCONF_KEY_HIDE_ON_MINIMIZE,
#else
	set_part_enabled (TRAY_SCHEMA, CONF_KEY_HIDE_ON_MINIMIZE,
#endif
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
toggle_hidden_on_close_cb (GtkWidget *widget, gpointer data)
{
	g_return_if_fail (widget != NULL);
#if EVOLUTION_VERSION < 30304
	set_part_enabled (GCONF_KEY_HIDE_ON_CLOSE,
#else
	set_part_enabled (TRAY_SCHEMA, CONF_KEY_HIDE_ON_CLOSE,
#endif
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

#if EVOLUTION_VERSION < 30304
#define GCONF_KEY_SOUND_BEEP            GCONF_KEY_NOTIF_ROOT "sound-beep"
#define GCONF_KEY_SOUND_USE_THEME       GCONF_KEY_NOTIF_ROOT "sound-use-theme"
#define GCONF_KEY_SOUND_PLAY_FILE	GCONF_KEY_NOTIF_ROOT "sound-play-file"
#define GCONF_KEY_SOUND_FILE            GCONF_KEY_NOTIF_ROOT "sound-file"
#else
#define CONF_KEY_SOUND_BEEP		"notify-sound-beep"
#define CONF_KEY_SOUND_USE_THEME	"notify-sound-use-theme"
#define CONF_KEY_SOUND_PLAY_FILE	"notify-sound-play-file"
#define CONF_KEY_SOUND_FILE		"notify-sound-file"
#endif

static void
do_play_sound (gboolean beep, gboolean use_theme, const gchar *file)
{
	if (!beep) {
#ifdef HAVE_CANBERRA
		if (!use_theme && file && *file)
			ca_context_play (mailnotification, 0,
			CA_PROP_MEDIA_FILENAME, file,
			NULL);
		else
			ca_context_play (mailnotification, 0,
			CA_PROP_EVENT_ID,"message-new-email",
			NULL);
#endif
	} else
		gdk_beep ();
}

struct _SoundConfigureWidgets
{
	GtkWidget *enable;
	GtkWidget *beep;
	GtkWidget *use_theme;
	GtkWidget *file;
	GtkWidget *filechooser;
	GtkWidget *play;
};

static void
sound_file_set_cb (GtkWidget *widget, gpointer data)
{
	gchar *file;
#if EVOLUTION_VERSION < 30304
	GConfClient *client;
#else
	GSettings *settings;
#endif

	g_return_if_fail (widget != NULL);

#if EVOLUTION_VERSION < 30304
	client = gconf_client_get_default ();
#else
	settings = g_settings_new ("org.gnome.evolution.plugin.mail-notification");
#endif

	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

#if EVOLUTION_VERSION < 30304
	gconf_client_set_string (client, GCONF_KEY_SOUND_FILE, file ? file : "", NULL);
#else
	g_settings_set_string (settings, CONF_KEY_SOUND_FILE, file ? file : "");
#endif

#if EVOLUTION_VERSION < 30304
	g_object_unref (client);
#else
	g_object_unref (settings);
#endif
	g_free (file);
}

static void
sound_play_cb (GtkWidget *widget, gpointer data)
{
	struct _SoundConfigureWidgets *scw = (struct _SoundConfigureWidgets *) data;
	gchar *file;

	g_return_if_fail (data != NULL);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->enable)))
		return;

	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (scw->filechooser));
	do_play_sound (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->beep)),
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (scw->use_theme)),
			file);
	g_free (file);
}

struct _SoundNotifyData {
	time_t last_notify;
	guint notify_idle_id;
};

static gboolean
sound_notify_idle_cb (gpointer user_data)
{
	gchar *file;
#if EVOLUTION_VERSION < 30304
	GConfClient *client;
#else
	GSettings *settings;
#endif
	struct _SoundNotifyData *data = (struct _SoundNotifyData *) user_data;

	g_return_val_if_fail (data != NULL, FALSE);

#if EVOLUTION_VERSION < 30304
	client = gconf_client_get_default ();
#else
	settings = g_settings_new ("org.gnome.evolution.plugin.mail-notification");
#endif

#if EVOLUTION_VERSION < 30304
	file = gconf_client_get_string (client, GCONF_KEY_SOUND_FILE, NULL);
#else
	file = g_settings_get_string (settings, CONF_KEY_SOUND_FILE);
#endif

	do_play_sound (
#if EVOLUTION_VERSION < 30304
			is_part_enabled (GCONF_KEY_SOUND_BEEP),
			is_part_enabled (GCONF_KEY_SOUND_USE_THEME),
#else
			is_part_enabled (NOTIF_SCHEMA, CONF_KEY_SOUND_BEEP),
			is_part_enabled (NOTIF_SCHEMA, CONF_KEY_SOUND_USE_THEME),
#endif
			file);

#if EVOLUTION_VERSION < 30304
	g_object_unref (client);
#else
	g_object_unref (settings);
#endif
	g_free (file);

	time (&data->last_notify);

	data->notify_idle_id = 0;

	return FALSE;
}

static GtkWidget *
get_config_widget_status (void)
{
	GtkWidget *widget;
#if EVOLUTION_VERSION < 30304
	GConfBridge *bridge;
#else
	GSettings *settings;
#endif
	const gchar *text;

#if EVOLUTION_VERSION < 30304
	bridge = gconf_bridge_get ();
#else
	settings = g_settings_new ("org.gnome.evolution.plugin.mail-notification");
#endif

#ifdef HAVE_LIBNOTIFY
	text = _("Popup _message together with the icon");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION < 30304
	gconf_bridge_bind_property (
		bridge, GCONF_KEY_STATUS_NOTIFICATION,
		G_OBJECT (widget), "active");
#else
	g_settings_bind (settings, CONF_KEY_STATUS_NOTIFICATION,
		G_OBJECT (widget),
		"active", G_SETTINGS_BIND_DEFAULT);
#endif
#endif

	return widget;
}


static GtkWidget *
get_config_widget_sound (void)
{
	GtkWidget *vbox;
	GtkWidget *container;
	GtkWidget *master;
	GtkWidget *widget;
	gchar *file;
#if EVOLUTION_VERSION < 30304
	GConfBridge *bridge;
	GConfClient *client;
#else
	GSettings *settings;
#endif
	GSList *group = NULL;
	struct _SoundConfigureWidgets *scw;
	const gchar *text;

#if EVOLUTION_VERSION < 30304
	bridge = gconf_bridge_get ();
#else
	settings = g_settings_new ("org.gnome.evolution.plugin.mail-notification");
#endif

	scw = g_malloc0 (sizeof (struct _SoundConfigureWidgets));

#if GTK_MAJOR_VERSION < 3
	vbox = gtk_vbox_new (FALSE, 6);
#else
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
#endif
	gtk_widget_show (vbox);

	container = vbox;

	text = _("_Play sound when new messages arrive");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION < 30304
	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_SOUND,
		G_OBJECT (widget), "active");
#else
	g_settings_bind (settings, CONF_KEY_ENABLED_SOUND,
		G_OBJECT (widget),
		"active", G_SETTINGS_BIND_DEFAULT);
#endif

	master = widget;
	scw->enable = widget;

	widget = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 0);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION >= 29101
	g_object_bind_property (
		master, "active",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);
#else
#if EVOLUTION_VERSION >= 22502
	e_binding_new (
		master, "active",
		widget, "sensitive");
#else
	g_warning("add missing properties binding for 2.24\n");
#endif
#endif

	container = widget;

#if GTK_MAJOR_VERSION < 3
	widget = gtk_vbox_new (FALSE, 6);
#else
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
#endif
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	text = _("_Beep");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION < 30304
	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_BEEP,
		G_OBJECT (widget), "active");
#else
	g_settings_bind (settings, CONF_KEY_SOUND_BEEP,
		G_OBJECT (widget),
		"active", G_SETTINGS_BIND_DEFAULT);
#endif

	scw->beep = widget;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	text = _("Use sound _theme");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION < 30304
	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_USE_THEME,
		G_OBJECT (widget), "active");
#else
	g_settings_bind (settings, CONF_KEY_SOUND_USE_THEME,
		G_OBJECT (widget),
		"active", G_SETTINGS_BIND_DEFAULT);
#endif

	scw->use_theme = widget;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

#if GTK_MAJOR_VERSION < 3
	widget = gtk_hbox_new (FALSE, 6);
#else
	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#endif
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Play _file:");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION < 30304
	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_PLAY_FILE,
		G_OBJECT (widget), "active");
#else
	g_settings_bind (settings, CONF_KEY_SOUND_PLAY_FILE,
		G_OBJECT (widget),
		"active", G_SETTINGS_BIND_DEFAULT);
#endif

	scw->file = widget;

	text = _("Select sound file");
	widget = gtk_file_chooser_button_new (
		text, GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	scw->filechooser = widget;

	widget = gtk_button_new ();
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_icon_name (
		"media-playback-start", GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	scw->play = widget;

#if EVOLUTION_VERSION < 30304
	client = gconf_client_get_default ();
	file = gconf_client_get_string (client, GCONF_KEY_SOUND_FILE, NULL);
#else
	file = g_settings_get_string(settings, CONF_KEY_SOUND_FILE);
#endif

	if (file && *file)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (scw->filechooser), file);

#if EVOLUTION_VERSION < 30304
	g_object_unref (client);
#else
	g_object_unref (settings);
#endif
	g_free (file);

	g_signal_connect (
		scw->filechooser, "file-set",
		G_CALLBACK (sound_file_set_cb), scw);
	g_signal_connect (
		scw->play, "clicked",
		G_CALLBACK (sound_play_cb), scw);

	/* to let structure free properly */
	g_object_set_data_full (G_OBJECT (vbox), "scw-data", scw, g_free);

	return vbox;
}

static GtkWidget *
get_original_cfg_widget (void)
{
	GtkWidget *container;
	GtkWidget *widget;
#if EVOLUTION_VERSION < 30304
	GConfBridge *bridge;
#else
	GSettings *settings;
#endif
	const gchar *text;

#if EVOLUTION_VERSION < 30304
	bridge = gconf_bridge_get ();
#else
	settings = g_settings_new ("org.gnome.evolution.plugin.mail-notification");
#endif

#if GTK_MAJOR_VERSION < 3
	widget = gtk_vbox_new (FALSE, 12);
#else
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
#endif
	gtk_widget_show (widget);

	container = widget;

	text = _("Notify new messages for _Inbox only");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION < 30304
	gconf_bridge_bind_property (
		bridge, GCONF_KEY_NOTIFY_ONLY_INBOX,
		G_OBJECT (widget), "active");
#else
	g_settings_bind (settings, CONF_KEY_NOTIFY_ONLY_INBOX,
		G_OBJECT (widget),
		"active", G_SETTINGS_BIND_DEFAULT);
#endif

	text = _("Generate a _D-Bus message");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

#if EVOLUTION_VERSION < 30304
	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_DBUS,
		G_OBJECT (widget), "active");
#else
	g_settings_bind (settings, CONF_KEY_ENABLED_DBUS,
		G_OBJECT (widget),
		"active", G_SETTINGS_BIND_DEFAULT);
#endif

	widget = get_config_widget_status ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	widget = get_config_widget_sound ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	return container;
}

//Configuration Widget
static GtkWidget *
get_cfg_widget (void)
{
	GtkWidget *container, *vbox, *check;

#if GTK_MAJOR_VERSION < 3
	vbox = gtk_vbox_new (FALSE, 6);
#else
	vbox = gtk_box_new (FALSE, 6);
#endif
	gtk_widget_show (vbox);

	container = vbox;

	check = gtk_check_button_new_with_mnemonic (_("Hidden on startup"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
#if EVOLUTION_VERSION < 30304
		is_part_enabled (GCONF_KEY_HIDDEN_ON_STARTUP));
#else
		is_part_enabled (TRAY_SCHEMA, CONF_KEY_HIDDEN_ON_STARTUP));
#endif
	g_signal_connect (G_OBJECT (check),
		"toggled",
		G_CALLBACK (toggled_hidden_on_startup_cb), NULL);
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (container), check, FALSE, FALSE, 0);

	// MINIMIZE
	check = gtk_check_button_new_with_mnemonic (_("Hide on minimize"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
#if EVOLUTION_VERSION < 30304
		is_part_enabled (GCONF_KEY_HIDE_ON_MINIMIZE));
#else
		is_part_enabled (TRAY_SCHEMA, CONF_KEY_HIDE_ON_MINIMIZE));
#endif
	g_signal_connect (G_OBJECT (check),
		"toggled",
		G_CALLBACK (toggled_hidde_on_minimize_cb), NULL);
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (container), check, FALSE, FALSE, 0);

	// CLOSE
	check = gtk_check_button_new_with_mnemonic (_("Hide on close"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
#if EVOLUTION_VERSION < 30304
		is_part_enabled (GCONF_KEY_HIDE_ON_CLOSE));
#else
		is_part_enabled (TRAY_SCHEMA, CONF_KEY_HIDE_ON_CLOSE));
#endif
	g_signal_connect (G_OBJECT (check),
		"toggled",
		G_CALLBACK (toggle_hidden_on_close_cb), NULL);
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (container), check, FALSE, FALSE, 0);

	return container;
}

void gtkut_window_popup(GtkWidget *window)
{
	gint x, y, sx, sy, new_x, new_y;

	g_return_if_fail(window != NULL);
#if GTK_CHECK_VERSION (2,14,0)
	g_return_if_fail(gtk_widget_get_window(window) != NULL);
#else
	g_return_if_fail(window->window != NULL);
#endif

	sx = gdk_screen_width();
	sy = gdk_screen_height();

#if GTK_CHECK_VERSION (2,14,0)
	gdk_window_get_origin(gtk_widget_get_window(window), &x, &y);
#else
	gdk_window_get_origin(window->window, &x, &y);
#endif
	new_x = x % sx; if (new_x < 0) new_x = 0;
	new_y = y % sy; if (new_y < 0) new_y = 0;
	if (new_x != x || new_y != y) {
#if GTK_CHECK_VERSION (2,14,0)
		gdk_window_move(gtk_widget_get_window(window), new_x, new_y);
#else
		gdk_window_move(window->window, new_x, new_y);
#endif
	}

	gtk_window_set_skip_taskbar_hint(
		GTK_WINDOW(window),
		FALSE);
	gtk_window_present(GTK_WINDOW(window));
#ifdef G_OS_WIN32
	/* ensure that the window is displayed at the top */
#if GTK_CHECK_VERSION (2,14,0)
	gdk_window_show(gtk_widget_get_window(window));
#else
	gdk_window_show(window->window);
#endif
#endif
}

//helper method for toggling used on init for hidden on startup and on tray click
static void
toggle_window (void)
{
	GList *p, *pnext;
#if EVOLUTION_VERSION < 22900
	for (p = (gpointer)evo_window; p != NULL; p = pnext) {
		pnext = p->next;

		if (gtk_window_is_active(GTK_WINDOW(p->data))) {
			gtk_widget_hide(GTK_WIDGET(p->data));
			winstatus = TRUE;
		} else {
			gtk_widget_show(GTK_WIDGET(p->data));
			gtkut_window_popup(GTK_WIDGET(p->data));
			winstatus = FALSE;
		}
	}
#else
	#if EVOLUTION_VERSION > 30704
	if (!evo_window) {
		GtkWindow *win = e_shell_get_active_window(e_shell_get_default());
		evo_window = (EShellWindow*)win;
	}
	#endif

	if (gtk_widget_get_visible(GTK_WIDGET(evo_window))) {
		gtk_widget_hide(GTK_WIDGET(evo_window));
		winstatus = TRUE;
	} else {
		gtk_widget_show(GTK_WIDGET(evo_window));
		gtkut_window_popup(GTK_WIDGET(evo_window));
		winstatus = FALSE;
	}
#endif
}

static void
icon_activated (GtkStatusIcon *icon, gpointer pnotify)
{
	status_icon_activate_cb();
	gtk_status_icon_set_from_pixbuf (
		tray_icon,
		e_icon_factory_get_icon (
			"mail-read",
			GTK_ICON_SIZE_SMALL_TOOLBAR));
#if GTK_CHECK_VERSION (2,16,0)
	gtk_status_icon_set_has_tooltip (tray_icon, FALSE);
#endif
	winnotify = FALSE;
}

static gboolean
button_press_cb (
		GtkWidget *widget,
		GdkEventButton *event,
		gpointer data)
{
	if (event->button != 1 || event->type != GDK_2BUTTON_PRESS
		&& winstatus != TRUE && winnotify == TRUE) {
		gtk_window_present(GTK_WINDOW(evo_window));
		return FALSE;
	}
	toggle_window();
	icon_activated(NULL, NULL);
	return TRUE;
}

static void
create_status_icon(void)
{
	if (!tray_icon) {
		tray_icon = gtk_status_icon_new ();
		gtk_status_icon_set_from_pixbuf (
			tray_icon,
			e_icon_factory_get_icon (
				"mail-read",
				GTK_ICON_SIZE_SMALL_TOOLBAR));
		g_signal_connect (
			G_OBJECT (tray_icon),
			"activate",
			G_CALLBACK (icon_activated),
			NULL);
		g_signal_connect (
			G_OBJECT (tray_icon),
			"button-press-event",
			G_CALLBACK (button_press_cb),
			NULL);
		g_signal_connect (
			tray_icon, "popup-menu",
			G_CALLBACK (popup_menu_status), tray_icon);
	}
	gtk_status_icon_set_visible (tray_icon, TRUE);
}

#ifdef HAVE_LIBNOTIFY
static gboolean
notification_callback (gpointer notify)
{
	return (!notify_notification_show (notify, NULL));
}
#endif

static void
do_quit (GtkMenuItem *item, gpointer user_data)
{
#if EVOLUTION_VERSION < 22900
	bonobo_main_quit();
#else
	EShell *shell;
	shell = e_shell_get_default ();
#if EVOLUTION_VERSION >= 23103
	e_shell_quit (shell, E_SHELL_QUIT_ACTION);
#else
	e_shell_quit (shell);
#endif
#endif
}

static void
do_properties (GtkMenuItem *item, gpointer user_data)
{
	GtkWidget *cfg, *ocfg, *dialog, *vbox, *label, *hbox;
	GtkWidget *content_area;
	gchar *text;

	cfg = get_cfg_widget ();
	if (!cfg)
		return;
	ocfg = get_original_cfg_widget ();
	if (!ocfg)
		return;

	text = g_markup_printf_escaped (
		"<span size=\"x-large\">%s</span>",
		_("Evolution Tray"));

#if GTK_MAJOR_VERSION < 3
	vbox = gtk_vbox_new (FALSE, 10);
#else
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
#endif
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), text);
	g_free (text);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_widget_show (vbox);

#if GTK_MAJOR_VERSION < 3
	hbox = gtk_hbox_new (FALSE, 10);
#else
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
#endif
	label = gtk_label_new ("   ");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show_all (hbox);

	gtk_box_pack_start (GTK_BOX (vbox), cfg, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), ocfg, TRUE, TRUE, 0);

	dialog = gtk_dialog_new_with_buttons (
		_("Mail Notification Properties"),
		NULL,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL);

#if GTK_CHECK_VERSION (2,14,0)
	content_area = gtk_dialog_get_content_area(GTK_DIALOG (dialog));
#else
	content_area = GTK_DIALOG (dialog)->vbox;
#endif



#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (dialog, "has-separator", FALSE, NULL);
#endif
	gtk_container_add (GTK_CONTAINER (content_area), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);
	gtk_widget_set_size_request (dialog, 400, -1);
	g_signal_connect_swapped (
		dialog, "response",
		G_CALLBACK (gtk_widget_destroy), dialog);
	gtk_widget_show (dialog);
}

static void
remove_notification (void)
{
#ifdef HAVE_LIBNOTIFY
	if (notify)
		notify_notification_close (notify, NULL);

	notify = NULL;
#endif

	status_count = 0;
}

static void
shown_first_time_cb (GtkWidget *widget, gpointer user_data)
{
	g_signal_handler_disconnect(widget, shown_first_time_handle);
	gtk_widget_hide(widget);
}

static void
status_icon_activate_cb (void)
{
#if EVOLUTION_VERSION >= 22900
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree;
	GtkAction *action;
	GList *list;
	const gchar *uri;
#if (EVOLUTION_VERSION >= 30101)
	GtkApplication *application;
#endif

	shell = e_shell_get_default ();
#if (EVOLUTION_VERSION >= 30301)
	application = GTK_APPLICATION(shell);
	list = gtk_application_get_windows (application);
#else
	list = e_shell_get_watched_windows (shell);
#endif

	/* Find the first EShellWindow in the list. */
	while (list != NULL && !E_IS_SHELL_WINDOW (list->data))
		list = g_list_next (list);

	g_return_if_fail (list != NULL);

	/* Present the shell window. */
	shell_window = E_SHELL_WINDOW (list->data);

	/* Switch to the mail view. */
	shell_view = e_shell_window_get_shell_view (shell_window, "mail");
	action = e_shell_view_get_action (shell_view);
	gtk_action_activate (action);

	/* Select the latest folder with new mail. */
	uri = g_object_get_data (G_OBJECT (tray_icon), "uri");
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	em_folder_tree_set_selected (folder_tree, uri, FALSE);
#endif

	remove_notification ();
}

static GStaticMutex mlock = G_STATIC_MUTEX_INIT;

#ifdef HAVE_LIBNOTIFY
/* Function to check if actions are supported by the notification daemon */
static gboolean
can_support_actions (void)
{
	static gboolean supports_actions = FALSE;
	static gboolean have_checked = FALSE;

	if (!have_checked) {
		GList *caps = NULL;
		GList *c;

		have_checked = TRUE;

		caps = notify_get_server_caps ();
		if (caps != NULL) {
			for (c = caps; c != NULL; c = c->next) {
				if (strcmp ((gchar *)c->data, "actions") == 0) {
					supports_actions = TRUE;
					break;
				}
			}
		}

		g_list_foreach (caps, (GFunc)g_free, NULL);
		g_list_free (caps);
	}

	return supports_actions;
}
#endif

static void
popup_menu_status (GtkStatusIcon *status_icon,
		guint button,
		guint activate_time,
		gpointer user_data)
{
	GtkMenu *menu;
	GtkWidget *item;

	menu = GTK_MENU (gtk_menu_new ());

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	g_signal_connect (
		item, "activate",
		G_CALLBACK (do_properties), NULL);

	item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	g_signal_connect (
		item, "activate",
		G_CALLBACK (do_quit), NULL);

	gtk_menu_popup (GTK_MENU (menu),
		NULL, NULL,
		gtk_status_icon_position_menu,
		user_data,
		button, activate_time);
}

static void
new_notify_status (EMEventTargetFolder *t)
{
	gchar *msg;
	gboolean new_icon = !tray_icon;
#if EVOLUTION_VERSION > 30501
	EShell *shell = e_shell_get_default ();
	CamelStore *store;
	gchar *folder_name;
	EMailBackend *backend;
	EMailSession *session;
	EShellBackend *shell_backend;
#endif
#if EVOLUTION_VERSION >= 30102
	gchar *uri;

#if EVOLUTION_VERSION > 30501
	uri = g_strdup(t->folder_name);
#else
	uri = e_mail_folder_uri_build (t->store, t->folder_name);
#endif
#endif

	g_object_set_data_full (
		G_OBJECT (tray_icon), "uri",
#if EVOLUTION_VERSION >= 30102
		uri,
#else
		g_strdup (t->uri),
#endif
		(GDestroyNotify) g_free);

#if EVOLUTION_VERSION > 30501
		ESource *source = NULL;
		ESourceRegistry *registry;
		const gchar *name;
#else
		EAccount *account;
#endif
#if EVOLUTION_VERSION >= 30102
		const gchar *uid;
		gchar *aname = t->display_name;
#else
		gchar *aname = t->name;
#endif

#if EVOLUTION_VERSION > 30501
		uid = camel_service_get_uid (CAMEL_SERVICE (t->store));
		registry = e_shell_get_registry (shell);
		source = e_source_registry_ref_source (registry,uid);
		name = e_source_get_display_name (source);
#else
#if EVOLUTION_VERSION >= 30102
		uid = camel_service_get_uid (CAMEL_SERVICE (t->store));
		account = e_get_account_by_uid (uid);
#else
#if EVOLUTION_VERSION == 30101
		account = t->account;
#else
#if EVOLUTION_VERSION < 29102
		account = mail_config_get_account_by_source_url (t->uri);
#else
		account = e_get_account_by_source_url (t->uri);
#endif
#endif
#endif
#endif

#if EVOLUTION_VERSION > 30501
		shell_backend = e_shell_get_backend_by_name (shell, "mail");

		backend = E_MAIL_BACKEND (shell_backend);
		session = e_mail_backend_get_session (backend);

		e_mail_folder_uri_parse (
			CAMEL_SESSION (session), t->folder_name,
			&store, &folder_name, NULL);

		if (name != NULL)
			folder_name = g_strdup_printf (
				"%s/%s", name, folder_name);
		else
			folder_name = g_strdup (folder_name);
#else
#if EVOLUTION_VERSION >= 30102
		if (account != NULL)
			folder_name = g_strdup_printf (
				"%s/%s", account->name, t->folder_name);
		else
			folder_name = g_strdup (t->folder_name);
#else
		if (account != NULL)
			folder_name = g_strdup_printf (
				"%s/%s", e_account_get_string (account, E_ACCOUNT_NAME),
				aname);
		else
			folder_name = g_strdup (t->name);
#endif
#endif

		status_count = t->new;

		/* Translators: '%d' is the count of mails received
		 * and '%s' is the name of the folder*/
		msg = g_strdup_printf (ngettext (
			"You have received %d new message\nin %s.",
			"You have received %d new messages\nin %s.",
			status_count), status_count, folder_name);

		g_free(folder_name);

#if EVOLUTION_VERSION >= 22902
		if (t->msg_sender) {
			gchar *tmp, *str;

			/* Translators: "From:" is preceding a new mail
                         * sender address, like "From: user@example.com" */
			str = g_strdup_printf (_("From: %s"), t->msg_sender);
			tmp = g_strconcat (msg, "\n", str, NULL);

			g_free (msg);
			g_free (str);
			msg = tmp;
		}
		if (t->msg_subject) {
			gchar *tmp, *str;

			/* Translators: "Subject:" is preceding a new mail
                         * subject, like "Subject: It happened again" */
			str = g_strdup_printf (_("Subject: %s"), t->msg_subject);
			tmp = g_strconcat (msg, "\n", str, NULL);

			g_free (msg);
			g_free (str);
			msg = tmp;
		}
#endif

#if GTK_CHECK_VERSION (2,16,0)
	gtk_status_icon_set_tooltip_text (tray_icon, msg);
#endif
	gtk_status_icon_set_from_pixbuf (
		tray_icon,
		e_icon_factory_get_icon (
			"mail-unread",
			GTK_ICON_SIZE_SMALL_TOOLBAR));

#ifdef HAVE_LIBNOTIFY
	/* Now check whether we're supposed to send notifications */
#if EVOLUTION_VERSION < 30304
	if (is_part_enabled (GCONF_KEY_STATUS_NOTIFICATION)) {
#else
	if (is_part_enabled (NOTIF_SCHEMA, CONF_KEY_STATUS_NOTIFICATION)) {
#endif
		gchar *safetext;

		safetext = g_markup_escape_text (msg, strlen (msg));
		//don't let the notification pile-up on the notification tray
		notify_notification_close(notify, NULL);
		if (!notify_init ("evolution-mail-notification"))
			fprintf (stderr,"notify init error");

		notify  = notify_notification_new (
			_("New email"), safetext,
#if LIBNOTIFY_VERSION < 7000
			"mail-unread", NULL);
#else
			"mail-unread");
#endif
#if LIBNOTIFY_VERSION < 7000
		notify_notification_attach_to_status_icon (
			notify, tray_icon);
#endif

		/* Check if actions are supported */
		if (can_support_actions ()) {
			notify_notification_set_urgency (
				notify, NOTIFY_URGENCY_NORMAL);
			notify_notification_set_timeout (
				notify, NOTIFY_EXPIRES_DEFAULT);
			g_timeout_add (
				500, notification_callback, notify);
		}
		g_free (safetext);
	}
#endif
	winnotify = TRUE;

	g_free (msg);
}

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl);

#if EVOLUTION_VERSION < 22900
void
org_gnome_evolution_tray_startup(
		void *ep,
		EMPopupTargetSelect *t);

void
org_gnome_evolution_tray_startup(
		void *ep,
		EMPopupTargetSelect *t)
#else
void
org_gnome_evolution_tray_startup(
		void *ep,
		ESEventTargetUpgrade *t);

void
org_gnome_evolution_tray_startup(
		void *ep,
		ESEventTargetUpgrade *t)
#endif
{
	g_print("Evolution-tray plugin enabled.\n");
	g_signal_connect(G_OBJECT (e_shell_get_default()),
		"quit-requested",
		G_CALLBACK (on_quit_requested), NULL);
	create_status_icon();
}

#if EVOLUTION_VERSION < 22900
void org_gnome_evolution_tray_mail_new_notify (void *ep, EMEventTargetFolder *t);
#else
void org_gnome_evolution_tray_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t);
#endif

void
#if EVOLUTION_VERSION < 22900
org_gnome_evolution_tray_mail_new_notify (void *ep, EMEventTargetFolder *t)
#else
org_gnome_evolution_tray_mail_new_notify (EPlugin *ep, EMEventTargetFolder *t)
#endif
{
	new_notify_status (t);
}

#if EVOLUTION_VERSION < 30101
void get_shell(void *ep, ESEventTargetShell *t)
{
	EShell *shell;
#if EVOLUTION_VERSION < 22900
	shell = t->shell;
	EShellPrivate *priv = (EShellPrivate *)shell->priv;
	evo_window = (GtkWidget *)priv->windows;
#endif
#if EVOLUTION_VERSION < 30304
	if (is_part_enabled(GCONF_KEY_HIDDEN_ON_STARTUP)) {
#else
	if (is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDDEN_ON_STARTUP)) {
#endif
		shown_first_time_handle =
			g_signal_connect (G_OBJECT (evo_window),
				"show",
				G_CALLBACK (shown_first_time_cb), NULL);
	}
}
#endif

#if EVOLUTION_VERSION >= 22900
static gboolean window_state_event (GtkWidget *widget, GdkEventWindowState *event)
{
#if EVOLUTION_VERSION < 30304
	if (is_part_enabled(GCONF_KEY_HIDE_ON_MINIMIZE)
#else
	if (is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDE_ON_MINIMIZE)
#endif
	&& event->changed_mask == GDK_WINDOW_STATE_ICONIFIED
	&& event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
		toggle_window ();
	}
	return TRUE;
}

gboolean
on_quit_requested(
	EShell *shell,
	EShellQuitReason reason,
	gpointer user_data)
{
#if EVOLUTION_VERSION < 30304
	if(is_part_enabled (GCONF_KEY_HIDE_ON_CLOSE)
#else
	if(is_part_enabled (TRAY_SCHEMA, CONF_KEY_HIDE_ON_CLOSE)
#endif
	&& (reason == E_SHELL_QUIT_LAST_WINDOW)) {
			e_shell_cancel_quit(e_shell_get_default());
			toggle_window();
		}
	return TRUE;
}

gboolean
e_plugin_ui_init (
	GtkUIManager *ui_manager,
	EShellView *shell_view)
{
	evo_window = e_shell_view_get_shell_window (shell_view);
#if EVOLUTION_VERSION < 30304
	if (is_part_enabled(GCONF_KEY_HIDDEN_ON_STARTUP)) {
#else
	if (is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDDEN_ON_STARTUP)) {
#endif
		shown_first_time_handle =
			g_signal_connect (G_OBJECT (evo_window),
				"show",
				G_CALLBACK (shown_first_time_cb), NULL);
	}

	g_signal_connect (G_OBJECT (evo_window),
		"window-state-event",
		G_CALLBACK (window_state_event), NULL);

	g_signal_connect(G_OBJECT (e_shell_get_default()),
		"quit-requested",
		G_CALLBACK (on_quit_requested), NULL);

	return TRUE;
}
#endif

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	return get_cfg_widget ();
}
