/* Evoution On plugin
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

#if EVOLUTION_VERSION < 30704
#include <e-util/e-config.h>
#include <e-util/e-plugin.h>
#include <e-util/e-icon-factory.h>
#else
#include <e-util/e-util.h>
#endif

#include <shell/e-shell.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#include <libemail-engine/e-mail-folder-utils.h>
#include <libemail-engine/mail-ops.h>

#include <mail/em-event.h>
#include <mail/em-folder-tree.h>

#include <mail/e-mail-reader.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#ifdef HAVE_LIBAPPINDICATOR
#include <libappindicator/app-indicator.h>
#endif

#include "on_properties.h"
#include "on_icon.h"

static gulong show_window_handle = 0;
static gboolean show_window_cb_called = FALSE;
struct OnIcon on_icon = ONICON_NEW;

gboolean
on_quit_requested(EShell *shell, EShellQuitReason reason, gpointer user_data);

void
gtkut_window_popup(GtkWidget *window)
{
	gint x, y, sx, sy, new_x, new_y;

	g_return_if_fail(window != NULL);
	g_return_if_fail(gtk_widget_get_window(window) != NULL);

	sx = gdk_screen_width();
	sy = gdk_screen_height();
	gdk_window_get_origin(gtk_widget_get_window(window), &x, &y);
	new_x = x % sx; if (new_x < 0) new_x = 0;
	new_y = y % sy; if (new_y < 0) new_y = 0;
	if (new_x != x || new_y != y) {
		gdk_window_move(gtk_widget_get_window(window), new_x, new_y);
	}
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), FALSE);
	/* make sure the window is rised */
#ifndef G_OS_WIN32
	GdkWindow *gwindow = gtk_widget_get_window(GTK_WIDGET(window));
	guint32 server_time = gdk_x11_get_server_time(gwindow);
	gtk_window_present_with_time(GTK_WINDOW(window), server_time);
#else
	gtk_window_present(GTK_WINDOW(window));
	/* ensure that the window is displayed at the top */
	gdk_window_show(gtk_widget_get_window(window));
#endif
}

//helper method for toggling used on init for hidden on startup and on tray click
static void
toggle_window()
{
	if (gtk_widget_get_visible(GTK_WIDGET(on_icon.evo_window))) {
		gtk_widget_hide(GTK_WIDGET(on_icon.evo_window));
	} else {
		gtk_widget_show(GTK_WIDGET(on_icon.evo_window));
		gtkut_window_popup(GTK_WIDGET(on_icon.evo_window));
	}

	if (on_icon.winnotify) {
		set_icon(&on_icon, FALSE, _(""));
		on_icon.winnotify = FALSE;
	}
}

#ifdef HAVE_LIBNOTIFY
static gboolean
notification_callback(gpointer data)
{
	struct OnIcon *_onicon = (struct OnIcon*)data;
	return !notify_notification_show(_onicon->notify, NULL);
}
#endif

static void
do_quit(GtkMenuItem *item, gpointer user_data)
{
	EShell *shell;
	shell = e_shell_get_default();
	e_shell_quit(shell, E_SHELL_QUIT_ACTION);
}

static void
do_properties(GtkMenuItem *item, gpointer user_data)
{
	GtkWidget *cfg, *ocfg, *dialog, *vbox, *label, *hbox;
	GtkWidget *content_area;
	gchar *text;

	cfg = get_cfg_widget();
	if (!cfg)
		return;
	ocfg = get_original_cfg_widget();
	if (!ocfg)
		return;

	text = g_markup_printf_escaped("<span size=\"x-large\">%s</span>",
			_("Evolution On"));

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	label = gtk_label_new(NULL);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free (text);

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_widget_show(vbox);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	label = gtk_label_new("   ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX (vbox), cfg, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (vbox), ocfg, TRUE, TRUE, 0);

	dialog = gtk_dialog_new_with_buttons(_("Mail Notification Properties"),
			NULL,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			NULL);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	gtk_container_add(GTK_CONTAINER(content_area), vbox);
	gtk_container_set_border_width(GTK_CONTAINER (vbox), 10);
	gtk_widget_set_size_request(dialog, 400, -1);
	g_signal_connect_swapped(dialog, "response",
			G_CALLBACK(gtk_widget_destroy), dialog);
	gtk_widget_show(dialog);
}

static void
shown_window_cb(GtkWidget *widget, gpointer user_data)
{
	if (!show_window_cb_called) {
		if (is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDDEN_ON_STARTUP)) {
#ifdef HAVE_LIBAPPINDICATOR
			GtkMenu *menu = app_indicator_get_menu(on_icon.appindicator);
			GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
			GtkWidget *item = g_list_nth_data(items, 0);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
#else /* !HAVE_LIBAPPINDICATOR */
			on_icon.toggle_window_func();
#endif /* HAVE_LIBAPPINDICATOR */
		}
		show_window_cb_called = TRUE;
	} else {
#ifdef HAVE_LIBAPPINDICATOR
			/*
			 * Make sure indicator has proper state no matter how we are shown.
			 * We could be shown by user clicking Evolution icon in her
			 * launcher when we are hiding it, in which case this is the only
			 * place we could set the indicator shown state to TRUE.
			 */
			GtkMenu *menu = app_indicator_get_menu(on_icon.appindicator);
			GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
			GtkWidget *item = g_list_nth_data(items, 0);
			if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
				on_icon.external_shown = TRUE;
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
			}
#endif /* HAVE_LIBAPPINDICATOR */
	}
}

