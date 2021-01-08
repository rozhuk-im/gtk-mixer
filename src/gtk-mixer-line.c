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


typedef struct gtk_mixer_line_s {
	gmp_dev_p dev;
	gmp_dev_line_p dev_line;
	GtkWidget *container;
	GtkWidget *enable_button; /* Mute/record, depend on line type. */
	GtkWidget *lock_button;
	GList *channel_faders;
	const char *icon_name;
	volatile gboolean ignore_signals;
} gm_line_t, *gm_line_p;


static void
gtk_mixer_line_icon_update(gm_line_p line) {
	const char *stock;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(line->enable_button),
	    line->dev_line->state.is_enabled);

	stock = volume_stock_from_level(line->dev_line->is_capture,
	    line->dev_line->state.is_enabled,
	    gmp_dev_line_vol_max_get(line->dev_line),
	    line->icon_name);
	if (NULL != stock) { /* Update icon. */
		line->icon_name = stock;
		gtk_button_set_image(GTK_BUTTON(line->enable_button),
		    gtk_image_new_from_icon_name(stock, GTK_ICON_SIZE_MENU));
	}
}

static void
gtk_mixer_line_enable_toggled(GtkToggleButton *button, gpointer user_data) {
	gm_line_p line = user_data;

	if (!line->ignore_signals) {
		line->dev_line->state.is_enabled =
		    (gtk_toggle_button_get_active(button) ? 1 : 0);
		line->dev_line->is_updated = -1;
		line->dev_line->write_required ++;
		gmp_dev_write(line->dev, 0);
	}
	gtk_mixer_line_icon_update(line);
}


static void
gtk_mixer_line_vol_glob_set(gm_line_p line, gdouble vol_new) {
	GList *iter;

	/* Disable gtk_mixer_line_fader_changed(). */
	line->ignore_signals = TRUE;
	/* Apply vol_new to all faders. */
	for (iter = line->channel_faders; iter != NULL; iter = g_list_next(iter)) {
		/* Apply the volume of the first fader to the others. */
		gtk_range_set_value(GTK_RANGE(iter->data), vol_new);
	}
	line->ignore_signals = FALSE;

	/* Update volume. */
	gmp_dev_line_vol_glob_set(line->dev_line, (int)vol_new);
	line->dev_line->is_updated = -1;
	line->dev_line->write_required ++;
	gmp_dev_write(line->dev, 0);
}

static void
gtk_mixer_line_fader_changed(GtkRange *range, gpointer user_data) {
	gm_line_p line = user_data;
	size_t ch_idx;
	gdouble vol_new = gtk_range_get_value(range);
	char tooltip_text[256];

	ch_idx = (size_t)g_object_get_data(G_OBJECT(range),
	    "__gtk_mixer_line_ch_idx");
	/* Do almost nothing if signals are to be ignored. */
	if (line->ignore_signals) {
update_tooltip:
		/* Update tooltip. */
		snprintf(tooltip_text, sizeof(tooltip_text),
		    "%s@%s: %i%%",
		    channel_name_long[ch_idx], line->dev_line->display_name,
		    (int)vol_new);
		gtk_widget_set_tooltip_text(GTK_WIDGET(range), tooltip_text);
		return;
	}

	/* Collect volumes of all channels. */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(line->lock_button))) {
		gtk_mixer_line_vol_glob_set(line, vol_new);
	} else { /* Single channel vol update. */
		line->dev_line->state.chan_vol[ch_idx] = (int)vol_new;
		/* Update volume. */
		line->dev_line->is_updated = -1;
		line->dev_line->write_required ++;
		/* Commit changes to mixer dev. */
		gmp_dev_write(line->dev, 0);
	}

	gtk_mixer_line_icon_update(line);

	goto update_tooltip;
}

static void
gtk_mixer_line_lock_toggled(GtkToggleButton *button, gpointer user_data) {
	gm_line_p line = user_data;
	GtkWidget *image;

	/* Do nothing if the channels were unlocked */
	if (!gtk_toggle_button_get_active(button)) {
		image = gtk_image_new_from_icon_name("emblem-shared",
		    GTK_ICON_SIZE_MENU);
		gtk_button_set_image(GTK_BUTTON(button), image);
		return;
	}

	image = gtk_image_new_from_icon_name("emblem-readonly",
	    GTK_ICON_SIZE_MENU);
	gtk_button_set_image(GTK_BUTTON(button), image);

	/* Apply first fader volume to all faders. */
	if (NULL != line->channel_faders &&
	    NULL != line->channel_faders->data &&
	    !line->ignore_signals) {
		gtk_mixer_line_vol_glob_set(line,
		    gtk_range_get_value(GTK_RANGE(line->channel_faders->data)));
	}
}

