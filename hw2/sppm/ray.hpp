#ifndef __RAY_H__
#define __RAY_H__ 

#include "utils.hpp"
#include "vec3.hpp"

class Ray
{
public:
	P3 o, d;
	Ray(P3 o_, P3 d_): o(o_), d(d_) {}
	P3 get(ld t) { return o + d * t; }
	void print() { puts("Ray: "); o.print(); d.print(); }
};

#endif // __RAY_H__