static GStaticMutex mlock = G_STATIC_MUTEX_INIT;

#ifdef HAVE_LIBNOTIFY
/* Function to check if actions are supported by the notification daemon */
static gboolean
can_support_actions()
{
	static gboolean supports_actions = FALSE;
	static gboolean have_checked = FALSE;
	if (!have_checked) {
		GList *caps = NULL;
		GList *c;
		have_checked = TRUE;
		caps = notify_get_server_caps();
		if (caps != NULL) {
			for (c = caps; c != NULL; c = c->next) {
				if (strcmp((gchar *)c->data, "actions") == 0) {
					supports_actions = TRUE;
					break;
				}
			}
		}

		g_list_foreach(caps, (GFunc)g_free, NULL);
		g_list_free(caps);
	}
	return supports_actions;
}
#endif

static void
new_notify_status(EMEventTargetFolder *t, struct OnIcon *_onicon)
{
	gchar *msg;

	EShell *shell = e_shell_get_default();
	CamelStore *store;
	gchar *folder_name;
	EMailBackend *backend;
	EMailSession *session;
	EShellBackend *shell_backend;

	_onicon->uri = g_strdup(t->folder_name);

	ESource *source = NULL;
		ESourceRegistry *registry;
	const gchar *name;

	const gchar *uid;
	gchar *aname = t->display_name;

	uid = camel_service_get_uid(CAMEL_SERVICE(t->store));
	registry = e_shell_get_registry(shell);
	source = e_source_registry_ref_source(registry,uid);
	name = e_source_get_display_name(source);

	shell_backend = e_shell_get_backend_by_name(shell, "mail");

	backend = E_MAIL_BACKEND(shell_backend);
	session = e_mail_backend_get_session(backend);

	e_mail_folder_uri_parse (CAMEL_SESSION(session), t->folder_name,
			&store, &folder_name, NULL);

	if (name != NULL)
		folder_name = g_strdup_printf("%s/%s", name, folder_name);
	else
		folder_name = g_strdup(folder_name);

	_onicon->status_count = t->new;

	/* Translators: '%d' is the count of mails received
	 * and '%s' is the name of the folder
	 */
	msg = g_strdup_printf(ngettext(
			"You have received %d new message\nin %s.",
		"	You have received %d new messages\nin %s.",
			_onicon->status_count), _onicon->status_count, folder_name);

	g_free(folder_name);
	if (t->msg_sender) {
		gchar *tmp, *str;

		/* Translators: "From:" is preceding a new mail
		 * sender address, like "From: user@example.com"
		 */
		str = g_strdup_printf(_("From: %s"), t->msg_sender);
		tmp = g_strconcat(msg, "\n", str, NULL);

		g_free(msg);
		g_free(str);
		msg = tmp;
	}
	if (t->msg_subject) {
		gchar *tmp, *str;

		/* Translators: "Subject:" is preceding a new mail
		 * subject, like "Subject: It happened again"
		 */
		str = g_strdup_printf(_("Subject: %s"), t->msg_subject);
		tmp = g_strconcat(msg, "\n", str, NULL);

		g_free(msg);
		g_free(str);
		msg = tmp;
	}

	set_icon(_onicon, TRUE, msg);

#ifdef HAVE_LIBNOTIFY
	/* Now check whether we're supposed to send notifications */
	if (is_part_enabled(NOTIF_SCHEMA, CONF_KEY_STATUS_NOTIFICATION)) {
		gchar *safetext;

		safetext = g_markup_escape_text(msg, strlen(msg));
		//don't let the notification pile-up on the notification tray
		if (_onicon->notify)
			notify_notification_close(_onicon->notify, NULL);
		if (!notify_init("evolution-mail-notification"))
			fprintf(stderr,"notify init error");

#if (LIBNOTIFY_VERSION < 7000)
		_onicon->notify  = notify_notification_new(_("New email"), safetext,
				"mail-unread", NULL);
		notify_notification_attach_to_status_icon(_onicon->notify, tray_icon);
#else /* !(LIBNOTIFY_VERSION < 7000) */
		_onicon->notify  = notify_notification_new(_("New email"), safetext,
				"mail-unread");
#endif /* (LIBNOTIFY_VERSION < 7000) */

		/* Check if actions are supported */
		if (can_support_actions()) {
			notify_notification_set_urgency(_onicon->notify,
					NOTIFY_URGENCY_NORMAL);
			notify_notification_set_timeout(_onicon->notify,
					NOTIFY_EXPIRES_DEFAULT);
			g_timeout_add(500, notification_callback, &on_icon);
		}
		g_free(safetext);
	}
#endif /* HAVE_LIBNOTIFY */
	_onicon->winnotify = TRUE;

	g_free(msg);
}

