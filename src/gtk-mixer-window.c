/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2012 Guido Berhoerster <guido+xfce@berhoerster.name>
 * Copyright (c) 2020 - 2021 Rozhuk Ivan <rozhuk.im@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>

#include "gtk-mixer.h"


typedef struct gtk_mixer_window_s {
	GtkWidget *window;

	/* Current window state. */
	gint width;
	gint height;

	GtkWidget *soundcard_combo;
	GtkWidget *makedef_button;

	/* Active mixer control set. */
	GtkWidget *mixer_container;
} gm_window_t, *gm_window_p;


static void
gtk_mixer_window_soundcard_changed(GtkWidget *combo __unused,
    gpointer user_data) {
	gm_window_p gm_win = user_data;
	char title[256];
	gmp_dev_p dev;

	/* Update mixer controls for the active sound card */
	dev = gtk_mixer_devs_combo_cur_get(gm_win->soundcard_combo);
	if (NULL != dev) {
		snprintf(title, sizeof(title),
		    "%s - %s", _("Audio Mixer"),
		    dev->description);
		gtk_window_set_title(GTK_WINDOW(gm_win->window), title);
		gtk_widget_set_sensitive(gm_win->makedef_button,
		    (dev->plugin->descr->can_set_default_device &&
		     0 == gmp_dev_is_default(dev)));
	} else {
		gtk_window_set_title(GTK_WINDOW(gm_win->window), _("Audio Mixer"));
		gtk_widget_set_sensitive(gm_win->makedef_button, FALSE);
	}
	/* Update the MixerContainer containing the controls. */
	gtk_mixer_container_dev_set(gm_win->mixer_container, dev);
}

static void
gtk_mixer_makedef_button(GtkButton *button __unused, gpointer user_data) {
	gm_window_p gm_win = user_data;
	gmp_dev_p dev;

	dev = gtk_mixer_devs_combo_cur_get(gm_win->soundcard_combo);
	if (NULL == dev)
		return;
	gmp_dev_set_default(dev);
	gtk_mixer_devs_combo_update(gm_win->soundcard_combo);
}

static void
gtk_mixer_window_destroy(GtkWidget *window __unused, gpointer user_data) {
	gm_window_p gm_win = user_data;

	//g_object_set(G_OBJECT(gm_win->preferences), "window-width",
	//    gm_win->current_width, "window-height", gm_win->current_height,
	//    NULL);

	free(gm_win);
}

