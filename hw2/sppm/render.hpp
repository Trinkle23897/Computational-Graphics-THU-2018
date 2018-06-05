#ifndef __RENDER_H__
#define __RENDER_H__ 

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

#endif // __RENDER_H__bool intersect(const Ray&r, ld&t, int&id){
