/*  Evoution Tray Icon Plugin
 *  Copyright (C) 2008-2010 Lucian Langa <cooly@gnome.eu.org>
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

#include <gconf/gconf-client.h>
#ifdef HAVE_LIBGCONFBRIDGE
#include <libgconf-bridge/gconf-bridge.h>
#else
#include <e-util/gconf-bridge.h>
#endif

#include <e-util/e-config.h>

#if EVOLUTION_VERSION < 22900
#include <mail/em-popup.h>
#else
#include <shell/e-shell.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>
#endif
#if EVOLUTION_VERSION >= 29101
#include <mail/e-mail-session.h>
#else
#include <mail/mail-config.h>
#include <mail/mail-session.h>
#endif
#include <mail/mail-ops.h>
#include <e-util/e-plugin.h>

#include <mail/em-event.h>
#include <mail/em-folder-tree.h>


#include <e-util/e-icon-factory.h>
#include <shell/es-event.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#define GCONF_KEY_NOTIF_ROOT                 "/apps/evolution/eplugin/mail-notification/"
#define GCONF_KEY_TRAY_ROOT                 "/apps/evolution/eplugin/tray/"
#define GCONF_KEY_HIDDEN_ON_STARTUP GCONF_KEY_TRAY_ROOT "hidden-on-startup"
#define GCONF_KEY_STATUS_NOTIFICATION GCONF_KEY_NOTIF_ROOT "status-notification"
#define GCONF_KEY_NOTIFY_ONLY_INBOX     GCONF_KEY_NOTIF_ROOT "notify-only-inbox"
#define GCONF_KEY_ENABLED_DBUS          GCONF_KEY_NOTIF_ROOT "dbus-enabled"
#define GCONF_KEY_ENABLED_STATUS        GCONF_KEY_NOTIF_ROOT "status-enabled"
#define GCONF_KEY_ENABLED_SOUND         GCONF_KEY_NOTIF_ROOT "sound-enabled"

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

/****************** Configuration *************************/


//Query GConf
static gboolean
is_part_enabled (const gchar *gconf_key)
{
	/* the part is not enabled by default */
	gboolean res = FALSE;
	GConfClient *client;
	GConfValue  *is_key;

	client = gconf_client_get_default ();

	is_key = gconf_client_get (client, gconf_key, NULL);
	if (is_key) {
		res = gconf_client_get_bool (client, gconf_key, NULL);
		gconf_value_free (is_key);
	}

	g_object_unref (client);

	return res;
}

//Set GConf
static void
set_part_enabled (const gchar *gconf_key, gboolean enable)
{
	GConfClient *client;

	client = gconf_client_get_default ();

	gconf_client_set_bool (client, gconf_key, enable, NULL);
	g_object_unref (client);
}

//Callback for Configuration Widget
static void
toggled_hidden_on_startup_cb (GtkWidget *widget, gpointer data)
{
	g_return_if_fail (widget != NULL);
	set_part_enabled (GCONF_KEY_HIDDEN_ON_STARTUP, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

#define GCONF_KEY_SOUND_BEEP            GCONF_KEY_NOTIF_ROOT "sound-beep"
#define GCONF_KEY_SOUND_FILE            GCONF_KEY_NOTIF_ROOT "sound-file"
#define GCONF_KEY_SOUND_PLAY_FILE       GCONF_KEY_NOTIF_ROOT "sound-play-file"
#define GCONF_KEY_SOUND_USE_THEME       GCONF_KEY_NOTIF_ROOT "sound-use-theme"

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
	GConfClient *client;

	g_return_if_fail (widget != NULL);

	client = gconf_client_get_default ();
	file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

	gconf_client_set_string (client, GCONF_KEY_SOUND_FILE, file ? file : "", NULL);

	g_object_unref (client);
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
	GConfClient *client;
	struct _SoundNotifyData *data = (struct _SoundNotifyData *) user_data;

	g_return_val_if_fail (data != NULL, FALSE);

	client = gconf_client_get_default ();
	file = gconf_client_get_string (client, GCONF_KEY_SOUND_FILE, NULL);

	do_play_sound (is_part_enabled (GCONF_KEY_SOUND_BEEP),
			is_part_enabled (GCONF_KEY_SOUND_USE_THEME),
			file);

	g_object_unref (client);
	g_free (file);

	time (&data->last_notify);

	data->notify_idle_id = 0;

	return FALSE;
}

static GtkWidget *
get_config_widget_status (void)
{
	GtkWidget *vbox;
	GtkWidget *master;
	GtkWidget *container;
	GtkWidget *widget;
	GConfBridge *bridge;
	const gchar *text;

	bridge = gconf_bridge_get ();

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	container = vbox;

	text = _("Show icon in _notification area");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_STATUS,
		G_OBJECT (widget), "active");

	master = widget;

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

	widget = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

#ifdef HAVE_LIBNOTIFY
	text = _("Popup _message together with the icon");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_STATUS_NOTIFICATION,
		G_OBJECT (widget), "active");
