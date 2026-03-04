/*-
 * Copyright (c) 2020-2024 Rozhuk Ivan <rozhuk.im@gmail.com>
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "plugin_api.h"


/* snd_mixer_selem_channel_id_t */
static const uint8_t alsa_ch_map[] = {
	MIXER_CHANNEL_FL,	/* Front left - SND_MIXER_SCHN_FRONT_LEFT */
	MIXER_CHANNEL_FR,	/* Front right - SND_MIXER_SCHN_FRONT_RIGHT */
	MIXER_CHANNEL_TBL,	/* Rear left - SND_MIXER_SCHN_REAR_LEFT */
	MIXER_CHANNEL_TBR,	/* Rear right - SND_MIXER_SCHN_REAR_RIGHT */
	MIXER_CHANNEL_FC,	/* Front center - SND_MIXER_SCHN_FRONT_CENTER */
	MIXER_CHANNEL_LFE,	/* Woofer - SND_MIXER_SCHN_WOOFER */
	MIXER_CHANNEL_SL,	/* Side Left - SND_MIXER_SCHN_SIDE_LEFT */
	MIXER_CHANNEL_SR,	/* Side Right - SND_MIXER_SCHN_SIDE_RIGHT */
	MIXER_CHANNEL_TBC	/* Rear Center - SND_MIXER_SCHN_REAR_CENTER */
};


static const char *ignored_device_list[] = {
	"cards",
	"center_lfe",
	"default",
	"dmix",
	"dsnoop",
	"file",
	"front",
	"hdmi",
	"hw",
	"iec958",
	"modem",
	"null",
#ifndef DEBUG
	"oss", /* We have OSS plugin in this project. */
#endif
	"phoneline",
	"plug",
	"plughw",
	"pulse",
	"rear",
	"samplerate",
	"shm",
	"side",
	"spdif",
	"surround40",
	"surround41",
	"surround50",
	"surround51",
	"surround71",
	"sysdefault",
	"tee",
};


static int
is_ignored_device(const char *name, const size_t name_size) {
	size_t dev_name_size;

	if (NULL == name ||
	    0 == name_size)
		return (1);
	for (size_t i = 0; i < nitems(ignored_device_list); i ++) {
		dev_name_size = strlen(ignored_device_list[i]);
		if (name_size < dev_name_size)
			continue;
		if (0 != memcmp(name, ignored_device_list[i], dev_name_size))
			continue;
		if (name_size == dev_name_size || ':' == name[dev_name_size])
			return 1;
	}

	return (0);
}

static int
alsa_list_devs(gm_plugin_p plugin, gmp_dev_list_p dev_list) {
	int error = 0, dev_index = -1;
	void **hints = NULL;
	char *name = NULL, *desc = NULL, dev_path[32];
	gmp_dev_t dev = { .name = dev_path };
	snd_ctl_t *ctl = NULL;
	snd_ctl_card_info_t *info = NULL;

	if (NULL == plugin || NULL == dev_list)
		return (EINVAL);

	if (0 != snd_ctl_card_info_malloc(&info))
		return (ENOMEM);

	/* Auto detect. */
	/* Physical sound cards. */
	while (0 == snd_card_next(&dev_index) && -1 != dev_index) {
		snprintf(dev_path, sizeof(dev_path), "hw:%i", dev_index);
		if (0 > snd_ctl_open(&ctl, dev_path, 0))
			continue;
		snd_ctl_card_info_clear(info);
		error = snd_ctl_card_info(ctl, info);
		snd_ctl_close(ctl);
		if (0 != error)
			continue;
		dev.description = snd_ctl_card_info_get_name(info);
		//snd_ctl_card_info_get_longname(info);
		//snd_ctl_card_info_get_mixername(info);
		//snd_ctl_card_info_get_components(info);
		dev.priv = strdup(dev.name);
		error = gmp_dev_list_add(plugin, dev_list, &dev);
		if (0 != error) {
			free(dev.priv);
			goto err_out;
		}
	}

	/* Virtual sound cards. */
	if (0 > snd_device_name_hint(-1, "pcm", &hints))
		goto err_out;
	for (size_t i = 0; NULL != hints[i]; i ++) {
		free(name);
		name = snd_device_name_get_hint(hints[i], "NAME");
		if (is_ignored_device(name, strlen(name)))
			continue;
		free(desc);
		desc = snd_device_name_get_hint(hints[i], "DESC");
		if (NULL == desc) {
			if (0 > snd_ctl_open(&ctl, name, 0))
				continue;
			snd_ctl_card_info_clear(info);
			error = snd_ctl_card_info(ctl, info);
			snd_ctl_close(ctl);
			if (0 != error)
				continue;
			desc = strdup(snd_ctl_card_info_get_name(info));
		}
		dev.name = name;
		dev.description = desc;
		dev.priv = strdup(dev.name);
		error = gmp_dev_list_add(plugin, dev_list, &dev);
		if (0 != error) {
			free(dev.priv);
			goto err_out;
		}
	}
	error = 0;

err_out:
	free(name);
	free(desc);
	snd_device_name_free_hint(hints);
	snd_ctl_card_info_free(info);

	return (error);
}

