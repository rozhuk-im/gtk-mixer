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


static void
gtk_mixer_container_destroy(GtkWidget *container __unused, gpointer user_data) {
	GHashTable *widgets = user_data;

	g_hash_table_remove_all(widgets);
	g_hash_table_unref(widgets);
}

GtkWidget *
gtk_mixer_container_create(void) {
	GtkWidget *container;
	GHashTable *widgets;

	container = gtk_notebook_new();
	gtk_notebook_set_show_border(GTK_NOTEBOOK(container), TRUE);
	widgets = g_hash_table_new_full(g_str_hash,
	    g_str_equal, g_free, NULL);
	g_object_set_data(G_OBJECT(container),
	    "__gtk_mixer_container_widgets", widgets);
	g_signal_connect(container, "destroy",
	    G_CALLBACK(gtk_mixer_container_destroy), widgets);

	return (container);
}

static void
gtk_mixer_container_create_contents(GtkWidget *container,
    GHashTable *widgets, gmp_dev_p dev) {
	gmp_dev_line_p line;
	const gchar *titles[4] = { N_("_Playback"), N_("C_apture"),
		N_("S_witches"), N_("_Options") };
	GtkWidget *line_widget, *line_label_widget;
	GtkWidget *labels[4], *scrollwins[4], *views[4];
	GtkWidget *last_separator[4] = { NULL, NULL, NULL, NULL };
	GtkWidget *vbox, *label1, *label2, *label3;
	const char *line_label;
	gint num_children[4] = { 0, 0, 0, 0 };
	gboolean no_controls_visible = TRUE;

	/* Create widgets for all four tabs. */
	for (size_t i = 0; i < 4; i ++) {
		labels[i] = gtk_label_new_with_mnemonic(_(titles[i]));
		scrollwins[i] = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_shadow_type(
		    GTK_SCROLLED_WINDOW(scrollwins[i]), GTK_SHADOW_IN);
		gtk_container_set_border_width(GTK_CONTAINER(scrollwins[i]),
		    BORDER_WIDTH);
		gtk_scrolled_window_set_policy(
		    GTK_SCROLLED_WINDOW(scrollwins[i]),
		    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

		views[i] = gtk_grid_new();
		g_object_set(G_OBJECT(views[i]),
		    "row-spacing", BORDER_WIDTH,
		    "column-spacing", (BORDER_WIDTH * 2),
		    "border-width", BORDER_WIDTH, NULL);
		gtk_container_add(GTK_CONTAINER(scrollwins[i]), views[i]);
		gtk_viewport_set_shadow_type(GTK_VIEWPORT(
		    gtk_bin_get_child(GTK_BIN(scrollwins[i]))),
		    GTK_SHADOW_NONE);
		gtk_widget_show(views[i]);
		gtk_widget_show(scrollwins[i]);
	}

	/* Create controls for all mixer lines. */
	//preferences = gtk_mixer_preferences_get();
	for (size_t i = 0; NULL != dev && i < dev->lines_count; i ++) {
		line = &dev->lines[i];
		line_label = line->display_name;

		//if (!gtk_mixer_preferences_get_control_visible(
		//	preferences, line_label))
		//	continue;

		size_t idx = ((0 == line->is_capture) ? 0 : 1);
		/* Create a regular volume control for this line. */
		line_label_widget = gtk_label_new(line_label);
		gtk_grid_attach(GTK_GRID(views[idx]),
		    line_label_widget, num_children[idx], 0, 1, 1);
		gtk_widget_show(line_label_widget);
		line_widget = gtk_mixer_line_create(dev, line);
		g_object_set(G_OBJECT(line_widget),
		    "valign", GTK_ALIGN_FILL,
		    "vexpand", TRUE, NULL);
		gtk_grid_attach(GTK_GRID(views[idx]),
		    line_widget, num_children[idx], 1, 1, 1);
		gtk_widget_show(line_widget);
		num_children[idx]++;

		/* Append a separator. The last one will be
		 * destroyed later. */
		last_separator[idx] = gtk_separator_new(
		    GTK_ORIENTATION_VERTICAL);
		gtk_grid_attach(GTK_GRID(views[idx]),
		    last_separator[idx], num_children[idx], 0, 1, 2);
		gtk_widget_show(last_separator[idx]);
		num_children[idx]++;

		/* Add the line to the hash table. */
		g_hash_table_insert(widgets,
		    g_strdup(line_label), line_widget);

#if 0
		case XFCE_MIXER_TRACK_TYPE_SWITCH:
			line_widget = gtk_mixer_switch_new(
			    container->card, line);
			g_object_set(G_OBJECT(line_widget),
			    "halign", GTK_ALIGN_FILL,
			    "hexpand", TRUE, NULL);
			gtk_grid_attach(GTK_GRID(views[2]),
			    line_widget, 0, num_children[2], 1, 1);
			gtk_widget_show(line_widget);

			num_children[2]++;

			/* Add the line to the hash table. */
			g_hash_table_insert(container->widgets,
			    g_strdup(line_label), line_widget);
			break;

		case XFCE_MIXER_TRACK_TYPE_OPTIONS:
			snprintf(option_line_label, sizeof(option_line_label),
			    "%s:", line_label);
			line_label_widget = gtk_label_new(
			    option_line_label);
			g_object_set(G_OBJECT(line_label_widget),
			    "halign", GTK_ALIGN_FILL, NULL);
			gtk_grid_attach(GTK_GRID(views[3]),
			    line_label_widget, 0, num_children[3], 1, 1);
			gtk_widget_show(line_label_widget);

			line_widget = gtk_mixer_option_new(
			    container->card, line);
			g_object_set(G_OBJECT(line_widget),
			    "halign", GTK_ALIGN_FILL,
			    "hexpand", TRUE, NULL);
			gtk_grid_attach(GTK_GRID(views[3]),
			    line_widget, 1, num_children[3], 1, 1);
			gtk_widget_show(line_widget);

			num_children[3]++;

			/* Add the line to the hash table. */
			g_hash_table_insert(container->widgets,
			    g_strdup(line_label), line_widget);
			break;
#endif
	}

	/* Append tab or destroy all its widgets - depending on the
	 * contents of each tab. */
	for (gint i = 0; i < 4; i ++) {
		/* Destroy the last separator in the tab. */
		if (last_separator[i] != NULL) {
			gtk_widget_destroy(last_separator[i]);
		}

		gtk_notebook_append_page(GTK_NOTEBOOK(container),
		    scrollwins[i], labels[i]);

		/* Hide tabs with no visible controls. */
		if (num_children[i] > 0) {
			no_controls_visible = FALSE;
		} else {
			gtk_widget_hide(gtk_notebook_get_nth_page(
			    GTK_NOTEBOOK(container), i));
		}
	}
	/* Show informational message if no controls are visible. */
	if (no_controls_visible) {
		label1 = gtk_label_new(_("No controls visible"));
		gtk_widget_show(label1);

		vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		g_object_set(G_OBJECT(vbox),
		    "halign", GTK_ALIGN_CENTER,
		    "hexpand", TRUE,
		    "valign", GTK_ALIGN_CENTER,
		    "vexpand", TRUE,
		    "border-width", BORDER_WIDTH, NULL);
		gtk_widget_show(vbox);

		label2 = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label2),
		    _("<span weight=\"bold\" size=\"larger\">No controls visible</span>"));
		g_object_set(G_OBJECT(label2),
		    "max-width-chars", 80,
		    "xalign", 0.0,
		    "wrap", TRUE, NULL);
		gtk_box_pack_start(GTK_BOX(vbox), label2, FALSE, TRUE, 0);
		gtk_widget_show(label2);

		label3 = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label3),
		    _("In order to toggle the visibility of mixer controls, open the <b>\"Select Controls\"</b> dialog."));
		g_object_set(G_OBJECT(label3),
		    "max-width-chars", 80,
		    "xalign", 0.0,
		    "wrap", TRUE, NULL);
		gtk_box_pack_start(GTK_BOX(vbox), label3, FALSE, TRUE, 0);
		gtk_widget_show(label3);

		gtk_notebook_append_page(GTK_NOTEBOOK(container), vbox,
		    label1);
	}
}