#endif

	return vbox;
}


static GtkWidget *
get_config_widget_sound (void)
{
	GtkWidget *vbox;
	GtkWidget *container;
	GtkWidget *master;
	GtkWidget *widget;
	gchar *file;
	GConfBridge *bridge;
	GConfClient *client;
	GSList *group = NULL;
	struct _SoundConfigureWidgets *scw;
	const gchar *text;

	bridge = gconf_bridge_get ();

	scw = g_malloc0 (sizeof (struct _SoundConfigureWidgets));

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	container = vbox;

	text = _("_Play sound when new messages arrive");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_SOUND,
		G_OBJECT (widget), "active");

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

	widget = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	text = _("_Beep");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_BEEP,
		G_OBJECT (widget), "active");

	scw->beep = widget;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	text = _("Use sound _theme");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_USE_THEME,
		G_OBJECT (widget), "active");

	scw->use_theme = widget;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	widget = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	container = widget;

	text = _("Play _file:");
	widget = gtk_radio_button_new_with_mnemonic (group, text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_SOUND_PLAY_FILE,
		G_OBJECT (widget), "active");

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

	client = gconf_client_get_default ();
	file = gconf_client_get_string (client, GCONF_KEY_SOUND_FILE, NULL);

	if (file && *file)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (scw->filechooser), file);

	g_object_unref (client);
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
	GConfBridge *bridge;
	const gchar *text;

	bridge = gconf_bridge_get ();

	widget = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (widget);

	container = widget;

	text = _("Notify new messages for _Inbox only");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_NOTIFY_ONLY_INBOX,
		G_OBJECT (widget), "active");

	text = _("Generate a _D-Bus message");
	widget = gtk_check_button_new_with_mnemonic (text);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	gconf_bridge_bind_property (
		bridge, GCONF_KEY_ENABLED_DBUS,
		G_OBJECT (widget), "active");

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
	GtkWidget *cfg, *vbox, *check;

	vbox = gtk_vbox_new (FALSE, 6);
	check = gtk_check_button_new_with_mnemonic (_("Hidden on startup"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check),
		is_part_enabled (GCONF_KEY_HIDDEN_ON_STARTUP));
	g_signal_connect (G_OBJECT (check),
		"toggled",
		G_CALLBACK (toggled_hidden_on_startup_cb), NULL);
	gtk_widget_show (check);
	gtk_box_pack_start (GTK_BOX (vbox), check, FALSE, FALSE, 0);

	gtk_widget_show (vbox);

	return vbox;
}