void
org_gnome_evolution_tray_startup(void *ep)
{
	if (!on_icon.quit_func)
		create_icon(&on_icon, do_properties, do_quit, toggle_window);
}

void
org_gnome_evolution_on_folder_changed(EPlugin *ep, EMEventTargetFolder *t)
{
	/* TODO:
	 * try to update state according what is changed in the folder. Note -
	 * getting the folder may block...
	 */
	if (t->new > 0)
		new_notify_status(t, &on_icon);
}

void
org_gnome_mail_read_notify(EPlugin *ep, EMEventTargetMessage *t)
{
	if (g_atomic_int_compare_and_exchange(&on_icon.status_count, 0, 0))
		return;

	CamelMessageInfo *info = camel_folder_get_message_info(t->folder, t->uid);
	if (info) {
		guint flags = camel_message_info_flags(info);
		if (!(flags & CAMEL_MESSAGE_SEEN)) {
			if (g_atomic_int_dec_and_test(&on_icon.status_count))
				set_icon(&on_icon, FALSE, _(""));
		}
		camel_folder_free_message_info(t->folder, info);
	}
}

static gboolean
window_state_event(GtkWidget *widget, GdkEventWindowState *event)
{
	if (is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDE_ON_MINIMIZE)
			&& (event->changed_mask == GDK_WINDOW_STATE_ICONIFIED)) {

		if (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) {
#ifdef HAVE_LIBAPPINDICATOR
			GtkMenu *menu = app_indicator_get_menu(on_icon.appindicator);
			GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
			GtkWidget *item = g_list_nth_data(items, 0);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
#else /* !HAVE_LIBAPPINDICATOR */
			on_icon.toggle_window_func();
#endif /* HAVE_LIBAPPINDICATOR */
		} else {
			gtk_window_deiconify(GTK_WINDOW(widget));
		}
	}
	return FALSE;
}

gboolean
on_quit_requested(EShell *shell, EShellQuitReason reason, gpointer user_data)
{
	if(is_part_enabled(TRAY_SCHEMA, CONF_KEY_HIDE_ON_CLOSE)
			&& (reason == E_SHELL_QUIT_LAST_WINDOW)) {
		e_shell_cancel_quit(e_shell_get_default());
#ifdef HAVE_LIBAPPINDICATOR
	GtkMenu *menu = app_indicator_get_menu(on_icon.appindicator);
	GList *items = gtk_container_get_children(GTK_CONTAINER(menu));
	GtkWidget *item = g_list_nth_data(items, 0);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
#else /* !HAVE_LIBAPPINDICATOR */
		on_icon.toggle_window_func();
#endif /* HAVE_LIBAPPINDICATOR */
	}
	return TRUE;
}

gboolean
e_plugin_ui_init(GtkUIManager *ui_manager, EShellView *shell_view)
{
	on_icon.evo_window = e_shell_view_get_shell_window(shell_view);

	show_window_handle = g_signal_connect(G_OBJECT(on_icon.evo_window),
			"show", G_CALLBACK(shown_window_cb), &on_icon);

	g_signal_connect(G_OBJECT(on_icon.evo_window), "window-state-event",
			G_CALLBACK(window_state_event), NULL);

	g_signal_connect(G_OBJECT(e_shell_get_default()), "quit-requested",
			G_CALLBACK(on_quit_requested), NULL);

	if (!on_icon.quit_func)
		create_icon(&on_icon, do_properties, do_quit, toggle_window);

	return TRUE;
}

GtkWidget *
e_plugin_lib_get_configure_widget(EPlugin *epl)
{
	return get_cfg_widget();
}
