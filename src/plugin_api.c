/*-
 * Copyright (c) 2020 - 2021 Rozhuk Ivan <rozhuk.im@gmail.com>
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "plugin_api.h"


static inline int
strcmp_safe(const char *s1, const char *s2) {
	
	if (s1 == s2)
		return (0);
	if (NULL == s1)
		return (-127);
	if (NULL == s2)
		return (127);
	return (strcmp(s1, s2));
}

static inline int
volume_apply_limits(const int vol) {

	if (0 > vol)
		return (0);
	if (100 < vol)
		return (100);
	return (vol);
}

static void
gmp_dev_line_state_vol_normalize(gmp_dev_line_state_p state,
    const uint32_t chan_map) {

	if (NULL == state || 0 == chan_map)
		return;
	for (size_t i = 0; i < MIXER_CHANNELS_COUNT; i ++) {
		if (0 == (((((uint32_t)1) << i) & chan_map)))
			continue;
		state->chan_vol[i] = volume_apply_limits(state->chan_vol[i]);
	}
}


int
gmp_init(gm_plugin_p *plugins, size_t *plugins_count) {
	size_t i, j;
	gm_plugin_p plugin;

	if (NULL == plugins || NULL == plugins_count)
		return (EINVAL);

	(*plugins) = calloc(nitems(plugins_descr), sizeof(gm_plugin_t));
	if (NULL == (*plugins))
		return (ENOMEM);
	for (i = 0, j = 0; i < nitems(plugins_descr); i ++) {
		plugin = &(*plugins)[j];
		plugin->descr = plugins_descr[i];
		if (NULL != plugin->descr->init) {
			if (0 != plugin->descr->init(plugin))
				continue;
			/* Init change detect. */
			if (NULL != plugin->descr->is_def_dev_changed) {
				plugin->descr->is_def_dev_changed(plugin);
			}
			if (NULL != plugin->descr->is_list_devs_changed) {
				plugin->descr->is_list_devs_changed(plugin);
			}
		}
		j ++;
	}
	(*plugins_count) = j;

	return (0);
}

void
gmp_uninit(gm_plugin_p plugins, const size_t plugins_count) {
	gm_plugin_p plugin;

	if (NULL == plugins || 0 == plugins_count)
		return;

	for (size_t i = 0; i < plugins_count; i ++) {
		plugin = &plugins[i];
		if (NULL == plugin->descr->uninit)
			continue;
		plugin->descr->uninit(plugin);
	}
	free(plugins);
}


int
gmp_is_def_dev_changed(gm_plugin_p plugins, const size_t plugins_count) {
	gm_plugin_p plugin;

	if (NULL == plugins || 0 == plugins_count)
		return (0);

	for (size_t i = 0; i < plugins_count; i ++) {
		plugin = &plugins[i];
		/* TODO: if sound backend does not support
		 * is_def_dev_changed() but support dev_is_default()
		 * then we must call list_devs() and compare resuls with
		 * cached devices list to detect changes. */
		if (NULL == plugin->descr->is_def_dev_changed)
			continue;
		if (0 != plugin->descr->is_def_dev_changed(plugin))
			return (1);
	}

	return (0);
}


int
gmp_list_devs(gm_plugin_p plugins, const size_t plugins_count,
    gmp_dev_list_p dev_list) {
	int error;
	gm_plugin_p plugin;

	if (NULL == plugins || NULL == dev_list)
		return (EINVAL);
	if (0 == plugins_count)
		return (ENODEV);

	for (size_t i = 0; i < plugins_count; i ++) {
		plugin = &plugins[i];
		error = plugin->descr->list_devs(plugin, dev_list);
		if (0 != error) {
			gmp_dev_list_clear(dev_list);
			return (error);
		}
	}

	return (0);
}