void gtkut_window_popup(GtkWidget *window)
{
	gint x, y, sx, sy, new_x, new_y;

	g_return_if_fail(window != NULL);
	g_return_if_fail(window->window != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();

	gdk_window_get_origin(window->window, &x, &y);
	new_x = x % sx; if (new_x < 0) new_x = 0;
	new_y = y % sy; if (new_y < 0) new_y = 0;
	if (new_x != x || new_y != y)
		gdk_window_move(window->window, new_x, new_y);

	gtk_window_set_skip_taskbar_hint(
		GTK_WINDOW(window),
		FALSE);
	gtk_window_present(GTK_WINDOW(window));
#ifdef G_OS_WIN32
	/* ensure that the window is displayed at the top */
	gdk_window_show(window->window);
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
	if (gtk_window_is_active(GTK_WINDOW(evo_window))) {
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
#if GTK_VERSION >= 2016000
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
			G_CALLBACK (popup_menu_status), NULL);
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

	vbox = gtk_vbox_new (FALSE, 10);
	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_markup (GTK_LABEL (label), text);
	g_free (text);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_widget_show (vbox);

	hbox = gtk_hbox_new (FALSE, 10);
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

#if GTK_VERSION >= 2014000
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
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

	shell = e_shell_get_default ();
	list = e_shell_get_watched_windows (shell);

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

	g_object_ref_sink (menu);
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, button, activate_time);
	g_object_unref (menu);
}

static void
new_notify_status (EMEventTargetFolder *t)
{
	gchar *msg;
	gboolean new_icon = !tray_icon;

	g_object_set_data_full (
		G_OBJECT (tray_icon), "uri",
		g_strdup (t->uri), (GDestroyNotify) g_free);

	if (!status_count) {
		EAccount *account;
		gchar *name = t->name;

#if EVOLUTION_VERSION < 29102
		account = mail_config_get_account_by_source_url (t->uri);
#else
		account = e_get_account_by_source_url (t->uri);
#endif

		if (account != NULL) {
			name = g_strdup_printf (
				"%s/%s", e_account_get_string (
				account, E_ACCOUNT_NAME), name);
		}

		status_count = t->new;

		/* Translators: '%d' is the count of mails received
		 * and '%s' is the name of the folder*/
		msg = g_strdup_printf (ngettext (
			"You have received %d new message\nin %s.",
			"You have received %d new messages\nin %s.",
			status_count), status_count, name);

		if (name != t->name)
			g_free (name);

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
	} else {
		status_count += t->new;
		msg = g_strdup_printf (ngettext (
			"You have received %d new message.",
			"You have received %d new messages.",
			status_count), status_count);
	}

#if GTK_VERSION >= 2016000
	gtk_status_icon_set_tooltip_text (tray_icon, msg);
#endif
	gtk_status_icon_set_from_pixbuf (
		tray_icon,
		e_icon_factory_get_icon (
			"mail-unread",
			GTK_ICON_SIZE_SMALL_TOOLBAR));

#ifdef HAVE_LIBNOTIFY
	/* Now check whether we're supposed to send notifications */
	if (is_part_enabled (GCONF_KEY_STATUS_NOTIFICATION)) {
		gchar *safetext;

		safetext = g_markup_escape_text (msg, strlen (msg));
		if (notify) {
			notify_notification_update (
				notify, _("New email"),
				safetext, "mail-unread");
		} else {
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

void get_shell(void *ep, ESEventTargetShell *t)
{
	EShell *shell;
#if EVOLUTION_VERSION < 22900
	shell = t->shell;
	EShellPrivate *priv = (EShellPrivate *)shell->priv;
	evo_window = (GtkWidget *)priv->windows;
#endif
	if (is_part_enabled(GCONF_KEY_HIDDEN_ON_STARTUP)) {
		shown_first_time_handle =
			g_signal_connect (G_OBJECT (evo_window),
				"show",
				G_CALLBACK (shown_first_time_cb), NULL);
	}
}

#if EVOLUTION_VERSION >= 22900
gboolean
e_plugin_ui_init (
	GtkUIManager *ui_manager,
	EShellView *shell_view)
{
	evo_window = e_shell_view_get_shell_window (shell_view);
	if (is_part_enabled(GCONF_KEY_HIDDEN_ON_STARTUP)) {
		shown_first_time_handle =
			g_signal_connect (G_OBJECT (evo_window),
				"show",
				G_CALLBACK (shown_first_time_cb), NULL);
	}
	return TRUE;
}
#endif

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	return get_cfg_widget ();
}

