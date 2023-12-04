/*-
 * Copyright (c) 2021-2023 Rozhuk Ivan <rozhuk.im@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Rozhuk Ivan <rozhuk.im@gmail.com>
 *
 */


#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>

#include "gtk-mixer.h"


typedef struct gtk_mixer_tray_icon_s {
	GtkStatusIcon *status_icon;
	GtkMenu *menu;
	const char *icon_name;
	GtkWidget *main_window;
	gmp_dev_p dev;
	gmp_dev_line_p dev_line;
} gm_tray_icon_t, *gm_tray_icon_p;



static gboolean
gtk_mixer_tray_icon_scroll(GtkStatusIcon *status_icon,
    GdkEventScroll *event, gpointer user_data) {
	gm_tray_icon_p tray_icon = user_data;

	if (NULL == tray_icon)
		return (FALSE);
	if (0 != tray_icon->dev_line->is_read_only)
		return (FALSE);

	switch (event->direction) {
	case GDK_SCROLL_UP:
	case GDK_SCROLL_DOWN:
		gmp_dev_line_vol_glob_add(tray_icon->dev_line,
		    ((GDK_SCROLL_UP == event->direction) ? 1 : -1));
		tray_icon->dev_line->is_updated = 1;
		tray_icon->dev_line->write_required ++;
		gmp_dev_write(tray_icon->dev, 0);
		gtk_mixer_window_lines_update(tray_icon->main_window);
		gtk_mixer_tray_icon_update(status_icon);
		break;
	default:
		break;
	}

	return (FALSE);
}

static gboolean
gtk_mixer_tray_icon_release(GtkStatusIcon *status_icon,
    GdkEventButton *event, gpointer user_data) {
	gm_tray_icon_p tray_icon = user_data;

	if (NULL == tray_icon)
		return (FALSE);
	if (0 != tray_icon->dev_line->is_read_only)
		return (FALSE);

	switch (event->button) {
	case 2:
		tray_icon->dev_line->state.is_enabled =
		    ((0 != tray_icon->dev_line->state.is_enabled) ? 0 : 1);
		tray_icon->dev_line->is_updated = 1; /* Mixer must update controls. */
		tray_icon->dev_line->write_required ++;
		gmp_dev_write(tray_icon->dev, 0);
		gtk_mixer_tray_icon_update(status_icon);
		break;
	case 3:
		//tray_icon_menu_show(user_data);
		break;
	}

	return (FALSE);
}

void
gtk_mixer_tray_icon_update(GtkStatusIcon *status_icon) {
	gm_tray_icon_p tray_icon = g_object_get_data(G_OBJECT(status_icon),
	    "__gtk_mixer_tray_icon");
	const char *stock = NULL, *display_name = "";
	char tool_tip[256];
	int vol = 0, is_enabled = 0, is_capture = 0;

	if (NULL == tray_icon)
		return;
	if (NULL != tray_icon->dev_line) {
		if (0 == tray_icon->dev_line->is_updated)
			return; /* No changes. */
		vol = gmp_dev_line_vol_max_get(tray_icon->dev_line);
		is_enabled = tray_icon->dev_line->state.is_enabled;
		is_capture = tray_icon->dev_line->is_capture;
		display_name = tray_icon->dev_line->display_name;
	}

	/* Tool tip. */
	snprintf(tool_tip, sizeof(tool_tip), "%s: %i%%", display_name, vol);
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_status_icon_set_tooltip_text(tray_icon->status_icon, tool_tip);
	G_GNUC_END_IGNORE_DEPRECATIONS

	/* Icon. */
	stock = volume_stock_from_level(is_capture, is_enabled, vol,
	    tray_icon->icon_name);
	if (NULL != stock) { /* Update icon. */
		tray_icon->icon_name = stock;
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_status_icon_set_from_icon_name(tray_icon->status_icon,
		    stock);
		G_GNUC_END_IGNORE_DEPRECATIONS
	}
}

void
gtk_mixer_tray_icon_dev_set(GtkStatusIcon *status_icon, gmp_dev_p dev) {
	gm_tray_icon_p tray_icon = g_object_get_data(G_OBJECT(status_icon),
	    "__gtk_mixer_tray_icon");

	if (NULL == tray_icon)
		return;

	if (NULL == dev ||
	    0 == dev->lines_count) {
		tray_icon->dev = NULL;
		tray_icon->dev_line = NULL;
		gtk_mixer_tray_icon_update(status_icon);
		return;
	}
	/* Use any line by default. */
	tray_icon->dev = dev;
	tray_icon->dev_line = &dev->lines[0];
	/* Try to get first playback device. */
	for (size_t i = 0; i < dev->lines_count; i ++) {
		if (dev->lines[i].is_capture)
			continue;
		tray_icon->dev_line = &dev->lines[i];
		break;
	}
	gtk_mixer_tray_icon_update(status_icon);
}

GtkStatusIcon *
gtk_mixer_tray_icon_create(GtkWidget *main_window) {
	gm_tray_icon_p tray_icon;

	tray_icon = calloc(1, sizeof(gm_tray_icon_t));
	if (NULL == tray_icon)
		return (NULL);

	tray_icon->main_window = main_window;
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	tray_icon->status_icon = gtk_status_icon_new();
	G_GNUC_END_IGNORE_DEPRECATIONS
	g_object_set_data(G_OBJECT(tray_icon->status_icon),
	    "__gtk_mixer_tray_icon", (void*)tray_icon);

	g_signal_connect(G_OBJECT(tray_icon->status_icon),
	    "scroll-event",
	    G_CALLBACK(gtk_mixer_tray_icon_scroll), tray_icon);
	g_signal_connect(G_OBJECT(tray_icon->status_icon),
	    "button-release-event",
	    G_CALLBACK(gtk_mixer_tray_icon_release), tray_icon);

	gtk_mixer_tray_icon_update(tray_icon->status_icon);

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	gtk_status_icon_set_visible(tray_icon->status_icon, TRUE);
	G_GNUC_END_IGNORE_DEPRECATIONS

	return (tray_icon->status_icon);
}
