#include <stdlib.h>

#include "bsp.h"
#include "container.h"
#include "particle.h"
#include "particles.h"


/* a "burst" (shockwave) particle type */
/* this doesn't draw anything, it just pushes neighbors away in an increasing radius */

#define BURST_FORCE		0.01f
#define BURST_MAX_LIFETIME	8

typedef struct _burst_ctxt_t {
	int	longevity;
	int	lifetime;
} burst_ctxt_t;


static int burst_init(particles_t *particles, const particles_conf_t *conf, particle_t *p)
{
	burst_ctxt_t	*ctxt = p->ctxt;

	ctxt->longevity = ctxt->lifetime = BURST_MAX_LIFETIME;
	p->props->virtual = 1;

	return 1;
}


static inline void thrust_part(particle_t *burst, particle_t *victim, float distance_sq)
{
	v3f_t	direction = v3f_sub(&victim->props->position, &burst->props->position);

	/* TODO: normalize is expensive, see about removing these. */
	direction = v3f_normalize(&direction);
	victim->props->direction = v3f_add(&victim->props->direction, &direction);
	victim->props->direction = v3f_normalize(&victim->props->direction);

	victim->props->velocity += BURST_FORCE;
}


typedef struct burst_sphere_t {
	particles_t		*particles;
	particle_t		*center, *last;
	til_fb_fragment_t	*fragment;
	float			radius_min;
	float			radius_max;
	unsigned		trace_matches:1;
	unsigned		trace_affected:1;
} burst_sphere_t;


static void burst_cb(bsp_t *bsp, list_head_t *occupants, void *_s)
{
	burst_sphere_t	*s = _s;
	bsp_occupant_t	*o;
	float		rmin_sq = s->radius_min * s->radius_min;
	float		rmax_sq = s->radius_max * s->radius_max;

	/* XXX: to avoid having a callback per-particle, bsp_occupant_t was
	 * moved to the public particle, and the particle-specific
	 * implementations directly perform bsp-accelerated searches.  Another
	 * wart caused by this is particles_bsp().
	 */
	list_for_each_entry(o, occupants, occupants) {
		particle_t	*p = container_of(o, particle_t, occupant);
		float		d_sq;

		if (p->props->virtual) {
			/* don't move virtual particles (includes ourself) */
			continue;
		}

		d_sq = v3f_distance_sq(&s->center->props->position, &p->props->position);

		if (d_sq > rmin_sq && d_sq < rmax_sq) {
			/* displace the part relative to the burst origin */
			thrust_part(s->center, p, d_sq);

			if (s->trace_affected) {
				particles_draw_line(s->particles, &s->last->props->position, &p->props->position, s->fragment);
				s->last = p;
			}
		}

		if (s->trace_matches) {
			particles_draw_line(s->particles, &s->last->props->position, &p->props->position, s->fragment);
			s->last = p;
		}
	}
}


static particle_status_t burst_sim(particles_t *particles, const particles_conf_t *conf, particle_t *p, til_fb_fragment_t *f)
{
	burst_ctxt_t	*ctxt = p->ctxt;
	bsp_t		*bsp = particles_bsp(particles);	/* XXX see note above about bsp_occupant_t */
	burst_sphere_t	s;

	if (!ctxt->longevity || (ctxt->longevity--) <= 0) {
		return PARTICLE_DEAD;
	}

	/* affect neighbors for the shock-wave */
	s.radius_min = (1.0f - ((float)ctxt->longevity / ctxt->lifetime)) * 0.075f;
	s.radius_max = s.radius_min + .01f;
	s.center = s.last = p;
	s.trace_matches = (conf->show_bsp_matches && !conf->show_bsp_matches_affected_only);
	s.trace_affected = (conf->show_bsp_matches && conf->show_bsp_matches_affected_only);
	s.particles = particles;
	s.fragment = f;
	bsp_search_sphere(bsp, &p->props->position, s.radius_min, s.radius_max, burst_cb, &s);

	return PARTICLE_ALIVE;
}


particle_ops_t	burst_ops = {
			.context_size = sizeof(burst_ctxt_t),
			.sim = burst_sim,
			.init = burst_init,
			.draw = NULL,
			.cleanup = NULL,
		};
