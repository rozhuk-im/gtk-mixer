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
#if defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#define HAS_FEATURE_DEFAULT_DEV 1
#endif
#include <sys/ioctl.h>
#include <sys/stat.h>
#if defined(__OpenBSD__)
	#include <soundcard.h>
#else
	#include <sys/soundcard.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "plugin_api.h"


#define PATH_DEV_MIXER	"/dev/mixer"
#define MIXER_ALLOC_CNT	8

static const char *oss_line_labels[] = SOUND_DEVICE_LABELS;

enum {
	MIXER_STATE_RECSRC = 0,
	MIXER_STATE_DEVMASK,
	MIXER_STATE_RECMASK,
	MIXER_STATE_CAPS,
	MIXER_STATE_STEREODEVS
};

static const uint32_t mixer_state_id[] = {
	SOUND_MIXER_RECSRC, /* 1 bit per recording source. */
	SOUND_MIXER_DEVMASK, /* 1 bit per supported device. */
	SOUND_MIXER_RECMASK, /* 1 bit per supp. recording source. */
	SOUND_MIXER_CAPS, /* Flags: SOUND_CAP_EXCL_INPUT - Only 1 rec. src at a time. */
	SOUND_MIXER_STEREODEVS, /* Mixer channels supporting stereo. */
};

typedef struct oss_device_context_s {
	int state[nitems(mixer_state_id)];
	int dev_index;
} oss_dev_ctx_t, *oss_dev_ctx_p;


/* Mixer device state: to track device change: connect/disconnect. */
typedef struct oss_mixer_device_state_s {
	dev_t		st_dev; /* inode's device */
	ino_t		st_ino; /* inode's number */
	struct timespec	st_mtim; /* time of last data modification */
} oss_dev_state_t, *oss_dev_state_p;

typedef struct oss_context_s {
	int def_dev_index;
	oss_dev_state_p dev_state;
	size_t dev_state_count;
} oss_ctx_t, *oss_ctx_p;


static int
oss_init(gm_plugin_p plugin) {

	if (NULL == plugin)
		return (EINVAL);

	plugin->priv = calloc(1, sizeof(oss_ctx_t));
	if (NULL == plugin->priv)
		return (ENOMEM);

	return (0);
}

static void
oss_uninit(gm_plugin_p plugin) {
	oss_ctx_p oss_ctx;

	if (NULL == plugin || NULL == plugin->priv)
		return;

	oss_ctx = plugin->priv;

	free(oss_ctx->dev_state);
	free(plugin->priv);
	plugin->priv = NULL;
}


#ifdef HAS_FEATURE_DEFAULT_DEV
static int
oss_default_dev_get(void) {
	int dev_idx = -1;
	size_t sz;

	/* Default sound device. */
	sz = sizeof(int);
	if (0 != sysctlbyname("hw.snd.default_unit", &dev_idx, &sz,
	    NULL, 0)) {
		return (-1);
	}

	return (dev_idx);
}
static int
oss_default_dev_set(int dev_idx) {
	size_t sz = sizeof(int);

	if (0 != sysctlbyname("hw.snd.default_unit", NULL, 0,
	    &dev_idx, sz)) {
		return (-1);
	}

	return (0);
}

static int
oss_is_def_dev_changed(gm_plugin_p plugin) {
	int ret, new_dev;
	oss_ctx_p oss_ctx;

	if (NULL == plugin || NULL == plugin->priv)
		return (0);

	oss_ctx = plugin->priv;
	new_dev = oss_default_dev_get();
	ret = (oss_ctx->def_dev_index != new_dev);
	oss_ctx->def_dev_index = new_dev;

	return (ret);
}
#endif


