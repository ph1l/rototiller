#define SDL_MAIN_HANDLED
#include <assert.h>
#include <SDL.h>
#include <stdlib.h>
#include <errno.h>

#include "til_fb.h"
#include "til_settings.h"


/* sdl fb backend, everything sdl-specific in rototiller resides here. */

typedef struct sdl_fb_t {
	unsigned	width, height;
	Uint32		flags;

	SDL_Window	*window;
	SDL_Renderer	*renderer;
	SDL_Texture	*texture;
} sdl_fb_t;

typedef struct sdl_fb_page_t sdl_fb_page_t;

struct sdl_fb_page_t {
	SDL_Surface	*surface;
};


static int sdl_fb_setup(const til_settings_t *settings, til_setting_t **res_setting, const til_setting_desc_t **res_desc, til_setup_t **res_setup)
{
	const char	*fullscreen_values[] = {
				"off",
				"on",
				NULL
			};
	const char	*fullscreen;
	int		r;

	r = til_settings_get_and_describe_value(settings,
						&(til_setting_desc_t){
							.name = "SDL fullscreen mode",
							.key = "fullscreen",
							.regex = NULL,
							.preferred = fullscreen_values[0],
							.values = fullscreen_values,
							.annotations = NULL
						},
						&fullscreen,
						res_setting,
						res_desc);
	if (r)
		return r;

	if (!strcasecmp(fullscreen, "off")) {
		const char	*size;

		r = til_settings_get_and_describe_value(settings,
							&(til_setting_desc_t){
								.name = "SDL window size",
								.key = "size",
								.regex = "[1-9][0-9]*[xX][1-9][0-9]*",
								.preferred = "640x480",
								.values = NULL,
								.annotations = NULL
							},
							&size,
							res_setting,
							res_desc);
		if (r)
			return r;
	}

	return 0;
}

static int sdl_err_to_errno(int err)
{
	switch (err) {
	case SDL_ENOMEM:
		return ENOMEM;
	case SDL_EFREAD:
	case SDL_EFWRITE:
	case SDL_EFSEEK:
		return EIO;
	case SDL_UNSUPPORTED:
		return ENOTSUP;
	default:
		return EINVAL;
	}
}

static int sdl_fb_init(const til_settings_t *settings, void **res_context)
{
	const char	*fullscreen;
	const char	*size;
	sdl_fb_t	*c;
	int		r;

	assert(settings);
	assert(res_context);

	fullscreen = til_settings_get_value(settings, "fullscreen", NULL);
	if (!fullscreen)
		return -EINVAL;

	size = til_settings_get_value(settings, "size", NULL);
	if (!size && !strcasecmp(fullscreen, "off"))
		return -EINVAL;

	c = calloc(1, sizeof(sdl_fb_t));
	if (!c)
		return -ENOMEM;

	if (!strcasecmp(fullscreen, "on")) {
		if (!size)
			c->flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
		else
			c->flags = SDL_WINDOW_FULLSCREEN;
	}

	if (size) /* TODO: errors */
		sscanf(size, "%u%*[xX]%u", &c->width, &c->height);

	SDL_SetMainReady();
	r = SDL_Init(SDL_INIT_VIDEO);
	if (r < 0) {
		free(c);
		return -sdl_err_to_errno(r);
	}

	if (c->flags == SDL_WINDOW_FULLSCREEN_DESKTOP) {
		SDL_DisplayMode	mode;

		r = SDL_GetDesktopDisplayMode(0, &mode);
		if (r != 0) {
			SDL_Quit();
			free(c);
			return -sdl_err_to_errno(r);
		}

		c->width = mode.w;
		c->height = mode.h;
	}

	*res_context = c;

	return 0;
}


static void sdl_fb_shutdown(til_fb_t *fb, void *context)
{
	sdl_fb_t	*c = context;

	SDL_Quit();
	free(c);
}


static int sdl_fb_acquire(til_fb_t *fb, void *context, void *page)
{
	sdl_fb_t	*c = context;
	sdl_fb_page_t	*p = page;

	c->window = SDL_CreateWindow("rototiller", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, c->width, c->height, c->flags);
	if (!c->window)
		return -1;

	c->renderer = SDL_CreateRenderer(c->window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (!c->renderer)
		return -1;

	c->texture = SDL_CreateTexture(c->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, c->width, c->height);
	if (!c->texture)
		return -1;

	return 0;
}


static void sdl_fb_release(til_fb_t *fb, void *context)
{
	sdl_fb_t	*c = context;

	SDL_DestroyTexture(c->texture);
	SDL_DestroyRenderer(c->renderer);
	SDL_DestroyWindow(c->window);
}


static void * sdl_fb_page_alloc(til_fb_t *fb, void *context, til_fb_page_t *res_page)
{
	sdl_fb_t	*c = context;
	sdl_fb_page_t	*p;

	p = calloc(1, sizeof(sdl_fb_page_t));
	if (!p)
		return NULL;

	p->surface = SDL_CreateRGBSurface(0, c->width, c->height, 32, 0, 0, 0, 0);

	/* rototiller wants to assume all pixels to be 32-bit aligned, so prevent unaligning pitches */
	assert(!(p->surface->pitch & 0x3));

	*res_page =	(til_fb_page_t){
				.fragment.buf = p->surface->pixels,
				.fragment.width = c->width,
				.fragment.frame_width = c->width,
				.fragment.height = c->height,
				.fragment.frame_height = c->height,
				.fragment.pitch = p->surface->pitch >> 2,
				.fragment.stride = (p->surface->pitch >> 2) - c->width,
			};

	return p;
}


static int sdl_fb_page_free(til_fb_t *fb, void *context, void *page)
{
	sdl_fb_t	*c = context;
	sdl_fb_page_t	*p = page;

	SDL_FreeSurface(p->surface);
	free(p);

	return 0;
}


static int sdl_ready()
{
	SDL_Event	ev;

	/* It's important on Windows in particular to
	 * drain the event queue vs. just SDL_QuitRequested()
	 */
	while (SDL_PollEvent(&ev)) {
		if (ev.type == SDL_QUIT)
			return -EPIPE;
	}

	return 0;
}


static int sdl_fb_page_flip(til_fb_t *fb, void *context, void *page)
{
	sdl_fb_t	*c = context;
	sdl_fb_page_t	*p = page;
	int		r;

	r = sdl_ready();
	if (r < 0)
		return r;

	if (SDL_UpdateTexture(c->texture, NULL, p->surface->pixels, p->surface->pitch) < 0)
		return -1;

	if (SDL_RenderClear(c->renderer) < 0)
		return -1;

	if (SDL_RenderCopy(c->renderer, c->texture, NULL, NULL) < 0)
		return -1;

	SDL_RenderPresent(c->renderer);

	return 0;
}


til_fb_ops_t sdl_fb_ops = {
	.setup = sdl_fb_setup,
	.init = sdl_fb_init,
	.shutdown = sdl_fb_shutdown,
	.acquire = sdl_fb_acquire,
	.release = sdl_fb_release,
	.page_alloc = sdl_fb_page_alloc,
	.page_free = sdl_fb_page_free,
	.page_flip = sdl_fb_page_flip
};
