/*  Evoution Tray Icon Plugin
 *  Copyright (C) 2008-2009 Lucian Langa <cooly@gnome.eu.org> 
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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include <gconf/gconf-client.h>

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
#include <mail/mail-session.h>
#endif
#include <mail/mail-ops.h>
#include <e-util/e-plugin.h>
#include <glade/glade.h>

#include <e-util/e-icon-factory.h>
#include <shell/es-event.h>

#define GCONF_KEY_ROOT                 "/apps/evolution/eplugin/tray/"
#define GCONF_KEY_HIDDEN_ON_STARTUP GCONF_KEY_ROOT "hidden-on-startup"

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

//Configuration Widget
static GtkWidget *
get_cfg_widget (void)
{
	GtkWidget *cfg, *vbox, *check;

	vbox = gtk_vbox_new (FALSE, 6);
	check = gtk_check_button_new_with_mnemonic (_("Hidden on startup"));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), is_part_enabled (GCONF_KEY_HIDDEN_ON_STARTUP));
	g_signal_connect (G_OBJECT (check), "toggled", G_CALLBACK (toggled_hidden_on_startup_cb), NULL);
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
toggle_window (int just_minimize)
{
	GList *p, *pnext;
#if EVOLUTION_VERSION < 22900
	for (p = (gpointer)evo_window; p != NULL; p = pnext) {
		pnext = p->next;

		if (just_minimize || gtk_window_is_active(GTK_WINDOW(p->data))) {
			gtk_window_iconify(GTK_WINDOW(p->data));
			gtk_window_set_skip_taskbar_hint(GTK_WINDOW(p->data), TRUE);
		} else {
			gtk_window_iconify(GTK_WINDOW(p->data));
			gtkut_window_popup(GTK_WIDGET(p->data));
			gtk_window_set_skip_taskbar_hint(GTK_WINDOW(p->data), FALSE);
		}
	}
#else
	if (gtk_window_is_active(GTK_WINDOW(evo_window))) {
		gtk_window_iconify(GTK_WINDOW(evo_window));
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(evo_window), TRUE);
	} else {
		gtk_window_iconify(GTK_WINDOW(evo_window));
		gtkut_window_popup(GTK_WIDGET(evo_window));
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(evo_window), FALSE);
	}
#endif
}

static void
icon_activated (GtkStatusIcon *icon, gpointer pnotify)
{
	toggle_window(FALSE);
}

static void
create_status_icon(void)
{
	if (!tray_icon) {
		tray_icon = gtk_status_icon_new ();
		gtk_status_icon_set_from_pixbuf (
			tray_icon, 
			e_icon_factory_get_icon (
				"mail-send-receive",
				GTK_ICON_SIZE_SMALL_TOOLBAR));
		g_signal_connect (
			G_OBJECT (tray_icon),
			"activate",
			G_CALLBACK (icon_activated),
			NULL);
	}
	gtk_status_icon_set_visible (tray_icon, TRUE);
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

void get_shell(void *ep, ESEventTargetShell *t)
{
	EShell *shell;
#if EVOLUTION_VERSION < 22900
	shell = t->shell;
	EShellPrivate *priv = (EShellPrivate *)shell->priv;
	evo_window = (GtkWidget *)priv->windows;
#endif
	if(is_part_enabled(GCONF_KEY_HIDDEN_ON_STARTUP)){
		toggle_window(TRUE);
	}
}

#if EVOLUTION_VERSION >= 22900
gboolean
e_plugin_ui_init (
	GtkUIManager *ui_manager,
	EShellView *shell_view)
{
	evo_window = e_shell_view_get_shell_window (shell_view);
	return TRUE;
}
#endif

GtkWidget *
e_plugin_lib_get_configure_widget (EPlugin *epl)
{
	return get_cfg_widget ();
}