void
gmp_dev_list_clear(gmp_dev_list_p dev_list) {
	gmp_dev_p dev;

	if (NULL == dev_list)
		return;

	for (size_t i = 0; i < dev_list->count; i ++) {
		dev = &dev_list->devs[i];
		gmp_dev_uninit(dev);
		if (NULL != dev->plugin->descr->dev_destroy) {
			dev->plugin->descr->dev_destroy(dev);
		}
		free((void*)dev->description);
		free((void*)dev->name);
	}
	dev_list->count = 0;
	free(dev_list->devs);
}

gmp_dev_p
gmp_dev_find_same(gmp_dev_list_p dev_list, gmp_dev_p dev) {
	gmp_dev_p cur_dev;

	if (NULL == dev_list)
		return (NULL);

	for (size_t i = 0; i < dev_list->count; i ++) {
		cur_dev = &dev_list->devs[i];
		if (dev->plugin != cur_dev->plugin)
			continue;
		if (0 != strcmp_safe(dev->name, cur_dev->name) ||
		    0 != strcmp_safe(dev->description, cur_dev->description))
			continue;
		return (cur_dev);
	}

	return (NULL);
}

int
gmp_dev_list_add(gm_plugin_p plugin, gmp_dev_list_p dev_list,
    gmp_dev_p dev) {
	gmp_dev_t gdd, *dev_new;

	if (NULL == plugin || NULL == dev_list || NULL == dev)
		return (EINVAL);

	gdd = (*dev);
	if (NULL == gdd.name || 0 == gdd.name[0]) {
		gdd.name = plugin->descr->name;
	}
	if (NULL == gdd.description || 0 == gdd.description[0]) {
		gdd.description = "";
	}

	dev_new = reallocarray(dev_list->devs,
	    (dev_list->count + 2),
	    sizeof(gmp_dev_t));
	if (NULL == dev_new)
		return (ENOMEM);
	dev_list->devs = dev_new;
	memset(&dev_list->devs[dev_list->count], 0x00, sizeof(gmp_dev_t));
	dev_list->devs[dev_list->count].name = strdup(gdd.name);
	dev_list->devs[dev_list->count].description = strdup(gdd.description);
	dev_list->devs[dev_list->count].plugin = plugin;
	dev_list->devs[dev_list->count].priv = gdd.priv;
	dev_list->count ++;

	return (0);
}

gmp_dev_p
gmp_dev_list_get_default(gmp_dev_list_p dev_list) {

	if (NULL == dev_list)
		return (NULL);
	for (size_t i = 0; i < dev_list->count; i ++) {
		if (gmp_dev_is_default(&dev_list->devs[i]))
			return (&dev_list->devs[i]);
	}

	return (NULL);
}


int
gmp_is_list_devs_changed(gm_plugin_p plugins, const size_t plugins_count) {
	gm_plugin_p plugin;

	if (NULL == plugins || 0 == plugins_count)
		return (0);

	for (size_t i = 0; i < plugins_count; i ++) {
		plugin = &plugins[i];
		/* TODO: if sound backend does not support
		 * is_list_devs_changed() then we must call
		 * list_devs() and compare resuls with
		 * cached devices list to detect changes. */
		if (NULL == plugin->descr->is_list_devs_changed)
			continue;
		if (0 != plugin->descr->is_list_devs_changed(plugin))
			return (1);
	}

	return (0);
}


int
gmp_dev_init(gmp_dev_p dev) {
	int error;

	if (NULL == dev)
		return (EINVAL);

	if (NULL != dev->plugin->descr->dev_init) {
		error = dev->plugin->descr->dev_init(dev);
		if (0 != error)
			return (error);
	}

	return (gmp_dev_read(dev, 1));
}

void
gmp_dev_uninit(gmp_dev_p dev) {
	gmp_dev_line_p dev_line;

	if (NULL == dev)
		return;

	if (NULL != dev->plugin->descr->dev_uninit) {
		dev->plugin->descr->dev_uninit(dev);
	}

	if (NULL != dev->lines) {
		for (size_t i = 0; i < dev->lines_count; i ++) {
			dev_line = &dev->lines[i];
			if (NULL != dev->plugin->descr->dev_line_destroy) {
				dev->plugin->descr->dev_line_destroy(dev,
				    dev_line);
			}
			free((void*)dev_line->display_name);
		}
		free(dev->lines);
		dev->lines = NULL;
	}
	dev->lines_count = 0;
}


