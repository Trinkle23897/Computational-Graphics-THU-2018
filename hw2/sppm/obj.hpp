#ifndef __OBJ_H__
#define __OBJ_H__ 

#include "utils.hpp"
#include "ray.hpp"

class Object {
public:
	P3 emision, color;
	Object(P3 c, P3 e): color(c), emision(e) {}
	virtual std::pair<ld, P3> intersect(Ray) = 0;
	virtual std::pair<P3, P3> aabb() = 0;
};

class CubeObject: public Object {
//store (x0, y0, z0) - (x1, y1, z1)
public:
	P3 m0, m1;
	CubeObject(P3 m0_, P3 m1_, P3 color = P3(), P3 emission = P3()): 
		m0(min(m0_, m1_)), m1(max(m0_, m1_)), Object(color, emission) {}
	std::pair<ld, P3> intersect(Ray ray) {
		// If no intersect, then return (INF, (0,0,0))
		ld ft = INF, t;
		P3 fq = P3(), q;
		// x dir
		t = (m0.x - ray.o.x) / ray.d.x;
		if (0 < t && t < ft) {
			q = ray.o + t * ray.d;
			if (m0.y <= q.y && q.y <= m1.y && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		t = (m1.x - ray.o.x) / ray.d.x;
		if (0 < t && t < ft) {
			q = ray.o + t * ray.d;
			if (m0.y <= q.y && q.y <= m1.y && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		// y dir
		t = (m0.y - ray.o.y) / ray.d.y;
		if (0 < t && t < ft) {
			q = ray.o + t * ray.d;
			if (m0.x <= q.x && q.x <= m1.x && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		t = (m1.y - ray.o.y) / ray.d.y;
		if (0 < t && t < ft) {
			q = ray.o + t * ray.d;
			if (m0.x <= q.x && q.x <= m1.x && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		// z dir
		t = (m0.z - ray.o.z) / ray.d.z;
		if (0 < t && t < ft) {
			q = ray.o + t * ray.d;
			if (m0.x <= q.x && q.x <= m1.x && m0.y <= q.y && q.y <= m1.y)
				ft = t, fq = q;
		}
		t = (m1.z - ray.o.z) / ray.d.z;
		if (0 < t && t < ft) {
			q = ray.o + t * ray.d;
			if (m0.x <= q.x && q.x <= m1.x && m0.y <= q.y && q.y <= m1.y)
				ft = t, fq = q;
		}
		return std::make_pair(ft, fq);
	}
	std::pair<P3, P3> aabb() {
		return std::make_pair<m0, m1>;
	}
};

class SphereObject: public Object {
public:
	P3 o; 
	ld r;
	SphereObject(P3 o_, ld r_, P3 color = P3(), P3 emission = P3()):
		o(o_), r(r_), Object(color, emission) {}
	std::pair<ld, P3> intersect(Ray ray) {

	}
	std::pair<P3, P3> aabb() {
		return std::make_pair<o-r, o+r>;
	}
}

#endif // __OBJ_H__