static void
device_descr_get(size_t dev_idx, char *buf, size_t buf_size) {
	size_t tmp = 0;
#if defined(__DragonFly__) || defined(__FreeBSD__)
	char dev_path[32];

	tmp = (buf_size - 1);
	snprintf(dev_path, sizeof(dev_path), "dev.pcm.%zu.%%desc",
	    dev_idx);
	if (0 != sysctlbyname(dev_path, buf, &tmp, NULL, 0)) {
		tmp = 0;
	}
#elif defined(SOUND_MIXER_INFO)
	char dev_path[32];
	mixer_info mi;

	snprintf(dev_path, sizeof(dev_path), PATH_DEV_MIXER"%zu",
	    dev_idx);
	int fd = open(dev_path, O_RDONLY);
	if (0 == ioctl(fd, SOUND_MIXER_INFO, &mi)) {
		strncpy(buf, mi.name, buf_size);
		tmp = (buf_size - 1);
	}
	close(fd);
#endif
	buf[tmp] = 0x00;
}
static int
oss_list_devs(gm_plugin_p plugin, gmp_dev_list_p dev_list) {
	int error;
	size_t i, fail_cnt;
	struct stat st;
	char dev_path[32], dev_descr[256];
	gmp_dev_t dev = { .name = dev_path, .description = dev_descr };
	oss_dev_ctx_p dev_ctx;

	if (NULL == plugin || NULL == dev_list)
		return (EINVAL);

	/* Auto detect. */
	for (i = 0, fail_cnt = 0; fail_cnt < MIXER_ALLOC_CNT; i ++, fail_cnt ++) {
		snprintf(dev_path, sizeof(dev_path),
		    PATH_DEV_MIXER"%zu", i);
		if (0 != stat(dev_path, &st))
			continue;
		device_descr_get(i, dev_descr, sizeof(dev_descr));
		dev_ctx = calloc(1, sizeof(oss_dev_ctx_t));
		if (NULL == dev_ctx)
			return (ENOMEM);
		dev_ctx->dev_index = (int)i;
		dev.priv = dev_ctx;
		error = gmp_dev_list_add(plugin, dev_list, &dev);
		if (0 != error) {
			free(dev.priv);
			return (error);
		}
		fail_cnt = 0; /* Reset fail counter. */
	}

	return (0);
}

static int
oss_is_list_devs_changed(gm_plugin_p plugin) {
	int ret = 0;
	size_t i, fail_cnt;
	struct stat st;
	char dev_path[32];
	oss_ctx_p oss_ctx;
	oss_dev_state_p dstate, dev_state_new;

	if (NULL == plugin || NULL == plugin->priv)
		return (0);

	oss_ctx = plugin->priv;
	/* Calculate count and latest mod time. */
	for (i = 0, fail_cnt = 0; fail_cnt < MIXER_ALLOC_CNT; i ++, fail_cnt ++) {
		if (oss_ctx->dev_state_count <= i) {
			dev_state_new = reallocarray(oss_ctx->dev_state,
			    (oss_ctx->dev_state_count + MIXER_ALLOC_CNT),
			    sizeof(oss_dev_state_t));
			if (NULL == dev_state_new)
				return (1); /* Assume that devices has changes. */
			/* Zeroize new mem. */
			memset(&dev_state_new[oss_ctx->dev_state_count],
			    0x00,
			    (MIXER_ALLOC_CNT * sizeof(oss_dev_state_t)));
			oss_ctx->dev_state = dev_state_new;
			oss_ctx->dev_state_count += MIXER_ALLOC_CNT;
		}
		dstate = &oss_ctx->dev_state[i];
		snprintf(dev_path, sizeof(dev_path),
		    PATH_DEV_MIXER"%zu", i);
		memset(&st, 0x00, sizeof(st));
		if (0 == stat(dev_path, &st)) {
			fail_cnt = 0; /* Reset fail counter. */
		}
		if (dstate->st_dev == st.st_dev &&
		    dstate->st_ino == st.st_ino &&
		    dstate->st_mtim.tv_nsec == st.st_mtim.tv_nsec &&
		    dstate->st_mtim.tv_sec == st.st_mtim.tv_sec)
			continue; /* Not changed. */
		/* Update. */
		dstate->st_dev = st.st_dev;
		dstate->st_ino = st.st_ino;
		dstate->st_mtim = st.st_mtim;
		ret ++;
	}

	return (ret);
}