int
gmp_dev_is_default(gmp_dev_p dev) {

	if (NULL == dev ||
	    NULL == dev->plugin->descr->dev_is_default)
		return (0);
	return (dev->plugin->descr->dev_is_default(dev));
}

int
gmp_dev_set_default(gmp_dev_p dev) {

	if (NULL == dev)
		return (EINVAL);
	if (NULL == dev->plugin->descr->dev_set_default)
		return (0);
	return (dev->plugin->descr->dev_set_default(dev));
}


int
gmp_dev_read(gmp_dev_p dev, int force) {
	int error;
	gmp_dev_line_p dev_line;
	gmp_dev_line_state_t state_muted, state;

	if (NULL == dev)
		return (EINVAL);

	memset(&state_muted, 0x00, sizeof(state_muted));
	for (size_t i = 0; i < dev->lines_count; i ++) {
		dev_line = &dev->lines[i];
		if (0 == force && 0 == dev_line->read_required)
			continue;
		/* Prepare to read. */
		memset(&state, 0x00, sizeof(state));
		state.is_enabled = dev_line->state.is_enabled;
		/* Read. */
		error = dev->plugin->descr->dev_line_read(dev, dev_line,
		    &state);
		/* Handle errors. */
		if (0 != error)
			return (error);
		gmp_dev_line_state_vol_normalize(&state, dev_line->chan_map);
		dev_line->read_required = 0;
		/* Detect changes. */
		if (0 != dev_line->has_enable) {
			/* Backend support mute, compare whole state. */
			if (0 == memcmp(&state, &dev_line->state,
			    sizeof(state)))
				continue; /* No changes. */
		} else if (dev_line->state.is_enabled) {
			/* Umuted, detect vol changes. */
			if (0 == memcmp(state.chan_vol,
			    dev_line->state.chan_vol,
			    sizeof(state.chan_vol)))
				continue; /* No changes. */
		} else { /* Muted, is unmuted? */
			/* On mute all channels volumes must be 0. */
			if (0 == memcmp(state_muted.chan_vol,
			    state.chan_vol,
			    sizeof((state.chan_vol))))
				continue; /* No changes. */
			/* Mark as unmuted + updated. */
			state.is_enabled = 1;
		}
		/* Update device line state. */
		dev_line->is_updated = 1; /* Mark as updated. */
		memcpy(&dev_line->state, &state, sizeof(state));
	}

	return (0);
}

int
gmp_dev_write(gmp_dev_p dev, int force) {
	int error;
	gmp_dev_line_p dev_line;
	gmp_dev_line_state_t state_muted;

	if (NULL == dev)
		return (EINVAL);

	memset(&state_muted, 0x00, sizeof(state_muted));
	for (size_t i = 0; i < dev->lines_count; i ++) {
		dev_line = &dev->lines[i];
		if (0 == force && 0 == dev_line->write_required)
			continue;
		if (0 != dev_line->is_read_only)
			continue;
		/* Write. */
		if (0 == dev_line->state.is_enabled &&
		    0 == dev_line->has_enable) {
			/* Set volumes to zero to simulate line disable. */
			error = dev->plugin->descr->dev_line_write(dev,
			    dev_line, &state_muted);
		} else { /* Set actual levels. */
			gmp_dev_line_state_vol_normalize(&dev_line->state,
			    dev_line->chan_map);
			error = dev->plugin->descr->dev_line_write(dev,
			    dev_line, &dev_line->state);
		}
		/* Handle errors. */
		if (0 != error)
			return (error);
		dev_line->write_required = 0;
	}

	return (0);
}


