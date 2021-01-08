/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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

#define GM_CMB_DEVS_COLUMN_NAME	0
#define GM_CMB_DEVS_COLUMN_CARD	1


static void
gtk_mixer_devs_combo_changed(GtkWidget *combo, gpointer user_data __unused) {
	GtkTreeIter iter;
	GtkListStore *list_store = g_object_get_data(G_OBJECT(combo),
	    "__gtk_mixer_devs_combo_list_store");
	gmp_dev_p dev_new = NULL;
	gmp_dev_p dev_old = g_object_get_data(G_OBJECT(combo),
	    "__gtk_mixer_devs_combo_current");

	if (NULL == list_store)
		return;
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter,
		    GM_CMB_DEVS_COLUMN_CARD, &dev_new, -1);
	}

	/* Init/uninit here, before other get notification. */
	gmp_dev_init(dev_new);
	g_object_set_data(G_OBJECT(combo),
	    "__gtk_mixer_devs_combo_current", dev_new);
	gmp_dev_uninit(dev_old);
}

static void
gtk_mixer_devs_combo_destroy(GtkWidget *combo __unused, gpointer user_data) {
	GtkListStore *list_store = user_data;

	gtk_list_store_clear(list_store);
	g_object_unref(list_store);
}

GtkWidget *
gtk_mixer_devs_combo_create(gmp_dev_list_p dev_list, gmp_dev_p dev) {
	GtkWidget *combo;
	GtkListStore *list_store;
	GtkCellRenderer *renderer;

	combo = gtk_combo_box_new();

	list_store = gtk_list_store_new(2, G_TYPE_STRING,
	    G_TYPE_POINTER);
	g_object_set_data(G_OBJECT(combo),
	    "__gtk_mixer_devs_combo_list_store", list_store);
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo),
	    GTK_TREE_MODEL(list_store));

	g_signal_connect(combo, "changed",
	    G_CALLBACK(gtk_mixer_devs_combo_changed), NULL);
	g_signal_connect(combo, "destroy",
	    G_CALLBACK(gtk_mixer_devs_combo_destroy), list_store);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(renderer), "ellipsize",
	    PANGO_ELLIPSIZE_END, NULL);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(combo), renderer,
	    "text", GM_CMB_DEVS_COLUMN_NAME);

	gtk_mixer_devs_combo_update(combo, dev_list);

	if (NULL == dev) {
		dev = gmp_dev_list_get_default(dev_list);
	}
	gtk_mixer_devs_combo_set_active_device(combo, dev);

	return (combo);
}

gmp_dev_p
gtk_mixer_devs_combo_get_dev(GtkWidget *combo) {

	if (NULL == combo)
		return (NULL);
	return (g_object_get_data(G_OBJECT(combo),
	    "__gtk_mixer_devs_combo_current"));
}

void
gtk_mixer_devs_combo_set_active_device(GtkWidget *combo,
    gmp_dev_p dev) {
	gmp_dev_p device_cur = NULL;
	GtkTreeIter iter;
	gboolean valid_iter;
	GtkListStore *list_store = g_object_get_data(G_OBJECT(combo),
	    "__gtk_mixer_devs_combo_list_store");

	if (NULL == dev || NULL == list_store) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
		return;
	}
	valid_iter = gtk_tree_model_get_iter_first(
	    GTK_TREE_MODEL(list_store), &iter);
	while (valid_iter) {
		gtk_tree_model_get(GTK_TREE_MODEL(list_store),
		    &iter, GM_CMB_DEVS_COLUMN_CARD, &device_cur, -1);
		if (device_cur == dev)
			break;
		valid_iter = gtk_tree_model_iter_next(
		    GTK_TREE_MODEL(list_store), &iter);
	}
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo), &iter);
}

void
gtk_mixer_devs_combo_update(GtkWidget *combo, gmp_dev_list_p dev_list) {
	gmp_dev_p device_cur = NULL;
	GtkTreeIter iter;
	gboolean valid_iter;
	GtkListStore *list_store = g_object_get_data(G_OBJECT(combo),
	    "__gtk_mixer_devs_combo_list_store");
	char display_name[256];

	if (NULL == list_store)
		return;

	if (NULL != dev_list) {
		for (size_t i = 0; i < dev_list->count; i ++) {
			gtk_list_store_append(list_store, &iter);
			gtk_list_store_set(list_store, &iter,
			    GM_CMB_DEVS_COLUMN_CARD, &dev_list->devs[i], -1);
		}
	}

	valid_iter = gtk_tree_model_get_iter_first(
	    GTK_TREE_MODEL(list_store), &iter);
	while (valid_iter) {
		gtk_tree_model_get(GTK_TREE_MODEL(list_store),
		    &iter, GM_CMB_DEVS_COLUMN_CARD, &device_cur, -1);
		snprintf(display_name, sizeof(display_name),
		    "%s: %s (%s)%s",
		    device_cur->plugin->descr->name,
		    device_cur->description,
		    device_cur->name,
		    ((gmp_dev_is_default(device_cur)) ? " [default]" : ""));
		gtk_list_store_set(list_store, &iter,
		    GM_CMB_DEVS_COLUMN_NAME, display_name, -1);
		valid_iter = gtk_tree_model_iter_next(
		    GTK_TREE_MODEL(list_store), &iter);
	}
}
