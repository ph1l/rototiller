#ifndef _TIL_H
#define _TIL_H

#include "til_fb.h"
#include "til_module_context.h"
#include "til_setup.h"

/* til_fragmenter_t produces fragments from an input fragment, num being the desired fragment for the current call.
 * return value of 1 means a fragment has been produced, 0 means num is beyond the end of fragments. */
typedef int (*til_fragmenter_t)(til_module_context_t *context, const til_fb_fragment_t *fragment, unsigned number, til_fb_fragment_t *res_fragment);

/* til_frame_plan_t is what til_module_t.prepare_frame() populates to return a fragmenter and any flags/rules */
typedef struct til_frame_plan_t {
	unsigned		cpu_affinity:1;	/* maintain a stable fragnum:cpu/thread mapping? (slower) */
	til_fragmenter_t	fragmenter;	/* fragmenter to use in rendering the frame */
} til_frame_plan_t;

typedef struct til_settings_t settings;
typedef struct til_setting_desc_t til_setting_desc_t;
typedef struct til_knob_t til_knob_t;

#define TIL_MODULE_OVERLAYABLE	1u

typedef struct til_module_t {
	til_module_context_t *	(*create_context)(unsigned seed, unsigned ticks, unsigned n_cpus, til_setup_t *setup);
	void			(*destroy_context)(til_module_context_t *context);
	void			(*prepare_frame)(til_module_context_t *context, unsigned ticks, til_fb_fragment_t *fragment, til_frame_plan_t *res_frame_plan);
	void			(*render_fragment)(til_module_context_t *context, unsigned ticks, unsigned cpu, til_fb_fragment_t *fragment);
	void			(*finish_frame)(til_module_context_t *context, unsigned ticks, til_fb_fragment_t *fragment);
	int			(*setup)(const til_settings_t *settings, til_setting_t **res_setting, const til_setting_desc_t **res_desc, til_setup_t **res_setup);
	size_t			(*knobs)(til_module_context_t *context, til_knob_t **res_knobs);
	char			*name;
	char			*description;
	char			*author;
	unsigned		flags;
} til_module_t;

int til_init(void);
void til_quiesce(void);
void til_shutdown(void);
const til_module_t * til_lookup_module(const char *name);
void til_get_modules(const til_module_t ***res_modules, size_t *res_n_modules);
void til_module_render(til_module_context_t *context, unsigned ticks, til_fb_fragment_t *fragment);
int til_module_create_context(const til_module_t *module, unsigned seed, unsigned ticks, unsigned n_cpus, til_setup_t *setup, til_module_context_t **res_context);
til_module_context_t * til_module_destroy_context(til_module_context_t *context);
int til_module_setup(til_settings_t *settings, til_setting_t **res_setting, const til_setting_desc_t **res_desc, til_setup_t **res_setup);
int til_module_randomize_setup(const til_module_t *module, til_setup_t **res_setup, char **res_arg);
int til_fragmenter_slice_per_cpu(til_module_context_t *context, const til_fb_fragment_t *fragment, unsigned number, til_fb_fragment_t *res_fragment);
int til_fragmenter_tile64(til_module_context_t *context, const til_fb_fragment_t *fragment, unsigned number, til_fb_fragment_t *res_fragment);

#endif
