/*-
 * Copyright (c) 2020-2022 Rozhuk Ivan <rozhuk.im@gmail.com>
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
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <getopt.h>

#include "gtk-mixer.h"


typedef struct gtk_mixer_app_s {
	gm_plugin_p	plugins;
	size_t		plugins_count;
	gmp_dev_list_t	dev_list;
	GtkWidget	*window;
	GtkStatusIcon	*status_icon;
	GtkWidget	*tray_icon_menu;

	gmp_dev_p	dev; /* Current sound device. */

	/* GUI update rate scaler. */
	size_t update_skip_counter;
	size_t update_force_counter;
} gm_app_t, *gm_app_p;

/* Check updates every 1s if no changes and every 100ms if something was
 * changes in last 5 second. */
#define UPDATE_INTERVAL		100
/* If no chenges - check every (UPDATE_INTERVAL * UPDATE_SKIP_MAX_COUNT) ms. */
#define UPDATE_SKIP_MAX_COUNT	10
/* If was changes - check every UPDATE_INTERVAL in next (UPDATE_INTERVAL * UPDATE_FORCE_MAX_COUNT) ms. */
#define UPDATE_FORCE_MAX_COUNT	50


static gboolean
gtk_mixer_check_update(gm_app_p app) {
	int error;
	size_t changes = 0;
	gmp_dev_list_t dev_list;
	gmp_dev_p dev = NULL;

	/* GUI update rate scaler. */
	app->update_skip_counter ++;
	if (UPDATE_SKIP_MAX_COUNT > app->update_skip_counter)
		return (TRUE);
	app->update_skip_counter = 0;

	/* Devices list update check. */
	if (gmp_is_list_devs_changed(app->plugins, app->plugins_count)) {
		changes ++;
		memset(&dev_list, 0x00, sizeof(dev_list));
		error = gmp_list_devs(app->plugins, app->plugins_count,
		    &dev_list);
		if (0 == error) {
			/* Try to find old current dev in updated dev list. */
			dev = gmp_dev_find_same(&dev_list, app->dev);
			gtk_mixer_window_dev_list_update(app->window,
			    &dev_list);
			gmp_dev_list_clear(&app->dev_list);
			app->dev_list = dev_list;
			/* Select new current device. */
			if (NULL == dev) {
				dev = gmp_dev_list_get_default(&app->dev_list);
			}
			gtk_mixer_window_dev_cur_set(app->window, dev);
		}
	} else if (0 != gmp_is_def_dev_changed(app->plugins,
	    app->plugins_count)) { /* Default device changed. */
		changes ++;
		gtk_mixer_window_dev_list_update(app->window, NULL);
	}

	/* Check lines update for current device. */
	if (NULL != app->dev) {
		error = gmp_dev_read(app->dev, 1);
		if (0 == error && 
		    gmp_dev_is_updated(app->dev)) {
			/* GUI update. */
			gtk_mixer_window_lines_update(app->window);
			gtk_mixer_tray_icon_update(app->status_icon);
			changes += gmp_dev_is_updated_clear(app->dev);
		}
	}

	/* GUI update rate scaler. */
	/* If something changed than force check updates on next timer fire. */
	if (0 != changes) {
		app->update_force_counter = UPDATE_FORCE_MAX_COUNT;
	}
	if (0 != app->update_force_counter) {
		app->update_force_counter --;
		app->update_skip_counter = UPDATE_SKIP_MAX_COUNT;
	}

	return (TRUE);
}

static void
gtk_mixer_soundcard_changed(GtkWidget *combo __unused,
    gpointer user_data) {
	gm_app_p app = user_data;

	if (NULL == app)
		return;

	app->dev = gtk_mixer_window_dev_cur_get(app->window);

	/* Tray icon.*/
	gtk_mixer_tray_icon_dev_set(app->status_icon, app->dev);
	gtk_mixer_tray_icon_update(app->status_icon);
}

static void
gtk_mixer_status_icon_activate(GtkStatusIcon *status_icon __unused,
    gpointer user_data) {
	gm_app_p app = user_data;

	if (NULL == app)
		return;

	if (gtk_widget_get_visible(app->window)) {
		gtk_widget_hide(app->window);
	} else {
		gtk_widget_show(app->window);
		gtk_window_deiconify(GTK_WINDOW(app->window));
	}
}

