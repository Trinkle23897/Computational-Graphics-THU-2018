#ifndef __RENDER_H__
#define __RENDER_H__ 

#include "texture.hpp"
#include "scene.hpp"

std::pair<int, ld> find_intersect_simple(Ray ray) {
	ld t = INF;
	int id = -1;
	for (int i = 0; i < scene_num; ++i) {
		std::pair<ld, P3> tmp = scene[i]->intersect(ray);
		if (tmp.first < INF && tmp.second.len2() > eps && tmp.first < t) {
			t = tmp.first;
			id = i;
		}
	}
	return std::make_pair(id, t);
}

P3 pt_render(Ray ray, int dep, unsigned short *X){
	// printf("Dep %d\n",dep);
	// ray.print();
	int into = 0;
	std::pair<int, ld> intersect_result = find_intersect_simple(ray);
	if(intersect_result.first == -1)
		return P3();
	// printf("Hit #%d t = %lf ",intersect_result.first, intersect_result.second);
	Object* obj = scene[intersect_result.first];
	Texture&texture = obj->texture;
	P3 x = ray.get(intersect_result.second);
	std::pair<Refl_t, P3> feature = texture.getcol(x.z / 15, x.x / 15);
	P3 f = feature.second, n = obj->norm(x), nl = n.dot(ray.d) < 0 ? into = 1, n : -n;
	ld p = f.max();
	if(++dep > 5)
		if(erand48(X) < p) f /= p;
		else return texture.emission;
	if(feature.first == DIFF){
		ld r1 = 2 * PI * erand48(X), r2 = erand48(X), r2s = sqrt(r2);
		P3 w = nl, u=((fabs(w.x) > .1 ? P3(0, 1) : P3(1)) & w).norm(), v = w & u;
		P3 d = (u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2)).norm();
		return texture.emission + f.mult(pt_render(Ray(x, d), dep, X));
	}
	else{
		Ray reflray = Ray(x, ray.d.reflect(n));
		if (feature.first == SPEC){
			return texture.emission + f.mult(pt_render(reflray, dep, X));
		}
		else{
			P3 d = ray.d.refract(n, into ? 1 : texture.brdf, into ? texture.brdf : 1);
			if (d.len2() < eps) // Total internal reflection
				return texture.emission + f.mult(pt_render(reflray, dep, X));
			ld a = texture.brdf - 1, b = texture.brdf + 1;
			ld R0 = a * a / (b * b), c = 1 - (into ? -ray.d.dot(nl) : d.dot(n));
			ld Re = R0 + (1 - R0) * c * c  * c * c * c, Tr = 1 - Re;
			ld P = .25 + .5 * Re, RP = Re / P, TP = Tr / (1 - P);
			return texture.emission + f.mult(dep > 2 ? (erand48(X) < P ?   // Russian roulette
				pt_render(reflray, dep, X) * RP : pt_render(Ray(x, d), dep, X) * TP)
			  : pt_render(reflray, dep, X) * Re + pt_render(Ray(x, d), dep, X) * Tr);
		}
	}
}

#endif // __RENDER_H__
