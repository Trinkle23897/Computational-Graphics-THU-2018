#ifndef __OBJ_H__
#define __OBJ_H__ 

#include "utils.hpp"
#include "ray.hpp"

// enum Refl_t { DIFF, SPEC, REFR };

class Object {
public:
	Texture texture;
	Object(Texture t): texture(t) {}
	Object(Refl_t refl, P3 color, P3 emission, ld brdf, std::string tname):
		texture(tname, brdf, color, emission, refl) {}
	virtual std::pair<ld, P3> intersect(Ray) {puts("virtual error in intersect!");}
		// If no intersect, then return (INF, (0,0,0))
	virtual std::pair<P3, P3> aabb() {puts("virtual error in aabb!");}
	virtual P3 norm(P3) {puts("virtual error in norm!");}
		// return norm vec out of obj
};

class CubeObject: public Object {
//store (x0, y0, z0) - (x1, y1, z1)
public:
	P3 m0, m1;
	CubeObject(P3 m0_, P3 m1_, Texture t):
		m0(min(m0_, m1_)), m1(max(m0_, m1_)), Object(t) {}
	CubeObject(P3 m0_, P3 m1_, Refl_t refl, ld brdf = 1.5, P3 color = P3(), P3 emission = P3(), std::string tname = ""):
		m0(min(m0_, m1_)), m1(max(m0_, m1_)), Object(refl, color, emission, brdf, tname) {}
	virtual P3 norm(P3 p) {
		if (std::abs(p.x - m0.x) < eps || std::abs(p.x - m1.x) < eps)
			return P3(std::abs(p.x - m1.x) < eps ? 1 : -1, 0, 0);
		if (std::abs(p.y - m0.y) < eps || std::abs(p.y - m1.y) < eps)
			return P3(0, std::abs(p.y - m1.y) < eps ? 1 : -1, 0);
		if (std::abs(p.z - m0.z) < eps || std::abs(p.z - m1.z) < eps)
			return P3(0, 0, std::abs(p.z - m1.z) < eps ? 1 : -1);
		assert(1 == 0);
	}
	virtual std::pair<ld, P3> intersect(Ray ray) {
		ld ft = INF, t;
		P3 fq = P3(), q;
		// x dir
		t = (m0.x - ray.o.x) / ray.d.x;
		if (0 < t && t < ft) {
			q = ray.get(t);
			if (m0.y <= q.y && q.y <= m1.y && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		t = (m1.x - ray.o.x) / ray.d.x;
		if (0 < t && t < ft) {
			q = ray.get(t);
			if (m0.y <= q.y && q.y <= m1.y && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		// y dir
		t = (m0.y - ray.o.y) / ray.d.y;
		if (0 < t && t < ft) {
			q = ray.get(t);
			if (m0.x <= q.x && q.x <= m1.x && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		t = (m1.y - ray.o.y) / ray.d.y;
		if (0 < t && t < ft) {
			q = ray.get(t);
			if (m0.x <= q.x && q.x <= m1.x && m0.z <= q.z && q.z <= m1.z)
				ft = t, fq = q;
		}
		// z dir
		t = (m0.z - ray.o.z) / ray.d.z;
		if (0 < t && t < ft) {
			q = ray.get(t);
			if (m0.x <= q.x && q.x <= m1.x && m0.y <= q.y && q.y <= m1.y)
				ft = t, fq = q;
		}
		t = (m1.z - ray.o.z) / ray.d.z;
		if (0 < t && t < ft) {
			q = ray.get(t);
			if (m0.x <= q.x && q.x <= m1.x && m0.y <= q.y && q.y <= m1.y)
				ft = t, fq = q;
		}
		return std::make_pair(ft, fq);
	}
	virtual std::pair<P3, P3> aabb() {
		return std::make_pair(m0, m1);
	}
};

class SphereObject: public Object {
public:
	P3 o; 
	ld r;
	SphereObject(P3 o_, ld r_, Texture t):
		o(o_), r(r_), Object(t) {}
	SphereObject(P3 o_, ld r_, Refl_t refl, ld brdf = 1.5, P3 color = P3(), P3 emission = P3(), std::string tname = ""):
		o(o_), r(r_), Object(refl, color, emission, brdf, tname) {}
	virtual std::pair<ld, P3> intersect(Ray ray) {
		P3 ro = o - ray.o;
		ld b = ray.d.dot(ro);
		ld d = b * b - ro.dot(ro) + r * r;
		if (d < 0) return std::make_pair(INF, P3());
		else d = sqrt(d);
		ld t = b - d > eps ? b - d : b + d > eps? b + d : -1;
		if (t < 0)
			return std::make_pair(INF, P3());
		return std::make_pair(t, ray.get(t));
	}
	virtual std::pair<P3, P3> aabb() {
		return std::make_pair(o-r, o+r);
	}
	virtual P3 norm(P3 p) {
		ld d = std::abs((p - o).len() - r);
		assert(d < eps);
		return (p - o).norm();
	}
};

class PlaneObject: public Object {
// store ax+by+cz=1
public:
	P3 n, n0;
	PlaneObject(P3 n_, Texture t):
		n(n_), n0(n_.norm()), Object(t) {}
	PlaneObject(P3 n_, Refl_t refl, ld brdf = 1.5, P3 color = P3(), P3 emission = P3(), std::string tname = ""):
		n(n_), n0(n_.norm()), Object(refl, color, emission, brdf, tname) {}
	virtual std::pair<ld, P3> intersect(Ray ray) {
		ld t = (1 - ray.o.dot(n)) / ray.d.dot(n);
		if (t < eps)
			return std::make_pair(INF, P3());
		return std::make_pair(t, ray.get(t));
	}
	virtual std::pair<P3, P3> aabb() {
		P3 p0 = P3(min_p[0], min_p[1], min_p[2]);
		P3 p1 = P3(max_p[0], max_p[1], max_p[2]);
		if (std::abs(n.x) <= eps && std::abs(n.y) <= eps) { // horizontal plane
			p0.z = 1. / n.z - eps;
			p1.z = 1. / n.z + eps;
			return std::make_pair(p0, p1);
		}
		if (std::abs(n.y) <= eps && std::abs(n.z) <= eps) { // verticle plane
			p0.x = 1. / n.x - eps;
			p1.x = 1. / n.x + eps;
			return std::make_pair(p0, p1);
		}
		if (std::abs(n.x) <= eps && std::abs(n.z) <= eps) { // verticle plane
			p0.y = 1. / n.y - eps;
			p1.y = 1. / n.y + eps;
			return std::make_pair(p0, p1);
		}
		return std::make_pair(p0, p1);
	}
	virtual P3 norm(P3) {
		return n0;
	}
};

#endif // __OBJ_H__
