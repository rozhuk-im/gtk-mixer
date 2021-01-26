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


static int
alsa_list_devs(gm_plugin_p plugin, gmp_dev_list_p dev_list) {
	int error, dev_index = -1;
	char dev_path[32];
	gmp_dev_t dev = { .name = dev_path };
	snd_ctl_t *ctl = NULL;
	snd_ctl_card_info_t *info = NULL;

	if (NULL == plugin || NULL == dev_list)
		return (EINVAL);

	/* Auto detect. */
	snd_ctl_card_info_alloca(&info);
	while (0 == snd_card_next(&dev_index) && -1 != dev_index) {
		snprintf(dev_path, sizeof(dev_path), "hw:%i", dev_index);
		if (snd_ctl_open(&ctl, dev_path, 0) < 0)
			continue;
		error = snd_ctl_card_info(ctl, info);
		snd_ctl_close(ctl);
		if (0 != error)
			continue;
		dev.description = snd_ctl_card_info_get_name(info);
		//snd_ctl_card_info_get_longname(info);
		//snd_ctl_card_info_get_mixername(info);
		//snd_ctl_card_info_get_components(info);
		dev.priv = (void*)(size_t)dev_index;
		error = gmp_dev_list_add(plugin, dev_list, &dev);
		if (0 != error)
			return (error);
		break;
	}

	return (0);
}

static int
alsa_dev_init(gmp_dev_p dev) {
	//gmp_dev_line_p dev_line;

	if (NULL == dev)
		return (EINVAL);

	return (0);
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
