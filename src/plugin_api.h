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


#ifndef __PLUGIN_API_H__
#define __PLUGIN_API_H__

#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>

#ifndef __unused
#	define __unused		__attribute__((__unused__))
#endif
#ifndef nitems
#	define nitems(__x)	(sizeof((__x)) / sizeof((__x)[0]))
#endif

#ifndef HAVE_REALLOCARRAY
static inline void *
reallocarray(void *ptr, const size_t nmemb, const size_t size) {
	size_t nmemb_size;

	nmemb_size = (nmemb * size);
	if (0 == nmemb_size) {
		if (0 != nmemb &&
		    0 != size) { /* Overflow. */
			errno = ENOMEM;
			return (NULL);
		}
		nmemb_size ++;
	} else if (((nmemb | size) & (SIZE_T_MAX << (sizeof(size_t) * 4))) &&
	    (nmemb_size / size) != nmemb) { /* size_t overflow. */
		errno = ENOMEM;
		return (NULL);
	}

	return (realloc(ptr, nmemb_size));
}
#endif


typedef struct gtk_mixer_plugin_s *gm_plugin_p;
typedef struct gtk_mixer_plugin_device_list_s *gmp_dev_list_p;
typedef struct gtk_mixer_plugin_device_s *gmp_dev_p;
typedef struct gtk_mixer_plugin_device_line_s *gmp_dev_line_p;
typedef struct gtk_mixer_plugin_device_line_state_s *gmp_dev_line_state_p;


/* Discribe plugin API. */
typedef struct gtk_mixer_plugin_description_s {
	const char *name;
	const char *description;
	int can_set_default_device; /* Can change system global default sound device. */

	/* Plugin level functions. */

	/* Optional. Initialize sound plugin. 0 = Ok. */
	int (*init)(gm_plugin_p plugin);

	/* Optional. Deinitialize sound plugin. */
	void (*uninit)(gm_plugin_p plugin);

	/* Optional. 0 - default sound device not changed since last call. */
	int (*is_def_dev_changed)(gm_plugin_p plugin);

	/* Get devices list. 0 - no error. */
	int (*list_devs)(gm_plugin_p plugin, gmp_dev_list_p devs);

	/* 0 - no change. If not defined - list_devs() will be called. */
	int (*is_list_devs_changed)(gm_plugin_p plugin);


	/* Plugin device level functions. */

	/* Sound device lines must be added on init. Called on device selected. */
	int (*dev_init)(gmp_dev_p dev);

	/* Optional. Called on device deselected. */
	void (*dev_uninit)(gmp_dev_p dev);

	/* Optional. Can be used to free device priv data that was set inside list_devs(). */
	void (*dev_destroy)(gmp_dev_p dev);

	/* Optional. non 0 - this is default sound device. */
	int (*dev_is_default)(gmp_dev_p dev);

	/* Optional. Make device as system default. 0 - no error. */
	int (*dev_set_default)(gmp_dev_p dev);

	/* Optional. Can be used to free device line priv data that was set inside dev_init(). */
	void (*dev_line_destroy)(gmp_dev_p dev, gmp_dev_line_p dev_line);

	/* Read from mixer dev line to app. 0 - no error. */
	int (*dev_line_read)(gmp_dev_p dev, gmp_dev_line_p dev_line,
	    gmp_dev_line_state_p line_state);
	/* Write from app to mixer dev line. 0 - no error. */
	int (*dev_line_write)(gmp_dev_p dev, gmp_dev_line_p dev_line,
	    gmp_dev_line_state_p line_state);
} gmp_descr_t, *gmp_descr_p;


typedef struct gtk_mixer_plugin_s {
	const gmp_descr_t *descr;
	void 		*priv; /* Plugin internal. */
} gm_plugin_t, *gm_plugin_p;



/* From: https://en.wikipedia.org/wiki/Surround_sound */
#define MIXER_CHANNELS_COUNT	18
enum {
	MIXER_CHANNEL_FL = 0,
	MIXER_CHANNEL_FR,
	MIXER_CHANNEL_FC,
	MIXER_CHANNEL_LFE,
	MIXER_CHANNEL_BL,
	MIXER_CHANNEL_BR,
	MIXER_CHANNEL_FLC,
	MIXER_CHANNEL_FRC,
	MIXER_CHANNEL_BC,
	MIXER_CHANNEL_SL,
	MIXER_CHANNEL_SR,
	MIXER_CHANNEL_TC,
	MIXER_CHANNEL_TFL,
	MIXER_CHANNEL_TFC,
	MIXER_CHANNEL_TFR,
	MIXER_CHANNEL_TBL,
	MIXER_CHANNEL_TBC,
	MIXER_CHANNEL_TBR
};

static const char *channel_name_long[MIXER_CHANNELS_COUNT] = {
	"Front Left",
	"Front Right",
	"Front Center",
	"Low Frequency",
	"Back Left",
	"Back Right",
	"Front Left of Center",
	"Front Right of Center",
	"Back Center",
	"Side Left",
	"Side Right",
	"Top Center",
	"Front Left Height",
	"Front Center Height",
	"Front Right Height",
	"Rear Left Height",
	"Rear Center Height",
	"Rear Right Height",
};