static int
oss_dev_init(gmp_dev_p dev) {
	int error, fd, chan_mask;
	oss_dev_ctx_p dev_ctx;
	gmp_dev_line_p dev_line;

	if (NULL == dev)
		return (EINVAL);

	dev_ctx = dev->priv;

	/* State, caps, settings. */
	fd = open(dev->name, O_RDONLY);
	if (-1 == fd) {
		error = errno;
		goto err_out;
	}
	for (size_t i = 0; i < nitems(mixer_state_id); i ++) {
		if (-1 == ioctl(fd, MIXER_READ(mixer_state_id[i]),
		    &dev_ctx->state[i])) {
			dev_ctx->state[i] = 0;
		}
	}
	close(fd);

	/* Lines / channels add. */
	for (size_t i = 0; i < SOUND_MIXER_NRDEVICES; i ++) {
		chan_mask = (((int)1) << i);
		if (0 == (chan_mask & dev_ctx->state[MIXER_STATE_DEVMASK]))
			continue;
		/* Add line. */
		error = gmp_dev_line_add(dev, oss_line_labels[i],
		    &dev_line);
		if (0 != error)
			goto err_out;
		dev_line->priv = (void*)i; /* Store line index. */
		dev_line->chan_map = (((uint32_t)1) << MIXER_CHANNEL_FL);
		dev_line->chan_vol_count = 1;
		if (0 != (chan_mask & dev_ctx->state[MIXER_STATE_STEREODEVS])) {
			dev_line->chan_map |= (((uint32_t)1) << MIXER_CHANNEL_FR);
			dev_line->chan_vol_count ++;
		}
		dev_line->is_capture = (0 != (chan_mask & dev_ctx->state[MIXER_STATE_RECMASK]));
		dev_line->is_read_only = 0;
		/* All rec lines can be disabled. */
		dev_line->has_enable = dev_line->is_capture;
	}

	return (0);

err_out:
	free(dev_ctx);

	return (error);
}

static void
oss_dev_destroy(gmp_dev_p dev) {

	if (NULL == dev)
		return;

	free(dev->priv);
	dev->priv = NULL;
}


#ifdef HAS_FEATURE_DEFAULT_DEV
static int
oss_dev_is_default(gmp_dev_p dev) {
	oss_dev_ctx_p dev_ctx;

	if (NULL == dev)
		return (0);

	dev_ctx = dev->priv;
	return (oss_default_dev_get() == dev_ctx->dev_index);
}

static int
oss_dev_set_default(gmp_dev_p dev) {
	oss_dev_ctx_p dev_ctx;

	if (NULL == dev)
		return (EINVAL);

	dev_ctx = dev->priv;
	return (oss_default_dev_set(dev_ctx->dev_index));
}
#endif


static int
oss_dev_line_read(gmp_dev_p dev, gmp_dev_line_p dev_line,
    gmp_dev_line_state_p line_state) {
	int error = 0, fd, chan_mask, vol;
	size_t chan_idx;
	oss_dev_ctx_p dev_ctx;

	if (NULL == dev || NULL == dev_line || NULL == line_state)
		return (EINVAL);

	dev_ctx = dev->priv;
	chan_idx = ((size_t)dev_line->priv);
	chan_mask = (((int)1) << chan_idx);
	if (0 == (chan_mask & dev_ctx->state[MIXER_STATE_DEVMASK]))
		return (EINVAL);

	/* Read state. */
	fd = open(dev->name, O_RDONLY);
	if (-1 == fd)
		return (errno);
	/* Volume level. */
	if (-1 == ioctl(fd, MIXER_READ(chan_idx), &vol)) {
		error = errno;
		goto err_out;
	}
	/* Map OSS to app values. */
	/* Left channel. */
	line_state->chan_vol[MIXER_CHANNEL_FL] = (vol & 0x7f);
	/* Right channel. */
	if (0 != (chan_mask & dev_ctx->state[MIXER_STATE_STEREODEVS])) {
		line_state->chan_vol[MIXER_CHANNEL_FR] = ((vol >> 8) & 0x7f);
	}
	/* Enabled state. */
	if (dev_line->is_capture) {
		if (-1 == ioctl(fd,
		    MIXER_READ(mixer_state_id[MIXER_STATE_RECSRC]),
		    &dev_ctx->state[MIXER_STATE_RECSRC])) {
			error = errno;
			goto err_out;
		}
		/* Map OSS to app values. */
		line_state->is_enabled = (0 != (chan_mask &
		    dev_ctx->state[MIXER_STATE_RECSRC]));
	}

err_out:
	close(fd);

	return (error);
}

