#ifndef __OBJ_H__
#define __OBJ_H__ 

#include "utils.hpp"
#include "ray.hpp"
#include "bezier.hpp"

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
	virtual P3 change_for_bezier(P3) {puts("virtual error in bezier!");}
};

class BezierObject: public Object {
// the curve will rotate line (x=pos.x and z=pos.z) as pivot
public:
	BezierCurve2D curve;
	P3 pos; // the buttom center point
	BezierObject(P3 pos_, BezierCurve2D c_, Texture t):
		pos(pos_), curve(c_), Object(t) {}
	BezierObject(P3 pos_, BezierCurve2D c_, Refl_t refl, ld brdf = 1.5, P3 color = P3(), P3 emission = P3(), std::string tname = ""):
		pos(pos_), curve(c_), Object(refl, color, emission, brdf, tname) {}
	ld solve_t(ld yc) { // solve y(t)=yc
		assert(0 <= yc && yc <= curve.height);
		ld t = .5, ft, dft;
		for (int i = 10; i--; )
		{
			if (t < 0) t = 0;
			else if (t > 1) t = 1;
			ft = curve.getpos(t).y - yc, dft = curve.getdir(t).y;
			if (std::abs(ft) < eps)
				return t;
			t -= ft / dft;
		}
		return -1;
	}
	virtual P3 change_for_bezier(P3 inter_p) {
		ld t = solve_t(inter_p.y - pos.y);
		ld u = atan2(inter_p.z - pos.z, inter_p.x - pos.x); // between -pi ~ pi
		if (u < 0)
			u += 2 * PI;
		return P3(u, t);
	}
	ld get_sphere_intersect(Ray ray, P3 o, ld r) {
		P3 ro = o - ray.o;
		ld b = ray.d.dot(ro);
		ld d = sqr(b) - ro.dot(ro) + sqr(r);
		if (d < 0) return -1;
		else d = sqrt(d);
		ld t = b - d > eps ? b - d : b + d > eps? b + d : -1;
		if (t < 0)
			return -1;
		return t;
	}
	virtual std::pair<ld, P3> intersect(Ray ray) {
		ld final_dis = INF;
		// check for |dy|<eps
		if (std::abs(ray.d.y) < 1e-3)
		{
			ld dis_to_axis = (P3(pos.x, ray.o.y, pos.z) - ray.o).len();
			ld hit = ray.get(dis_to_axis).y;
			if (hit < pos.y + eps || hit > pos.y + curve.height - eps)
				return std::make_pair(INF, P3());
			// solve function pos.y+y(t)=ray.o.y to get x(t)
			ld t = solve_t(hit - pos.y);
			if (t < 0 || t > 1)
				return std::make_pair(INF, P3());
			P3 loc = curve.getpos(t);
			ld ft = pos.y + loc.y - hit;
			if (std::abs(ft) > eps)
				return std::make_pair(INF, P3());
			// assume sphere (pos.x, pos.y + loc.y, pos.z) - loc.x
			final_dis = get_sphere_intersect(ray, P3(pos.x, pos.y + loc.y, pos.z), loc.x);
			if (final_dis < 0)
				return std::make_pair(INF, P3());
			P3 inter_p = ray.get(final_dis);
			// printf("y %f small!!!",std::abs((inter_p - P3(pos.x, inter_p.y, pos.z)).len2() - sqr(loc.x)));
			if (std::abs((inter_p - P3(pos.x, inter_p.y, pos.z)).len2() - sqr(loc.x)) > 1e-1)
				return std::make_pair(INF, P3());
			// second iteration, more accuracy
			hit = inter_p.y;
			if (hit < pos.y + eps || hit > pos.y + curve.height - eps)
				return std::make_pair(INF, P3());
			t = solve_t(hit - pos.y);
			loc = curve.getpos(t);
			ft = pos.y + loc.y - hit;
			if (std::abs(ft) > eps)
				return std::make_pair(INF, P3());
			final_dis = get_sphere_intersect(ray, P3(pos.x, pos.y + loc.y, pos.z), loc.x);
			if (final_dis < 0)
				return std::make_pair(INF, P3());
			inter_p = ray.get(final_dis);
			if (std::abs((inter_p - P3(pos.x, hit, pos.z)).len2() - sqr(loc.x)) > 1e-2)
				return std::make_pair(INF, P3());
			// printf("---y %f small!!!",std::abs((inter_p - P3(pos.x, inter_p.y, pos.z)).len2() - sqr(loc.x)));
			return std::make_pair(final_dis, inter_p);
		}
		// printf("y big\n");
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
		a += sqr(t2);
		b += 2 * t1 * t2;
		c += sqr(t1);
		// ay^2+by+c -> a'(y-b')^2+c'
		c = c - b * b / 4 / a;
		b = -b / 2 / a - pos.y;
		// printf("%lf %lf %lf\n",a,b,c);
		if (0 <= b && b <= curve.height && c > curve.max2
		 || (b < 0 || b > curve.height) && std::min(sqr(b), sqr(curve.height - b)) * a + c > curve.max2) // no intersect
			return std::make_pair(INF, P3());
		// ld pick[20] = {0, 0, 1}; int tot = 2;
		// for (ld _ = 0; _ <= 1; _ += 0.1)
		// {
		// 	ld t_pick = newton2(_, a, b, c);
		// 	if (0 <= t_pick && t_pick <= 1)
		// 	{
		// 		bool flag = 1;
		// 		for (int j = 1; j <= tot; ++j)
		// 			if (std::abs(t_pick - pick[j]) < eps)
		// 				flag = 0;
		// 		if (flag)
		// 			pick[++tot] = t_pick;
		// 	}
		// }
		// std::sort(pick + 1, pick + 1 + tot);
		// for (int j = 1; j < tot; ++j)
		// 	if (getft(pick[j], a, b, c) * getft(pick[j + 1], a, b, c) <= 0)
		// 		check(pick[j], pick[j+1], (pick[j] + pick[j + 1]) * .5, ray, a, b, c, final_dis);
		int ind = 0;
		for (ld _ = 0; _ <= 0.9; _ += 0.1, ++ind)
		{
			// y = curve.ckpt[ind] ~ curve.ckpt[ind+1]
			// calc min(a(y-b)^2+c)
			// ld lower;
			// if (curve.ckpt[ind] <= b && b <= curve.ckpt[ind + 1])
				// lower = c;
			// else
				// lower = a * std::min(sqr(curve.ckpt[ind] - b), sqr(curve.ckpt[ind + 1] - b)) + c;
			// if (lower < sqr(curve.width[ind]) + 1)
			{
				check(_, _+0.1, _+.033, ray, a, b, c, final_dis);
				check(_, _+0.1, _+.066, ray, a, b, c, final_dis);
			}
		}
		if (final_dis < INF / 2)
			return std::make_pair(final_dis, ray.get(final_dis));
		else
			return std::make_pair(INF, P3());
	}
	bool check(ld low, ld upp, ld init, Ray ray, ld a, ld b, ld c, ld&final_dis)
	{
		ld t = newton(init, a, b, c, low, upp);
		if (t <= 0 || t >= 1)
			return false;
		P3 loc = curve.getpos(t), dir = curve.getdir(t);
		ld x = loc.x, dx = dir.x;
		ld y = loc.y, dy = dir.y;
		ld ft = x - sqrt(a * sqr(y - b) + c);
		if (std::abs(ft) > eps)
			return false;
		// calc t for ray
		ld dis = (pos.y + y - ray.o.y) / ray.d.y;
		if (dis < eps)
			return false;
		P3 inter_p = ray.get(dis);
		if (std::abs((P3(pos.x, pos.y + y, pos.z) - inter_p).len2() - x * x) > eps)
			return false;
		if (dis < final_dis)
		{
			final_dis = dis;
			return true;
		}
		return false;
	}
	ld getft(ld t, ld a, ld b, ld c)
	{
		if (t < 0) t = eps;
		if (t > 1) t = 1 - eps;
		P3 loc = curve.getpos(t);
		ld x = loc.x, y = loc.y;
		return x - sqrt(a * sqr(y - b) + c);
	}
	ld newton(ld t, ld a, ld b, ld c, ld low=eps, ld upp=1-eps)
	{
		// solve sqrt(a(y(t)+pos.y-b)^2+c)=x(t)
		// f(t) = x(t) - sqrt(a(y(t)+pos.y-b)^2+c)
		// f'(t) = x'(t) - a(y(t)+pos.y-b)*y'(t) / sqrt(...)
		// if t is not in [0, 1] then assume f(t) is a linear function
		ld ft, dft, x, y, dx, dy, sq, last=-1;
		P3 loc, dir;
		for (int i = 20; i--; )
		{
			if (t < 0) t = low;
			if (t > 1) t = upp;
			last = t;
			loc = curve.getpos(t), dir = curve.getdir(t);
			x = loc.x, dx = dir.x;
			y = loc.y, dy = dir.y;
			// printf("%lf %lf %lf\n",t,x,y);
			sq = sqrt(a * sqr(y - b) + c);
			ft = x - sq;
			dft = dx - a * (y - b) * dy / sq;
			if (std::abs(ft) < eps)
				return t;
			t -= ft / dft;
		}
		return -1;
	}
	ld newton2(ld t, ld a, ld b, ld c)
	{
		ld dft, ddft, y, dx, dy, ddx, ddy, sq;
		P3 loc, dir, dir2;
		for (int i = 5; i--; )
		{
			if (t < 0) t = eps;
			if (t > 1) t = 1 - eps;
			loc = curve.getpos(t), dir = curve.getdir(t), dir2 = curve.getdir2(t);
			y = loc.y, dx = dir.x, dy = dir.y;
			ddx = dir2.x, ddy = dir2.y;
			sq = sqrt(a * sqr(y - b) + c);
			dft = dx - a * (y - b) * dy / sq;
			ddft = ddx - a * ((y - b) * ddy + sqr(dy)) / sq + sqr(a * (y - b) * dy) / sq / sq / sq;
			if (std::abs(dft) < eps)
				return t;
			t -= dft / ddft;
		}
		return -1;
	}
	virtual std::pair<P3, P3> aabb() {
		return std::make_pair(P3(pos.x - curve.max, pos.y, pos.z - curve.max), P3(pos.x + curve.max, pos.y + curve.height, pos.z + curve.max));
	}
	virtual P3 norm(P3 p) {
		P3 tmp = change_for_bezier(p);
		P3 dir = curve.getdir(tmp.y);
		P3 d_surface = P3(cos(tmp.x), dir.y / dir.x, sin(tmp.x));
		P3 d_circ = P3(-sin(tmp.x), 0, cos(tmp.x));
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
		ld d = sqr(b) - ro.dot(ro) + sqr(r);
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