GtkWidget *
gtk_mixer_window_create(void) {
	gm_window_p gm_win;
	GtkWidget *label, *vbox, *hbox, *mixer_frame;

	gm_win = calloc(1, sizeof(gm_window_t));
	if (NULL == gm_win)
		return (NULL);
	gm_win->window = gtk_dialog_new();
	g_object_set_data(G_OBJECT(gm_win->window), "__gtk_mixer_window",
	    (void*)gm_win);
	g_signal_connect(gm_win->window, "destroy",
	    G_CALLBACK(gtk_main_quit), gm_win);

	gm_win->height = 350;
	gm_win->width = 500;
#if 0
	g_object_get(gm_win->preferences, "window-width",
	    &gm_win->width, "window-height", &gm_win->height,
#endif

	/* Configure the main window. */
	gtk_window_set_type_hint(GTK_WINDOW(gm_win->window),
	    GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_icon_name(GTK_WINDOW(gm_win->window), APP_ICON_NAME);
	gtk_window_set_title(GTK_WINDOW(gm_win->window), _("Audio Mixer"));
	gtk_window_set_default_size(GTK_WINDOW(gm_win->window),
	    gm_win->width, gm_win->height);
	gtk_window_set_position(GTK_WINDOW(gm_win->window), GTK_WIN_POS_CENTER);
	g_signal_connect(gm_win->window, "destroy",
	    G_CALLBACK(gtk_mixer_window_destroy), gm_win);

	vbox = gtk_dialog_get_content_area(GTK_DIALOG(gm_win->window));
	gtk_widget_show(vbox);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (BORDER_WIDTH * 2));
	gtk_container_set_border_width(GTK_CONTAINER(hbox), BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show(hbox);

	/* Top line. */
	label = gtk_label_new_with_mnemonic(_("Sound _card:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	gm_win->soundcard_combo = gtk_mixer_devs_combo_create();
	g_signal_connect(gm_win->soundcard_combo, "changed",
	    G_CALLBACK(gtk_mixer_window_soundcard_changed), gm_win);
	gtk_box_pack_start(
	    GTK_BOX(hbox), gm_win->soundcard_combo, TRUE, TRUE, 0);
	gtk_label_set_mnemonic_widget(
	    GTK_LABEL(label), gm_win->soundcard_combo);
	gtk_widget_show(gm_win->soundcard_combo);
	
	gm_win->makedef_button = gtk_button_new_from_icon_name("audio-card",
	    GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(gm_win->makedef_button, "Make default");
	gtk_box_pack_start(GTK_BOX(hbox), gm_win->makedef_button,
	    FALSE, TRUE, 0);
	g_signal_connect(gm_win->makedef_button, "clicked",
	    G_CALLBACK(gtk_mixer_makedef_button), gm_win);
	gtk_widget_show(gm_win->makedef_button);

	/* Frame with tabs. */
	mixer_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(mixer_frame),
	    GTK_SHADOW_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(mixer_frame),
	    BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(vbox), mixer_frame, TRUE, TRUE, 0);
	gtk_widget_show(mixer_frame);

	gm_win->mixer_container = gtk_mixer_container_create();
	gtk_container_add(GTK_CONTAINER(mixer_frame),
	    gm_win->mixer_container);
	gtk_widget_show(gm_win->mixer_container);

	/* Update mixer controls for the active sound card. */
	gtk_mixer_window_soundcard_changed(NULL, gm_win);

	return (gm_win->window);
}

gulong
gtk_mixer_window_connect_dev_changed(GtkWidget *window,
    GCallback c_handler, gpointer data) {
	gm_window_p gm_win = g_object_get_data(G_OBJECT(window),
	    "__gtk_mixer_window");

	if (NULL == gm_win)
		return (0);

	return (g_signal_connect(gm_win->soundcard_combo, "changed",
	    c_handler, data));
}

gmp_dev_p
gtk_mixer_window_dev_cur_get(GtkWidget *window) {
	gm_window_p gm_win = g_object_get_data(G_OBJECT(window),
	    "__gtk_mixer_window");

	if (NULL == gm_win)
		return (0);
	return (gtk_mixer_devs_combo_cur_get(gm_win->soundcard_combo));
}

void
gtk_mixer_window_dev_cur_set(GtkWidget *window, gmp_dev_p dev) {
	gm_window_p gm_win = g_object_get_data(G_OBJECT(window),
	    "__gtk_mixer_window");

	if (NULL == gm_win)
		return;
	gtk_mixer_devs_combo_cur_set(gm_win->soundcard_combo, dev);
}

void
gtk_mixer_window_dev_list_update(GtkWidget *window, gmp_dev_list_p dev_list) {
	gmp_dev_p dev;
	gm_window_p gm_win = g_object_get_data(G_OBJECT(window),
	    "__gtk_mixer_window");

	if (NULL == gm_win)
		return;

	/* Update dev list only if it can be changed. */
	dev = gtk_mixer_devs_combo_cur_get(gm_win->soundcard_combo);
	if (NULL != dev_list) {
		gtk_mixer_devs_combo_dev_list_set(gm_win->soundcard_combo,
		    dev_list);
	}
	if (NULL != dev &&
	    dev->plugin->descr->can_set_default_device) {
		gtk_mixer_devs_combo_update(gm_win->soundcard_combo);
		gtk_widget_set_sensitive(gm_win->makedef_button,
		    (0 == gmp_dev_is_default(dev)));
	}
}

void
gtk_mixer_window_lines_update(GtkWidget *window) {
	gm_window_p gm_win = g_object_get_data(G_OBJECT(window),
	    "__gtk_mixer_window");

	if (NULL == gm_win)
		return;
	gtk_mixer_container_update(gm_win->mixer_container);
}