int
gmp_dev_is_updated(gmp_dev_p dev) {

	if (NULL == dev || NULL == dev->lines)
		return (0);

	for (size_t i = 0; i < dev->lines_count; i ++) {
		if (0 != dev->lines[i].is_updated)
			return (1);
	}

	return (0);
}

size_t
gmp_dev_is_updated_clear(gmp_dev_p dev) {
	size_t ret = 0;

	if (NULL == dev || NULL == dev->lines)
		return (0);

	for (size_t i = 0; i < dev->lines_count; i ++) {
		ret += (0 != dev->lines[i].is_updated);
		dev->lines[i].is_updated = 0;
	}

	return (ret);
}


int
gmp_dev_line_add(gmp_dev_p dev, const char *display_name,
    gmp_dev_line_p *dev_line_ret) {
	char *dn;
	gmp_dev_line_p dev_line, dev_line_new;

	if (NULL == dev || NULL == display_name)
		return (EINVAL);

	dev_line_new = reallocarray(dev->lines, (dev->lines_count + 2),
	    sizeof(gmp_dev_line_t));
	if (NULL == dev_line_new)
		return (ENOMEM);
	dev->lines = dev_line_new;
	dev_line = &dev->lines[dev->lines_count];
	dev->lines_count ++;

	memset(dev_line, 0x00, sizeof(gmp_dev_line_t));
	/* Remove spaces from end. */
	dn = strdup(display_name);
	for (size_t i = strlen(dn); 0 < i; i --) {
		if (' ' != dn[(i - 1)])
			break;
		dn[(i - 1)] = 0x00;
	}
	dev_line->display_name = dn;

	if (NULL != dev_line_ret) {
		(*dev_line_ret) = dev_line;
	}

	return (0);
}

int
gmp_dev_line_vol_max_get(gmp_dev_line_p dev_line) {
	int ret = 0;

	if (NULL == dev_line)
		return (0);
	for (size_t i = 0; i < MIXER_CHANNELS_COUNT; i ++) {
		if (0 == (((((uint32_t)1) << i) & dev_line->chan_map)))
			continue;
		if (ret < dev_line->state.chan_vol[i]) {
			ret = dev_line->state.chan_vol[i];
		}
	}

	return (ret);
}

void
gmp_dev_line_vol_glob_set(gmp_dev_line_p dev_line, int vol_new) {

	if (NULL == dev_line)
		return;
	vol_new = volume_apply_limits(vol_new);
	for (size_t i = 0; i < MIXER_CHANNELS_COUNT; i ++) {
		if (0 == (((((uint32_t)1) << i) & dev_line->chan_map)))
			continue;
		dev_line->state.chan_vol[i] = vol_new;
	}
}

void
gmp_dev_line_vol_glob_add(gmp_dev_line_p dev_line, int vol) {

	if (NULL == dev_line)
		return;
	for (size_t i = 0; i < MIXER_CHANNELS_COUNT; i ++) {
		if (0 == (((((uint32_t)1) << i) & dev_line->chan_map)))
			continue;
		dev_line->state.chan_vol[i] = volume_apply_limits(
		    (vol + dev_line->state.chan_vol[i]));
	}
}

size_t
gmp_dev_line_chan_first(gmp_dev_line_p dev_line) {

	if (NULL == dev_line)
		return (MIXER_CHANNELS_COUNT);
	for (size_t i = 0; i < MIXER_CHANNELS_COUNT; i ++) {
		if (0 == (((((uint32_t)1) << i) & dev_line->chan_map)))
			continue;
		return (i);
	}

	return (MIXER_CHANNELS_COUNT);
}
size_t
gmp_dev_line_chan_next(gmp_dev_line_p dev_line, size_t cur) {

	if (NULL == dev_line)
		return (MIXER_CHANNELS_COUNT);
	for (size_t i = (cur + 1); i < MIXER_CHANNELS_COUNT; i ++) {
		if (0 == (((((uint32_t)1) << i) & dev_line->chan_map)))
			continue;
		return (i);
	}

	return (MIXER_CHANNELS_COUNT);
}