static int
alsa_dev_init(gmp_dev_p dev) {
	int error = 0;
	gmp_dev_line_p dev_line;
	snd_mixer_t *mixer = NULL;
	snd_mixer_elem_t *elem;

	if (NULL == dev)
		return (EINVAL);

	if (0 > snd_mixer_open(&mixer, 0))
		return (EINVAL);
	if (0 > snd_mixer_attach(mixer, dev->priv))
		goto err_out;
	if (0 > snd_mixer_selem_register(mixer, NULL, NULL))
		goto err_out;
	if (0 > snd_mixer_load(mixer))
		goto err_out;

	for (elem = snd_mixer_first_elem(mixer); NULL != elem;
	     elem = snd_mixer_elem_next(elem)) {
		if (0 == snd_mixer_selem_is_active(elem))
			continue;
		if (0 == snd_mixer_selem_has_playback_volume(elem) &&
		    0 == snd_mixer_selem_has_capture_volume(elem))
			continue;
		/* Add line. */
		error = gmp_dev_line_add(dev, snd_mixer_selem_get_name(elem),
		    &dev_line);
		if (0 != error)
			goto err_out;
		dev_line->priv = (void*)snd_mixer_selem_get_index(elem); /* Store line index. */
		for (int i = 0; i < (int)nitems(alsa_ch_map); i ++) {
			if (0 == snd_mixer_selem_has_playback_channel(elem, i) &&
			    0 == snd_mixer_selem_has_capture_channel(elem, i))
				continue;
			dev_line->chan_map |= (((uint32_t)1) << alsa_ch_map[i]);
			dev_line->chan_vol_count ++;
		}
		dev_line->is_capture = (0 != snd_mixer_selem_has_capture_volume(elem));
		dev_line->is_read_only = 0;
		dev_line->has_enable = snd_mixer_selem_has_playback_switch(elem);
	}

err_out:
	snd_mixer_close(mixer);

	return (error);
}

static void
alsa_dev_destroy(gmp_dev_p dev) {

	if (NULL == dev)
		return;

	free(dev->priv);
	dev->priv = NULL;
}


static int
alsa_dev_line_read(gmp_dev_p dev, gmp_dev_line_p dev_line,
    gmp_dev_line_state_p line_state) {
	int error = 0;

	if (NULL == dev || NULL == dev_line || NULL == line_state)
		return (EINVAL);

	return (error);
}

static int
alsa_dev_line_write(gmp_dev_p dev, gmp_dev_line_p dev_line,
    gmp_dev_line_state_p line_state) {
	int error = 0;

	if (NULL == dev || NULL == dev_line || NULL == line_state)
		return (EINVAL);

	return (error);
}

const gmp_descr_t plugin_alsa = {
	.name		= "ALSA",
	.description	= "ALSA Mixer driver plugin",
	.list_devs	= alsa_list_devs,
	.dev_init	= alsa_dev_init,
	.dev_destroy	= alsa_dev_destroy,
	.dev_line_read	= alsa_dev_line_read,
	.dev_line_write	= alsa_dev_line_write,
};