static gboolean
gtk_mixer_line_lock_button_line_draw(GtkWidget *widget, cairo_t *cr,
    gpointer user_data) {
	GtkPositionType position = (GtkPositionType)(size_t)user_data;
	GtkAllocation allocation;
	GtkStyleContext *style_context = gtk_widget_get_style_context(widget);
	GdkPoint points[3];
	const int line_width = 2;
	GdkRGBA fg_color;

	gtk_widget_get_allocation(widget, &allocation);
	if (gtk_widget_get_direction(widget) == GTK_TEXT_DIR_RTL) {
		position = (position == GTK_POS_LEFT) ? GTK_POS_RIGHT :
							GTK_POS_LEFT;
	}

	/* Draw an L-shaped line from the right/left center to the top
	 * middle of the allocation. */
	gtk_style_context_get_color(style_context, GTK_STATE_FLAG_NORMAL,
	    &fg_color);
	gdk_cairo_set_source_rgba(cr, &fg_color);
	cairo_set_line_width(cr, line_width);
	if (position == GTK_POS_RIGHT) {
		points[0].x = 0;
		points[0].y = ((allocation.height - line_width) / 2);
		points[1].x = ((allocation.width - line_width) / 2);
		points[1].y = points[0].y;
		points[2].x = points[1].x;
		points[2].y = 0;
	} else {
		points[0].x = allocation.width;
		points[0].y = ((allocation.height - line_width) / 2);
		points[1].x = ((allocation.width + line_width) / 2);
		points[1].y = points[0].y;
		points[2].x = points[1].x;
		points[2].y = 0;
	}
	cairo_move_to(cr, points[0].x, points[0].y);
	cairo_line_to(cr, points[1].x, points[1].y);
	cairo_line_to(cr, points[2].x, points[2].y);
	cairo_stroke(cr);

	return (TRUE);
}

static void
gtk_mixer_line_destroy(GtkWidget *container __unused, gpointer user_data) {
	gm_line_p line = user_data;

	g_list_free(line->channel_faders);
	free(line);
}

