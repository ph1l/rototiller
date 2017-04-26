#ifndef _PARTICLE_H
#define _PARTICLE_H

#include "bsp.h"
#include "fb.h"
#include "v3f.h"

typedef struct particle_props_t {
	v3f_t		position;	/* position in 3d space */
	v3f_t		direction;	/* trajectory in 3d space */
	float		velocity;	/* linear velocity */
	float		mass;		/* mass of particle */
	float		drag;		/* drag of particle */
	int		of_use:1;	/* are these properties of use/meaningful? */
	int		virtual:1;	/* is this a virtual particle? (not to be moved or otherwise acted upon) */
} particle_props_t;

typedef enum particle_status_t {
	PARTICLE_ALIVE,
	PARTICLE_DEAD
} particle_status_t;

typedef struct particle_t particle_t;
typedef struct particles_t particles_t;

typedef struct particle_ops_t {
	unsigned		context_size;								/* size of the particle context (0 for none) */
	int			(*init)(particles_t *, particle_t *);					/* initialize the particle, called after allocating context (optional) */
	void			(*cleanup)(particles_t *, particle_t *);				/* cleanup function, called before freeing context (optional) */
	particle_status_t	(*sim)(particles_t *, particle_t *);					/* simulate the particle for another cycle (required) */
	void			(*draw)(particles_t *, particle_t *, int, int, fb_fragment_t *);	/* draw the particle, 3d->2d projection has been done already (optional) */
} particle_ops_t;

struct particle_t {
	bsp_occupant_t		occupant;	/* occupant node in the bsp tree */
	particle_props_t	*props;
	particle_ops_t		*ops;
	void			*ctxt;
};


//#define rand_within_range(_min, _max) ((rand() % (_max - _min)) + _min)
// the style of random number generator used by c libraries has less entropy in the lower bits meaning one shouldn't just use modulo, while this is slower, the results do seem a little different.
#define rand_within_range(_min, _max) (int)(((float)_min) + ((float)rand() / (float)RAND_MAX) * (_max - _min))

#define INHERIT_OPS	NULL
#define INHERIT_PROPS	NULL


static inline int particle_init(particles_t *particles, particle_t *p) {
	if (p->ops->init) {
		return p->ops->init(particles, p);
	}

	return 1;
}


static inline void particle_cleanup(particles_t *particles, particle_t *p) {
	if (p->ops->cleanup) {
		p->ops->cleanup(particles, p);
	}
}


static inline particle_status_t particle_sim(particles_t *particles, particle_t *p) {
	return p->ops->sim(particles, p);
}


static inline void particle_draw(particles_t *particles, particle_t *p, int x, int y, fb_fragment_t *f) {
	if (p->ops->draw) {
		p->ops->draw(particles, p, x, y, f);
	}
}


void particle_convert(particles_t *particles, particle_t *p, particle_props_t *props, particle_ops_t *ops);

#endif