void
gtk_mixer_container_dev_set(GtkWidget *container, gmp_dev_p dev) {
	gint current_tab, i;
	GHashTable *widgets = g_object_get_data(G_OBJECT(container),
	    "__gtk_mixer_container_widgets");

	g_hash_table_remove_all(widgets);

	/* Remember active tab */
	current_tab = gtk_notebook_get_current_page(GTK_NOTEBOOK(container));

	/* Destroy all tabs */
	for (i = gtk_notebook_get_n_pages(GTK_NOTEBOOK(container)); i >= 0; i --) {
		gtk_notebook_remove_page(GTK_NOTEBOOK(container), i);
	}

	/* Re-create contents */
	gtk_mixer_container_create_contents(container, widgets, dev);

	/* Restore previously active tab if possible */
	if (current_tab > 0 && current_tab < 4) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(container),
		    current_tab);
	}
}

void
gtk_mixer_container_update(GtkWidget *container) {
	GtkWidget *line_widget;
	GHashTable *widgets = g_object_get_data(G_OBJECT(container),
	    "__gtk_mixer_container_widgets");
	GHashTableIter iter;

	if (NULL == container || NULL == widgets)
		return;

	g_hash_table_iter_init(&iter, widgets);
	while (g_hash_table_iter_next(&iter, NULL, (void**)&line_widget)) {
		gtk_mixer_line_update(line_widget);
	}
}