GtkWidget *
gtk_mixer_line_create(gmp_dev_p dev, gmp_dev_line_p dev_line) {
	gm_line_p line;
	size_t ch_idx;
	int vol_prev = GMPDL_CHAN_VOL_INVALID;
	gboolean volume_locked = TRUE;
	char tooltip_text[256];
	GtkWidget *faders_vbox, *faders_hbox, *fader;
	GtkWidget *lock_button_hbox, *buttons_hbox;
	GtkWidget *lock_button_line1, *lock_button_line2;
	GtkRequisition lock_button_hbox_requisition;

	line = calloc(1, sizeof(gm_line_t));
	if (NULL == line)
		return (NULL);
	line->dev = dev;
	line->dev_line = dev_line;
	line->container = gtk_box_new(GTK_ORIENTATION_VERTICAL,
	    BORDER_WIDTH);
	g_object_set_data(G_OBJECT(line->container),
	    "__gtk_mixer_line", (void*)line);
	g_signal_connect(line->container, "destroy",
	    G_CALLBACK(gtk_mixer_line_destroy), line);

	/* Disable gtk_mixer_line_fader_changed(). */
	line->ignore_signals = TRUE;

	/* Center and do not expand faders and lock button. */
	faders_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, BORDER_WIDTH);
	g_object_set(G_OBJECT(faders_vbox),
	    "halign", GTK_ALIGN_CENTER,
	    "valign", GTK_ALIGN_FILL,
	    "vexpand", TRUE, NULL);
	gtk_box_pack_start(GTK_BOX(line->container), faders_vbox, TRUE,
	    TRUE, 0);
	gtk_widget_show(faders_vbox);

	faders_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, BORDER_WIDTH);
	gtk_box_pack_start(GTK_BOX(faders_vbox), faders_hbox, TRUE,
	    TRUE, 0);
	gtk_widget_show(faders_hbox);

	/* Create a fader for each channel. */
	for (ch_idx = gmp_dev_line_chan_first(line->dev_line);
	    ch_idx < MIXER_CHANNELS_COUNT;
	    ch_idx = gmp_dev_line_chan_next(line->dev_line, ch_idx)) {
		fader = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL,
		    0.0, 100.0 , 1.0);
		line->channel_faders = g_list_append(line->channel_faders,
		    fader);
		gtk_scale_set_draw_value(GTK_SCALE(fader), FALSE);
		gtk_range_set_inverted(GTK_RANGE(fader), TRUE);
		g_object_set_data(G_OBJECT(fader),
		    "__gtk_mixer_line_ch_idx", (void*)ch_idx);
		/* Make read-only lines insensitive. */
		if (dev_line->is_read_only) {
			gtk_widget_set_sensitive(fader, FALSE);
		}
		gtk_box_pack_start(GTK_BOX(faders_hbox), fader, TRUE,
		    TRUE, 0);
		g_signal_connect(fader, "value-changed",
		    G_CALLBACK(gtk_mixer_line_fader_changed), line);
		gtk_range_set_value(GTK_RANGE(fader),
		    dev_line->state.chan_vol[ch_idx]);
		gtk_widget_show(fader);

		/* Equal volume across all channels means the line is locked. */
		if (volume_locked && GMPDL_CHAN_VOL_INVALID != vol_prev &&
		    dev_line->state.chan_vol[ch_idx] != vol_prev) {
			volume_locked = FALSE;
		}
		vol_prev = dev_line->state.chan_vol[ch_idx];
	}

	/* Create lock button with lines. */
	lock_button_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(faders_vbox), lock_button_hbox,
	    FALSE, FALSE, 0);
	gtk_widget_show(lock_button_hbox);

	/* Left L-shaped line. */
	lock_button_line1 = gtk_drawing_area_new();
	gtk_widget_set_size_request(lock_button_line1,
	    (BORDER_WIDTH * 2), 8);
	gtk_box_pack_start(GTK_BOX(lock_button_hbox), lock_button_line1,
	    TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(lock_button_line1), "draw",
	    G_CALLBACK(gtk_mixer_line_lock_button_line_draw),
	    GINT_TO_POINTER(GTK_POS_LEFT));
	gtk_widget_show(lock_button_line1);

	/* Lock button. */
	snprintf(tooltip_text, sizeof(tooltip_text),
	    _("Lock channels for %s together"), dev_line->display_name);
	line->lock_button = gtk_toggle_button_new();
	gtk_widget_set_tooltip_text(line->lock_button, tooltip_text);
	/* Make button insensitive for read-only lines. */
	if (dev_line->is_read_only) {
		gtk_widget_set_sensitive(line->lock_button, FALSE);
	}
	g_signal_connect(line->lock_button, "toggled",
	    G_CALLBACK(gtk_mixer_line_lock_toggled), line);
	gtk_box_pack_start(GTK_BOX(lock_button_hbox), line->lock_button,
	    FALSE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(line->lock_button),
	    volume_locked);
	gtk_widget_show(line->lock_button);

	/* Right L-shaped line. */
	lock_button_line2 = gtk_drawing_area_new();
	gtk_widget_set_size_request(lock_button_line2,
	    (BORDER_WIDTH * 2), 8);
	gtk_box_pack_start(GTK_BOX(lock_button_hbox), lock_button_line2,
	    TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(lock_button_line2), "draw",
	    G_CALLBACK(gtk_mixer_line_lock_button_line_draw),
	    GINT_TO_POINTER(GTK_POS_RIGHT));
	gtk_widget_show(lock_button_line2);

	/* Destroy the chain button and lines and replace them with an
	 * equally sized placeholder if there is only one fader. */
	if (dev_line->chan_vol_count == 1) {
		gtk_widget_get_preferred_size(lock_button_hbox, NULL,
		    &lock_button_hbox_requisition);
		gtk_widget_destroy(lock_button_hbox);
		lock_button_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_size_request(lock_button_hbox,
		    lock_button_hbox_requisition.width,
		    lock_button_hbox_requisition.height);
		gtk_box_pack_start(GTK_BOX(faders_vbox), lock_button_hbox,
		    FALSE, FALSE, 0);
		gtk_widget_show(lock_button_hbox);
	}

	buttons_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,
	    (BORDER_WIDTH * 2));
	g_object_set(G_OBJECT(buttons_hbox),
	    "halign", GTK_ALIGN_CENTER,
	    "valign", GTK_ALIGN_END, NULL);
	gtk_box_pack_start(GTK_BOX(line->container), buttons_hbox, FALSE,
	    FALSE, 0);
	gtk_widget_show(buttons_hbox);

	/* Mute/record button. */
	line->enable_button = gtk_toggle_button_new();
	snprintf(tooltip_text, sizeof(tooltip_text),
	    _("Enable/disable line %s"),
	    dev_line->display_name);
	gtk_widget_set_tooltip_text(line->enable_button, tooltip_text);
	/* Make button insensitive for read-only lines. */
	if (dev_line->is_read_only) {
		gtk_widget_set_sensitive(line->enable_button, FALSE);
	}
	g_signal_connect(line->enable_button, "toggled",
	    G_CALLBACK(gtk_mixer_line_enable_toggled), line);
	gtk_box_pack_start(GTK_BOX(buttons_hbox),
	    line->enable_button, FALSE, FALSE, 0);
	gtk_widget_show(line->enable_button);

	/* Some of the mixer controls need to be updated before they
	 * can be used. */
	gtk_mixer_line_icon_update(line);
	line->ignore_signals = FALSE;

	return (line->container);
}

void
gtk_mixer_line_update(GtkWidget *container) {
	size_t ch_idx;
	GList *iter;
	gm_line_p line = g_object_get_data(G_OBJECT(container),
	    "__gtk_mixer_line");

	if (NULL == container || NULL == line || NULL == line->dev_line)
		return;
	if (0 >= line->dev_line->is_updated)
		return;

	line->ignore_signals = TRUE;
	for (iter = line->channel_faders; iter != NULL; iter = g_list_next(iter)) {
		ch_idx = (size_t)g_object_get_data(G_OBJECT(iter->data),
		    "__gtk_mixer_line_ch_idx");
		gtk_range_set_value(GTK_RANGE(iter->data),
		    line->dev_line->state.chan_vol[ch_idx]);
	}
	gtk_mixer_line_icon_update(line);
	line->ignore_signals = FALSE;
}
