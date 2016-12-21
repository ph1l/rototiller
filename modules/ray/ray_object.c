#include <assert.h>

#include "ray_object.h"
#include "ray_object_light.h"
#include "ray_object_plane.h"
#include "ray_object_point.h"
#include "ray_object_sphere.h"
#include "ray_ray.h"
#include "ray_scene.h"
#include "ray_surface.h"


/* Determine if a ray intersects object.
 * If the object is intersected, store where along the ray the intersection occurs in res_distance.
 */
int ray_object_intersects_ray(ray_object_t *object, ray_ray_t *ray, float *res_distance)
{
	switch (object->type) {
	case RAY_OBJECT_TYPE_SPHERE:
		return ray_object_sphere_intersects_ray(&object->sphere, ray, res_distance);

	case RAY_OBJECT_TYPE_POINT:
		return ray_object_point_intersects_ray(&object->point, ray, res_distance);

	case RAY_OBJECT_TYPE_PLANE:
		return ray_object_plane_intersects_ray(&object->plane, ray, res_distance);

	case RAY_OBJECT_TYPE_LIGHT:
		return ray_object_light_intersects_ray(&object->light, ray, res_distance);
	default:
		assert(0);
	}
}


/* Return the surface normal of object @ point */
ray_3f_t ray_object_normal(ray_object_t *object, ray_3f_t *point)
{
	switch (object->type) {
	case RAY_OBJECT_TYPE_SPHERE:
		return ray_object_sphere_normal(&object->sphere, point);

	case RAY_OBJECT_TYPE_POINT:
		return ray_object_point_normal(&object->point, point);

	case RAY_OBJECT_TYPE_PLANE:
		return ray_object_plane_normal(&object->plane, point);

	case RAY_OBJECT_TYPE_LIGHT:
		return ray_object_light_normal(&object->light, point);
	default:
		assert(0);
	}
}


/* Return the surface of object @ point */
ray_surface_t ray_object_surface(ray_object_t *object, ray_3f_t *point)
{
	switch (object->type) {
	case RAY_OBJECT_TYPE_SPHERE:
		return ray_object_sphere_surface(&object->sphere, point);

	case RAY_OBJECT_TYPE_POINT:
		return ray_object_point_surface(&object->point, point);

	case RAY_OBJECT_TYPE_PLANE:
		return ray_object_plane_surface(&object->plane, point);

	case RAY_OBJECT_TYPE_LIGHT:
		return ray_object_light_surface(&object->light, point);
	default:
		assert(0);
	}
}
