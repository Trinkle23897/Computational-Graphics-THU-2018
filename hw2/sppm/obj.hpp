#ifndef __OBJ_H__
#define __OBJ_H__ 

#include "utils.hpp"
#include "ray.hpp"

// enum Refl_t { DIFF, SPEC, REFR };

class Texture {
public:
	std::string filename;
	unsigned char *buf;
	int w, h, c;
	Texture(): buf(NULL), w(0), h(0), c(0) {}
	Texture(std::string _): filename(_) {
		if(_ == "")
			return;
		buf = stbi_load(filename.c_str(), &w, &h, &c, 0);
	}
	std::pair<Refl_t, P3> getcol(ld x, ld y) {
		// assume x, y in [0, 1]
		if (buf == NULL || x < 0 || y < 0 || x > 1 || y > 1)
			return std::make_pair(SPEC, P3());
		int pw = int(x * w), ph = int(y * h);
		int idx = ph * w * c + pw * c;
		int x = buf[idx + 0], y = buf[idx + 1], z = buf[idx + 2];
		if (x == 233 && y == 233 && z == 233) {
			return std::make_pair(SPEC, P3());
		}
		return std::pair<DIFF, P3(x, y, z) / 255.>;
	}
};

class Object {
public:
	P3 color, emision;
	Texture* texture;
	Object(P3 c, P3 e, std::string tname): color(c), emision(e), texture(NULL) {
		if (tname != "")
			texture = new Texture(tname);
	}
	virtual std::pair<ld, P3> intersect(Ray) = 0;
	virtual std::pair<P3, P3> aabb() = 0;
};

class CubeObject: public Object {
//store (x0, y0, z0) - (x1, y1, z1)
public:
	P3 m0, m1;
	CubeObject(P3 m0_, P3 m1_, std::string tname = "", P3 color = P3(), P3 emission = P3()): 
		m0(min(m0_, m1_)), m1(max(m0_, m1_)), Object(color, emission, tname) {}
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
	SphereObject(P3 o_, ld r_, std::string tname = "", P3 color = P3(), P3 emission = P3()):
		o(o_), r(r_), Object(color, emission, tname) {}
	std::pair<ld, P3> intersect(Ray ray) {
		P3 ro = o - ray.o;
		ld b = ray.d.dot(ro);
		ld d = b * b - ro.dot(ro) + r * r;
		if (d < 0) return std::make_pair(INF, P3());
		else d = sqrt(d);
		ld t = b - d > eps ? b-d : b+d>eps? b+d : -1;
		if (t < -eps)
			return std::make_pair(INF, P3());
		return std::make_pair(t, ray.o + t * ray.d);
	}
	std::pair<P3, P3> aabb() {
		return std::make_pair<o-r, o+r>;
	}
}

class PlaneObject: public Object {
// store ax+by+cz=1
public:
	P3 n;
	PlaneObject(P3 n_, std::string tname = "", P3 color = P3(), P3 emission = P3()):
		n(n_), Object(color, emission, tname) {}
	std::pair<ld, P3> intersect(Ray ray) {
		ld t = (1 - ray.o.dot(n)) / ray.d.dot(n);
		if (t < eps)
			return std::make_pair(INF, P3());
		return std::make_pair(t, ray.o + t * ray.d);
	}
	std::pair<P3, P3> aabb() {
		return std::make_pair<P3() - INF, P3() + INF>;
	}
};



#endif // __OBJ_H__