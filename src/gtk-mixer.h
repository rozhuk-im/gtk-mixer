/*-
 * Copyright (c) 2020-2025 Rozhuk Ivan <rozhuk.im@gmail.com>
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


#ifndef __GTK_MIXER_H__
#define __GTK_MIXER_H__

#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h> /* snprintf, fprintf */
#include <time.h>
#include <string.h> /* bcopy, bzero, memcpy, memmove, memset, strerror... */
#include <stdlib.h> /* malloc, exit */
#include <unistd.h> /* close, write, sysconf */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#else
#	define PACKAGE_NAME		"gtk-mixer"
#	define VERSION			"1.0.0"
#	define PACKAGE_DESCRIPTION	""
#	define PACKAGE_URL		""
#endif

#define APP_ICON_NAME "multimedia-volume-control"

#define GETTEXT_PACKAGE PACKAGE_NAME
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "plugin_api.h"

#define BORDER_WIDTH 5


const char *volume_stock_from_level(const int is_mic, const int is_enabled,
    const int level, const char *cur_icon_name);

GtkWidget *gtk_mixer_window_create(void);
gulong gtk_mixer_window_connect_dev_changed(GtkWidget *window,
    GCallback c_handler, gpointer data);
gmp_dev_p gtk_mixer_window_dev_cur_get(GtkWidget *window);
void gtk_mixer_window_dev_cur_set(GtkWidget *window, gmp_dev_p dev);
void gtk_mixer_window_dev_list_update(GtkWidget *window, gmp_dev_list_p dev_list);
void gtk_mixer_window_lines_update(GtkWidget *window);

GtkWidget *gtk_mixer_devs_combo_create(void);
gmp_dev_p gtk_mixer_devs_combo_cur_get(GtkWidget *combo);
void gtk_mixer_devs_combo_cur_set(GtkWidget *combo, gmp_dev_p dev);
void gtk_mixer_devs_combo_dev_list_set(GtkWidget *combo, gmp_dev_list_p dev_list);
void gtk_mixer_devs_combo_update(GtkWidget *combo);

GtkWidget *gtk_mixer_container_create(void);
void gtk_mixer_container_dev_set(GtkWidget *container, gmp_dev_p dev);
void gtk_mixer_container_update(GtkWidget *container);

GtkWidget *gtk_mixer_line_create(gmp_dev_p dev, gmp_dev_line_p dev_line);
void gtk_mixer_line_update(GtkWidget *container);


GtkStatusIcon *gtk_mixer_tray_icon_create(GtkWidget *main_window);
void gtk_mixer_tray_icon_dev_set(GtkStatusIcon *status_icon, gmp_dev_p dev);
void gtk_mixer_tray_icon_update(GtkStatusIcon *status_icon);


#endif /* __GTK_MIXER_H__ */
