#ifndef __OBJ_H__
#define __OBJ_H__ 

#include "utils.hpp"
#include "ray.hpp"
#include "bezier.hpp"

// enum Refl_t { DIFF, SPEC, REFR };

class Object {
public:
	Texture texture;
	ld cache_u, cache_t;
	Object(Texture t): texture(t) {}
	Object(Refl_t refl, P3 color, P3 emission, ld brdf, std::string tname):
		texture(tname, brdf, color, emission, refl) {}
	virtual std::pair<ld, P3> intersect(Ray) {puts("virtual error in intersect!");}
		// If no intersect, then return (INF, (0,0,0))
	virtual std::pair<P3, P3> aabb() {puts("virtual error in aabb!");}
	virtual P3 norm(P3) {puts("virtual error in norm!");}
		// return norm vec out of obj
};

class BezierObject: public Object {
// the curve will rotate line (x=pos.x and z=pos.z) as pivot
public:
	BezierCurve2D curve;
	P3 pos; // the buttom center point
	ld bezier_cache_t, bezier_cache_u; // the last intersect result given by bezier
	BezierObject(P3 pos_, BezierCurve2D c_, Texture t):
		pos(pos_), curve(c_), Object(t) {}
	BezierObject(P3 pos_, BezierCurve2D c_, Refl_t refl, ld brdf = 1.5, P3 color = P3(), P3 emission = P3(), std::string tname = ""):
		pos(pos_), curve(c_), Object(refl, color, emission, brdf, tname) {}
	virtual std::pair<ld, P3> intersect(Ray ray) {
		ld final_dis = INF, dis = 0;
		// check for |dy|<eps
		if (std::abs(ray.d.y) < -1)
		{
			if (ray.o.y < pos.y || ray.o.y > pos.y + curve.height)
				return std::make_pair(INF, P3());
			// solve function pos.y+y(t)=ray.o.y to get x(t)
			for (ld _ = 0; _ <= 1; _ += 0.5)
			{
				ld t = _, ft, dft;
				for (int i = 10; i--; )
				{
					if (t <= 0)
						ft = pos.y + curve.p0.y + t * curve.dp0.y - ray.o.y, dft = curve.dp0.y;
					else if (t >= 1)
						ft = pos.y + curve.p1.y + (t - 1) * curve.dp1.y - ray.o.y, dft = curve.dp1.y;
					else
						ft = pos.y + curve.getpos(t).y - ray.o.y, dft = curve.getdir(t).y;
					t -= ft / dft;
					if (std::abs(ft) < eps)
						break;
				}
				if (t < 0 || t > 1)
					continue;
				P3 loc = curve.getpos(t);
				ft = pos.y + loc.y - ray.o.y;
				if (std::abs(ft) > eps)
					continue;
				ld dis_to_axis = (P3(pos.x, ray.o.y, pos.z) - ray.o).len();
				// axis - x
				if (dis_to_axis - loc.x > eps)
					final_dis = dis_to_axis - loc.x;
				else if (dis_to_axis + loc.x > eps)
					final_dis = dis_to_axis + loc.x;
				else
					continue;
				P3 inter_p = ray.get(final_dis);
				bezier_cache_t = t;
				P3 angle_point = inter_p - pos - P3(0, ray.o.y);
				bezier_cache_u = atan2(angle_point.z, angle_point.x); // between -pi ~ pi
				if (bezier_cache_u < 0)
					bezier_cache_u += 2 * PI;
				cache_t = bezier_cache_t;
				cache_u = bezier_cache_u / 2 / PI;
				return std::make_pair(final_dis, inter_p);
			}
			return std::make_pair(INF, P3());
		}
		// check for top circle: the plane is y=pos.y + curve.height
		// TODO
		// check for buttom circle: the plane is y=pos.y
		// TODO
		// normal case
		// calc ay^2+by+c
		ld a = 0, b = 0, c = 0, t1, t2;
		// (xo-x'+xd/yd*(y-yo))^2 -> (t1+t2*y)^2
		t1 = ray.o.x - pos.x - ray.d.x / ray.d.y * ray.o.y;
		t2 = ray.d.x / ray.d.y;
		a += t2 * t2;
		b += 2 * t1 * t2;
		c += t1 * t1;
		// (zo-z'+zd/yd*(y-yo))^2 -> (t1+t2*y)^2
		t1 = ray.o.z - pos.z - ray.d.z / ray.d.y * ray.o.y;
		t2 = ray.d.z / ray.d.y;
		a += t2 * t2;
		b += 2 * t1 * t2;
		c += t1 * t1;
		// ay^2+by+c -> a'(y-b')^2+c'
		c = c - b * b / 4 / a;
		b = -b / 2 / a;
		// printf("%lf %lf %lf\n",a,b,c);
		// solve sqrt(a(y(t)+pos.y-b)^2+c)=x(t)
		// f(t) = x(t) - sqrt(a(y(t)+pos.y-b)^2+c)
		// f'(t) = x'(t) - a(y(t)+pos.y-b)*y'(t) / sqrt(...)
		if (c > curve.max2) // no intersect
			return std::make_pair(INF, P3());
		// if t is not in [0, 1] then assume f(t) is a linear function
		P3 pt, dpt;
		for (ld _ = 0; _ <= 1; _ += 0.2)
		{
			ld t = _, ft, dft, x, y, dx, dy;
			P3 loc, dir;
			for (int i = 10; i--; )
			{
				if (t <= 0)
				{
					x = curve.p0.x + t * curve.dp0.x, dx = curve.dp0.x;
					y = pos.y + curve.p0.y + t * curve.dp0.y, dy = curve.dp0.y;
				}
				else if (t >= 1)
				{
					x = curve.p1.x + (t - 1) * curve.dp1.x, dx = curve.dp1.x;
					y = pos.y + curve.p1.y + (t - 1) * curve.dp1.y, dy = curve.dp1.y;
				}
				else
				{
					loc = curve.getpos(t), dir = curve.getdir(t);
					x = loc.x, dx = dir.x;
					y = pos.y + loc.y, dy = dir.y;
				}
				// printf("%lf %lf %lf\n",t,x,y);
				ld sq = sqrt(a * (y - b) * (y - b) + c);
				ft = x - sq;
				dft = dx - a * (y - b) * dy / sq;
				t -= ft / dft;
				if (std::abs(ft) < eps)
					break;
			}
			if (t <= 0 || t >= 1)
				continue;
			loc = curve.getpos(t), dir = curve.getdir(t);
			x = loc.x, dx = dir.x;
			y = pos.y + loc.y, dy = dir.y;
			ft = x - sqrt(a * (y - b) * (y - b) + c);
			dft = dx - a * (y - b) * dy / (x - ft);
			if (std::abs(ft) > eps)
				continue;
			// calc t for ray
			dis = (y - ray.o.y) / ray.d.y;
			if (dis < eps)
				continue;
			P3 inter_p = ray.get(dis);
			if (std::abs((P3(pos.x, y, pos.z) - inter_p).len2() - x * x) > eps)
				continue;
			if (dis < final_dis)
			{
				final_dis = dis;
				bezier_cache_t = t;
				P3 angle_point = inter_p - P3(pos.x, y, pos.z);
				bezier_cache_u = atan2(angle_point.z, angle_point.x); // between -pi ~ pi
				if (bezier_cache_u < 0)
					bezier_cache_u += 2 * PI;
				cache_u = bezier_cache_u / 2 / PI;
				cache_t = bezier_cache_t;
				// printf("%lf %lf %lf %lf\n",t,inter_p.x, inter_p.y, inter_p.z);
			}
		}
		if (final_dis < INF / 2)
			return std::make_pair(final_dis, ray.get(final_dis));
		else
			return std::make_pair(INF, P3());
	}
	virtual std::pair<P3, P3> aabb() {
		return std::make_pair(P3(pos.x - curve.max, pos.y, pos.z - curve.max), P3(pos.x + curve.max, pos.y + curve.height, pos.z + curve.max));
	}
	virtual P3 norm(P3 p) {
		P3 dir = curve.getdir(bezier_cache_t);
		P3 d_surface = P3(cos(bezier_cache_u), dir.y / dir.x, sin(bezier_cache_u));
		P3 d_circ = P3(-sin(bezier_cache_u), 0, cos(bezier_cache_u));
		return d_circ.cross(d_surface).norm();
	}
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
// store ax+by+cz=1 n=(a,b,c)
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