static void
on_tray_icon_menu_about_click(GtkMenuItem *menuitem __unused,
    gpointer user_data __unused) {
	GtkAboutDialog *dlg = GTK_ABOUT_DIALOG(gtk_about_dialog_new());
	const char *authors[] = {
		"",
		"2020-2022 Rozhuk Ivan",
		"",
		"Original xfce4-mixer",
		"2012 Guido Berhoerster",
		"2008 Jannis Pohlmann",
		"and others...",
		NULL
	};

	gtk_about_dialog_set_program_name(dlg, "GTK-Mixer");
	gtk_about_dialog_set_version(dlg, VERSION);
	gtk_about_dialog_set_copyright(dlg,
	    "Copyright (c) 2020-2022 Rozhuk Ivan <rozhuk.im@gmail.com>");
	gtk_about_dialog_set_comments(dlg, PACKAGE_DESCRIPTION);
	gtk_about_dialog_set_license_type(dlg, GTK_LICENSE_GPL_2_0);
	gtk_about_dialog_set_website(dlg, PACKAGE_URL);
	gtk_about_dialog_set_website_label(dlg, "github.com");
	gtk_about_dialog_set_authors(dlg, authors);
	gtk_about_dialog_set_translator_credits(dlg, _("translator-credits"));
	gtk_about_dialog_set_logo_icon_name(dlg, APP_ICON_NAME);
	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(GTK_WIDGET(dlg));
}
static void
gtk_mixer_status_icon_menu(GtkStatusIcon *status_icon __unused,
    guint button, guint activate_time __unused, gpointer user_data) {
	gm_app_p app = user_data;

	if (NULL == app ||
	    3 != button)
		return;
	if (NULL == app->tray_icon_menu) {
		GtkWidget *mi;
		app->tray_icon_menu = gtk_menu_new();

		/* About. */
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		mi = gtk_image_menu_item_new_from_stock("gtk-about", NULL);
		G_GNUC_END_IGNORE_DEPRECATIONS
		g_signal_connect(G_OBJECT(mi), "activate",
		    G_CALLBACK(on_tray_icon_menu_about_click), app);
		gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_icon_menu),
		    mi);
		/* Separator. */
		gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_icon_menu),
		    gtk_separator_menu_item_new());
		/* Quit. */
		G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		mi  = gtk_image_menu_item_new_from_stock("gtk-quit", NULL);
		G_GNUC_END_IGNORE_DEPRECATIONS
		g_signal_connect(G_OBJECT(mi), "activate",
		    G_CALLBACK(gtk_main_quit), NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(app->tray_icon_menu),
		    mi);

		gtk_widget_show_all(GTK_WIDGET(app->tray_icon_menu));
	}
	gtk_menu_popup_at_pointer(GTK_MENU(app->tray_icon_menu), NULL);
}


int
main(int argc, char **argv) {
	int error;
	int ch, opt_idx = -1, start_hidden = 0;
	gm_app_t app;
	gmp_dev_p dev = NULL;
	struct option long_options[] = {
		{ "start-hidden",	no_argument,	&start_hidden,	1 },
		{ NULL,			0,		NULL,		0 }
	};

	memset(&app, 0x00, sizeof(gm_app_t));

	while ((ch = getopt_long_only(argc, argv, "", long_options,
	    &opt_idx)) != -1) {
	}


	error = gmp_init(&app.plugins, &app.plugins_count);
	if (0 != error)
		return (error);
	error = gmp_list_devs(app.plugins, app.plugins_count,
	    &app.dev_list);
	if (0 != error)
		return (error);

	gtk_init(&argc, &argv);

	/* Set application name. */
	g_set_application_name(_("Audio Mixer"));

	/* Use volume control icon for all mixer windows. */
	gtk_window_set_default_icon_name(APP_ICON_NAME);

	/* Main window. */
	app.window = gtk_mixer_window_create();
	gtk_mixer_window_dev_list_update(app.window, &app.dev_list);
#if 0
	if (card_name != NULL) {
		dev = gtk_mixer_get_card(card_name);
	} else {
		dev = gtk_mixer_get_default_card();
		g_object_set(gm_win->preferences, "sound-card",
		    gtk_mixer_get_card_internal_name(dev), NULL);
	}
	g_free(card_name);
#endif
	if (NULL == dev) {
		dev = gmp_dev_list_get_default(&app.dev_list);
	}
	gtk_mixer_window_dev_cur_set(app.window, dev);


	/* Tray icon. */
	app.status_icon = gtk_mixer_tray_icon_create(app.window);
	g_signal_connect(app.status_icon, "activate",
	    G_CALLBACK(gtk_mixer_status_icon_activate), &app);
	g_signal_connect(app.status_icon, "popup-menu",
	    G_CALLBACK(gtk_mixer_status_icon_menu), &app);

	/* Allow monitor selected sound dev. */
	gtk_mixer_window_connect_dev_changed(app.window,
	    G_CALLBACK(gtk_mixer_soundcard_changed), &app);
	/* Force set sound dev. */
	gtk_mixer_soundcard_changed(NULL, &app);

	/* Display the mixer window. */
	if (start_hidden) {
		gtk_widget_hide(app.window);
	} else {
		gtk_window_present(GTK_WINDOW(app.window));
	}

	/* For update, if volume changed from other app. */
	g_timeout_add(UPDATE_INTERVAL,
	    (GSourceFunc)gtk_mixer_check_update, &app);

	gtk_main();

	/* Cleanup. */
	gmp_dev_list_clear(&app.dev_list);
	gmp_uninit(app.plugins, app.plugins_count);

	return (error);
}

const char *
volume_stock_from_level(const int is_mic, const int is_enabled,
    const int level, const char *cur_icon_name) {
	const int levels[] = { -1, 0, 33, 66, 100 };
	const char *volume_level[nitems(levels)] = {
	    "audio-volume-muted",
	    "audio-volume-muted",
	    "audio-volume-low",
	    "audio-volume-medium",
	    "audio-volume-high"
	};
	const char *mic_sens_level[nitems(levels)] = {
	    "microphone-disabled-symbolic",
	    "microphone-sensitivity-muted",
	    "microphone-sensitivity-low",
	    "microphone-sensitivity-medium",
	    "microphone-sensitivity-high"
	};
	const char **stocks = ((0 != is_mic) ? mic_sens_level : volume_level);

	for (size_t i = 0; i < nitems(levels); i ++) {
		if (levels[i] < level && 0 != is_enabled)
			continue;
		if (NULL != cur_icon_name &&
		    strcmp(cur_icon_name, stocks[i]) == 0)
			break; /* No need to update. */
		return (stocks[i]);
	}

	return (NULL);
}
