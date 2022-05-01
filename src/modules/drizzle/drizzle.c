/*
 *  Copyright (C) 2020 - Vito Caputo - <vcaputo@pengaru.com>
 *
 *  This program is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "til.h"
#include "til_fb.h"

#include "puddle/puddle.h"

#define PUDDLE_SIZE		512
#define DRIZZLE_CNT		20
#define DEFAULT_VISCOSITY	.01

typedef struct v3f_t {
	float	x, y, z;
} v3f_t;

typedef struct v2f_t {
	float	x, y;
} v2f_t;

typedef struct drizzle_setup_t {
	til_setup_t	til_setup;
	float		viscosity;
} drizzle_setup_t;

typedef struct drizzle_context_t {
	puddle_t	*puddle;
	unsigned	n_cpus;
	drizzle_setup_t	setup;
} drizzle_context_t;

static drizzle_setup_t drizzle_default_setup = {
	.viscosity = DEFAULT_VISCOSITY,
};


/* convert a color into a packed, 32-bit rgb pixel value (taken from libs/ray/ray_color.h) */
static inline uint32_t color_to_uint32(v3f_t color) {
	uint32_t	pixel;

	if (color.x > 1.0f) color.x = 1.0f;
	if (color.y > 1.0f) color.y = 1.0f;
	if (color.z > 1.0f) color.z = 1.0f;

	if (color.x < .0f) color.x = .0f;
	if (color.y < .0f) color.y = .0f;
	if (color.z < .0f) color.z = .0f;

	pixel = (uint32_t)(color.x * 255.0f);
	pixel <<= 8;
	pixel |= (uint32_t)(color.y * 255.0f);
	pixel <<= 8;
	pixel |= (uint32_t)(color.z * 255.0f);

	return pixel;
}


static void * drizzle_create_context(unsigned ticks, unsigned num_cpus, til_setup_t *setup)
{
	drizzle_context_t	*ctxt;

	if (!setup)
		setup = &drizzle_default_setup.til_setup;

	ctxt = calloc(1, sizeof(drizzle_context_t));
	if (!ctxt)
		return NULL;

	ctxt->puddle = puddle_new(PUDDLE_SIZE, PUDDLE_SIZE);
	if (!ctxt->puddle) {
		free(ctxt);
		return NULL;
	}

	ctxt->n_cpus = num_cpus;
	ctxt->setup = *(drizzle_setup_t *)setup;

	return ctxt;
}


static void drizzle_destroy_context(void *context)
{
	drizzle_context_t	*ctxt = context;

	puddle_free(ctxt->puddle);
	free(ctxt);
}


static int drizzle_fragmenter(void *context, unsigned n_cpus, const til_fb_fragment_t *fragment, unsigned number, til_fb_fragment_t *res_fragment)
{
	drizzle_context_t	*ctxt = context;

	return til_fb_fragment_slice_single(fragment, ctxt->n_cpus, number, res_fragment);
}


static void drizzle_prepare_frame(void *context, unsigned ticks, unsigned n_cpus, til_fb_fragment_t *fragment, til_fragmenter_t *res_fragmenter)
{
	drizzle_context_t	*ctxt = context;

	*res_fragmenter = drizzle_fragmenter;

	for (int i = 0; i < DRIZZLE_CNT; i++) {
		int	x = rand() % (PUDDLE_SIZE - 1);
		int	y = rand() % (PUDDLE_SIZE - 1);

		/* TODO: puddle should probably offer a normalized way of setting an
		 * area to a value, so if PUDDLE_SIZE changes this automatically
		 * would adapt to cover the same portion of the unit square...
		 */
		puddle_set(ctxt->puddle, x, y, 1.f);
		puddle_set(ctxt->puddle, x + 1, y, 1.f);
		puddle_set(ctxt->puddle, x, y + 1, 1.f);
		puddle_set(ctxt->puddle, x + 1, y + 1, 1.f);
	}

	puddle_tick(ctxt->puddle, ctxt->setup.viscosity);
}


static void drizzle_render_fragment(void *context, unsigned ticks, unsigned cpu, til_fb_fragment_t *fragment)
{
	drizzle_context_t	*ctxt = context;
	float			xf = 1.f / (float)fragment->frame_width;
	float			yf = 1.f / (float)fragment->frame_height;
	v2f_t			coord;

	coord.y = yf * (float)fragment->y;
	for (int y = fragment->y; y < fragment->y + fragment->height; y++) {

		coord.x = xf * (float)fragment->x;
		for (int x = fragment->x; x < fragment->x + fragment->width; x++) {
			v3f_t		color = {};
			uint32_t	pixel;

			color.z = puddle_sample(ctxt->puddle, &coord);

			pixel = color_to_uint32(color);
			til_fb_fragment_put_pixel_unchecked(fragment, x, y, pixel);

			coord.x += xf;
		}

		coord.y += yf;
	}
}


static int drizzle_setup(const til_settings_t *settings, til_setting_t **res_setting, const til_setting_desc_t **res_desc, til_setup_t **res_setup)
{
	const char	*viscosity;
	const char	*values[] = {
				".005",
				".01",
				".03",
				".05",
				NULL
			};
	int		r;

	r = til_settings_get_and_describe_value(settings,
						&(til_setting_desc_t){
							.name = "Puddle viscosity",
							.key = "viscosity",
							.regex = "\\.[0-9]+",
							.preferred = TIL_SETTINGS_STR(DEFAULT_VISCOSITY),
							.values = values,
							.annotations = NULL
						},
						&viscosity,
						res_setting,
						res_desc);
	if (r)
		return r;

	if (res_setup) {
		drizzle_setup_t	*setup;

		setup = til_setup_new(sizeof(*setup), (void(*)(til_setup_t *))free);
		if (!setup)
			return -ENOMEM;

		sscanf(viscosity, "%f", &setup->viscosity);

		*res_setup = &setup->til_setup;
	}

	return 0;
}


til_module_t	drizzle_module = {
	.create_context = drizzle_create_context,
	.destroy_context = drizzle_destroy_context,
	.prepare_frame = drizzle_prepare_frame,
	.render_fragment = drizzle_render_fragment,
	.name = "drizzle",
	.description = "Classic 2D rain effect (threaded (poorly))",
	.author = "Vito Caputo <vcaputo@pengaru.com>",
	.setup = drizzle_setup,
};
