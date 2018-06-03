#ifndef __VEC3_H__
#define __VEC3_H__ 

#include "utils.hpp"

struct P3{
	ld x, y, z;
	P3(ld x_=0, ld y_=0, ld z_=0): x(x_), y(y_), z(z_) {}
	P3 operator-() const {return P3(-x, -y, -z);}
	P3 operator+(const P3&a) const {return P3(x+a.x, y+a.y, z+a.z);}
	P3 operator-(const P3&a) const {return P3(x-a.x, y-a.y, z-a.z);}
	P3 operator+(ld p) const {return P3(x+p, y+p, z+p);}
	P3 operator-(ld p) const {return P3(x-p, y-p, z-p);}
	P3 operator*(ld p) const {return P3(x*p, y*p, z*p);}
	P3 operator/(ld p) const {return P3(x/p, y/p, z/p);}
	bool operator==(const P3&a) const {return x==a.x && y==a.y && z==a.z;}
	bool operator!=(const P3&a) const {return x!=a.x || y!=a.y || z!=a.z;}
	P3&operator+=(const P3&a) {return *this = *this + a;}
	P3&operator-=(const P3&a) {return *this = *this - a;}
	P3&operator*=(ld p) {return *this = *this * p;}
	P3&operator/=(ld p) {return *this = *this / p;}
	ld operator|(const P3&a) const {return x*a.x + y*a.y + z*a.z;}
	ld dot(const P3&a) const {return x*a.x + y*a.y + z*a.z;}
	ld max() const {return x>y&&x>z?x:y>z?y:z;}
	ld len() const {return sqrt(x*x + y*y + z*z);}
	ld len2() const {return x*x + y*y + z*z;}
	P3 mult(const P3&a) const {return P3(x*a.x, y*a.y, z*a.z);}
	P3 operator&(const P3&a) const {return P3(y*a.z-z*a.y, z*a.x-x*a.z, x*a.y-y*a.x);}
	P3 cross(const P3&a) const {return P3(y*a.z-z*a.y, z*a.x-x*a.z, x*a.y-y*a.x);}
	P3 norm() const {return (*this)/len();}
	P3 clip(ld r0=0, ld r1=1) const {return P3(x>r1?r1:x<r0?r0:x, y>r1?r1:y<r0?r0:y, z>r1?r1:z<r0?r0:z);}
	P3 reflect(const P3&n) const {return (*this)-n*2.*n.dot(*this);}
	P3 refract(const P3&n, ld ni, ld nr) const { // smallPT1.ppt Page#72
		ld cosi = this->norm().dot(n);
		ld nir = ni / nr;
		ld cosr2 = 1. - nir*nir*(1-cosi*cosi);
		if (cosr2 <= 0)
			return P3();
		ld cosr = sqrt(cosr2);
		if (cosi > 0) // out
			cosr = -cosr;
		return ((*this)*nir - n*(nir*cosi+cosr)).norm();
	}
	void print() const {std::cout << x << " " << y << " " << z << std::endl;}
};

P3 min(P3 a, P3 b) {return P3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z))}
P3 max(P3 a, P3 b) {return P3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z))}

// assume P3() == NULL (0,0,0)

#endif // __VEC3_H__