static int
oss_dev_line_write(gmp_dev_p dev, gmp_dev_line_p dev_line,
    gmp_dev_line_state_p line_state) {
	int error = 0, fd, chan_mask, vol;
	size_t chan_idx;
	oss_dev_ctx_p dev_ctx;

	if (NULL == dev || NULL == dev_line || NULL == line_state)
		return (EINVAL);

	dev_ctx = dev->priv;
	chan_idx = ((size_t)dev_line->priv);
	chan_mask = (((int)1) << chan_idx);
	if (0 == (chan_mask & dev_ctx->state[MIXER_STATE_DEVMASK]))
		return (EINVAL);

	/* Map app to OSS values. */
	vol = line_state->chan_vol[MIXER_CHANNEL_FL];
	if (0 != (chan_mask & dev_ctx->state[MIXER_STATE_STEREODEVS])) {
		vol |= (line_state->chan_vol[MIXER_CHANNEL_FR] << 8);
	}
	/* Write state. */
	fd = open(dev->name, O_RDWR);
	if (-1 == fd)
		return (errno);
	/* Volume level. */
	if (-1 == ioctl(fd, MIXER_WRITE(chan_idx), &vol)) {
		error = errno;
		goto err_out;
	}
	/* Enabled state. */
	if (dev_line->is_capture) {
		/* Map OSS to app values. */
		if (0 != line_state->is_enabled) {
			dev_ctx->state[MIXER_STATE_RECSRC] |= chan_mask;
		} else {
			dev_ctx->state[MIXER_STATE_RECSRC] &= ~chan_mask;
		}
		if (-1 == ioctl(fd,
		    MIXER_WRITE(mixer_state_id[MIXER_STATE_RECSRC]),
		    &dev_ctx->state[MIXER_STATE_RECSRC])) {
			error = errno;
			goto err_out;
		}
		/* Read to ensure that it got aplly. */
		if (-1 == ioctl(fd,
		    MIXER_READ(mixer_state_id[MIXER_STATE_RECSRC]),
		    &dev_ctx->state[MIXER_STATE_RECSRC])) {
			error = errno;
			goto err_out;
		}
		/* Map OSS to app values. */
		line_state->is_enabled = (0 != (chan_mask &
		    dev_ctx->state[MIXER_STATE_RECSRC]));
	}

err_out:
	close(fd);

	return (error);
}

const gmp_descr_t plugin_oss3 = {
	.name		= "OSS",
	.description	= "OSSv3 Mixer driver plugin",
#ifdef HAS_FEATURE_DEFAULT_DEV
	.can_set_default_device = 1,
	.is_def_dev_changed = oss_is_def_dev_changed,
	.dev_is_default	= oss_dev_is_default,
	.dev_set_default= oss_dev_set_default,
#endif
	.init		= oss_init,
	.uninit		= oss_uninit,
	.list_devs	= oss_list_devs,
	.is_list_devs_changed= oss_is_list_devs_changed,
	.dev_init	= oss_dev_init,
	.dev_destroy	= oss_dev_destroy,
	.dev_line_read	= oss_dev_line_read,
	.dev_line_write	= oss_dev_line_write,
};