static const char *channel_name_short[MIXER_CHANNELS_COUNT] = {
	"FL",
	"FR",
	"FC",
	"LFE",
	"BL",
	"BR",
	"FLC",
	"FRC",
	"BC",
	"SL",
	"SR",
	"TC",
	"TFL",
	"TFC",
	"TFR",
	"TBL",
	"TBC"
	"TBR"
};


/* Soundcard line: Main, pcm, cd, video... */

/* This is for read/write. */
#define GMPDL_CHAN_VOL_INVALID	0xffffff
typedef struct gtk_mixer_plugin_device_line_state_s {
	int chan_vol[MIXER_CHANNELS_COUNT]; /* Volume level per channel: 0-100. */
	int is_enabled; /* 0 - is line muted / record disabled. */
} gmp_dev_line_state_t;

/* Keep all soundcard line data. */
typedef struct gtk_mixer_plugin_device_line_s {
	/* Set by plugin. */
	const char *display_name; /* Line name to display: main, pcm, mic... */
	void *priv; /* Plugin internal per device line. */
	uint32_t chan_map; /* Bitmask for avail channels. */
	size_t chan_vol_count; /* Actual channels count. popcnt(chan_map) */
	int is_capture; /* Device is capture else playback. */
	int is_read_only; /* Only display values, no set. */
	int has_enable; /* Line can be enabled/disabled. (muted) */

	/* Used by app. */
	gmp_dev_line_state_t state;
	ssize_t is_updated; /* State changed, need GUI update. -1 = set from gui, 1 = set from backend. */
	size_t read_required; /* Plugin must read state from mixer. */
	size_t write_required; /* Plugin must write state to mixer. */
} gmp_dev_line_t, *gmp_dev_line_p;


/* Soundcard. */

/* Keep all soundcard device data. */
typedef struct gtk_mixer_plugin_device_s {
	/* Set by plugin. */
	const char *name; /* Symbolic name. Make it unique! */
	const char *description; /* Verbose human readable name. */
	gm_plugin_p plugin;
	void *priv; /* Plugin internal per device. Handle it on dev_init()/dev_uninit() or list_devs()/dev_destroy(). */

	/* Used by app. */
	gmp_dev_line_p lines; /* Auto destroy on dev_uninit. */
	size_t lines_count;
} gmp_dev_t, *gmp_dev_p;

typedef struct gtk_mixer_plugin_device_list_s {
	gmp_dev_p devs;
	size_t count;
} gmp_dev_list_t, *gmp_dev_list_p;



int gmp_init(gm_plugin_p *plugins, size_t *plugins_count);
void gmp_uninit(gm_plugin_p plugins, const size_t plugins_count);

int gmp_is_def_dev_changed(gm_plugin_p plugins, const size_t plugins_count);

int gmp_list_devs(gm_plugin_p plugins, const size_t plugins_count,
    gmp_dev_list_p dev_list);
/* Does not free dev_list, work only with stored data. */
void gmp_dev_list_clear(gmp_dev_list_p dev_list);
int gmp_dev_list_add(gm_plugin_p plugin, gmp_dev_list_p dev_list,
    gmp_dev_p dev);
gmp_dev_p gmp_dev_list_get_default(gmp_dev_list_p dev_list);

int gmp_is_list_devs_changed(gm_plugin_p plugins, const size_t plugins_count);

int gmp_dev_init(gmp_dev_p dev);
void gmp_dev_uninit(gmp_dev_p dev);

/* Is mixer dev default?. */
int gmp_dev_is_default(gmp_dev_p dev);
/* Make mixer dev default. */
int gmp_dev_set_default(gmp_dev_p dev);

/* Read from mixer dev. */
int gmp_dev_read(gmp_dev_p dev, int force);
/* Write to mixer dev new values. */
int gmp_dev_write(gmp_dev_p dev, int force);

/* Return 1 if at least 1 line gui update required. */
int gmp_dev_is_updated(gmp_dev_p dev);
/* Clear updated_clear, that was set by gmp_dev_read().
 * Return changes count. */
size_t gmp_dev_is_updated_clear(gmp_dev_p dev);

/* Add line to device. */
int gmp_dev_line_add(gmp_dev_p dev, const char *display_name,
    gmp_dev_line_p *dev_line_ret);

/* Get max channel volume per line. */
int gmp_dev_line_vol_max_get(gmp_dev_line_p dev_line);
/* Set volume to all channels on line. */
void gmp_dev_line_vol_glob_set(gmp_dev_line_p dev_line, int vol_new);
/* Increment/decrement volume. */
void gmp_dev_line_vol_glob_add(gmp_dev_line_p dev_line, int vol);
/* Get first/next channel index on line. */
size_t gmp_dev_line_chan_first(gmp_dev_line_p dev_line);
size_t gmp_dev_line_chan_next(gmp_dev_line_p dev_line, size_t cur);


#ifdef HAVE_OSS
extern const gmp_descr_t plugin_oss3;
#endif
#ifdef HAVE_ALSA
extern const gmp_descr_t plugin_alsa;
#endif

static const gmp_descr_t * const plugins_descr[] = {
#ifdef HAVE_OSS
	&plugin_oss3,
#endif
#ifdef HAVE_ALSA
	&plugin_alsa,
#endif
};


#endif /* __PLUGIN_API_H__ */
