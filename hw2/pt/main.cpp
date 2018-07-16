#include "utils.hpp"

#define eps 1e-6
#define mcol 255
struct Ray{
	P3 o, d;
	Ray(P3 o_, P3 d_): o(o_), d(d_) {}
};

enum Refl_t { DIFF, SPEC, REFR };

struct Sphere{
	ld rad, ns;
	P3 o, e, c;
	Refl_t refl;
	Sphere(ld rad_, P3 o_, P3 e_, P3 c_, Refl_t refl_, ld ns_=1.5): 
		rad(rad_), ns(ns_), o(o_), e(e_), c(c_), refl(refl_) {}
	ld intersect(const Ray&r) const {
		P3 ro = o-r.o;
		ld b = r.d.dot(ro);
		ld d = b*b-ro.dot(ro)+rad*rad;
		if (d<0) return 0;
		else d=sqrt(d);
		return b-d>eps ? b-d : b+d>eps? b+d : 0;
	}
};
Sphere scene[] = {//Scene: radius, position, emission, color, material 
   Sphere(1e5, P3( 1e5+1,40.8,81.6), P3(),P3(.1,.25,.25),DIFF),//Left 
   Sphere(1e5, P3(-1e5+99,40.8,81.6),P3(),P3(.25,.75,.25),DIFF),//Rght 
   Sphere(1e5, P3(50,40.8, 1e5),     P3(),P3(.75,.75,.75),DIFF),//Back 
   Sphere(1e5, P3(50,40.8,-1e5+170), P3(),P3(.25,.25,.25),DIFF),//Frnt 
   Sphere(1e5, P3(50, 1e5, 81.6),    P3(),P3(.75,.75,.75),DIFF),//Botm 
   Sphere(1e5, P3(50,-1e5+81.6,81.6),P3(),P3(.75,.75,.75),DIFF),//Top 
   Sphere(16.5,P3(27,16.5,47),       P3(),P3(1,1,1)*.999, SPEC),//Mirr 
   Sphere(16.5,P3(73,16.5,78),       P3(),P3(1,1,1)*.999, REFR),//Glas 
   Sphere(16.5,P3(20,60,100),       P3(),P3(.75,.25,.25), DIFF),//Glas 
   Sphere(600, P3(50,681.6-.27,81.6),P3(12,12,12),  P3(), DIFF) //Lite 
}; 

int n=sizeof(scene)/sizeof(Sphere);

int output(ld x) {return int(.5+mcol*pow(x<0?0:x>1?1:x,1/2.2));}

bool intersect(const Ray&r, ld&t, int&id){
	ld inf=t=1e30, tmp;
	for (int i=0;i<n;++i)
		if((tmp=scene[i].intersect(r))&&tmp<t)
			id=i, t=tmp;
	return t<inf;
}

P3 radiance(const Ray&r, int dep,unsigned short *X){
	ld t;int id, into=0;
	if(!intersect(r,t,id))return P3();
	const Sphere&obj=scene[id];
	P3 x=r.o+r.d*t,n=(x-obj.o).norm(),f=obj.c,nl=n.dot(r.d)<0?into=1,n:-n;
	ld p=f.max();
	if(++dep>5)
		if(erand48(X)<p) f/=p;
		else return obj.e;
	if(obj.refl==DIFF){
		ld r1=2*PI*erand48(X), r2=erand48(X), r2s=sqrt(r2);
		P3 w=nl, u=((fabs(w.x)>.1?P3(0,1):P3(1))&w).norm(), v=w&u;
		P3 d = (u*cos(r1)*r2s + v*sin(r1)*r2s + w*sqrt(1-r2)).norm();
		return obj.e + f.mult(radiance(Ray(x,d),dep,X));
	}
	else{
		Ray reflray = Ray(x,r.d.reflect(n));
		if (obj.refl == SPEC){
		    return obj.e + f.mult(radiance(reflray,dep,X)); 
		}
		else{
			P3 d = r.d.refract(n, into?1:obj.ns, into?obj.ns:1); //...
			if (d.len2()<eps) // Total internal reflection 
				return obj.e + f.mult(radiance(reflray, dep,X));
			ld a=obj.ns-1, b=obj.ns+1, R0=a*a/(b*b), c = 1-(into?-r.d.dot(nl):d.dot(n)); 
			ld Re=R0+(1-R0)*c*c*c*c*c,Tr=1-Re,P=.25+.5*Re,RP=Re/P,TP=Tr/(1-P); 
			return obj.e + f.mult(dep>2 ? (erand48(X)<P ?   // Russian roulette 
				radiance(reflray,dep,X)*RP:radiance(Ray(x,d),dep,X)*TP) : 
				radiance(reflray,dep,X)*Re+radiance(Ray(x,d),dep,X)*Tr); 
		}
	}
}

int main(int argc, char*argv[])
{
	int w=atoi(argv[1]), h=atoi(argv[2]), samp=atoi(argv[4])/4;
	Ray cam(P3(70,32,280), P3(-0.15,0.05,-1).norm());
	P3 cx=P3(w*.5/h), cy=(cx&cam.d).norm()*.5, r, *c=new P3[w*h];
#pragma omp parallel for schedule(dynamic, 1) private(r)
	for(int y=0;y<h;++y){
		fprintf(stderr,"\rUsing %d spp  %5.2f%%",samp*4,100.*y/h);
		for(int x=0;x<w;++x){
			for(int sy=0;sy<2;++sy)
				for(int sx=0;sx<2;++sx)
				{
					unsigned short X[3]={y+sx,y*x+sy,y*x*y+sx*sy};
					r.x=r.y=r.z=0;
					for(int s=0;s<samp;++s){
						ld r1=2*erand48(X), dx=r1<1 ? sqrt(r1): 2-sqrt(2-r1);
						ld r2=2*erand48(X), dy=r2<1 ? sqrt(r2): 2-sqrt(2-r2);
						P3 d=cx*((sx+dx/2+x)/w-.5)+cy*((sy+dy/2+y)/h-.5)+cam.d; 
						r+=radiance(Ray(cam.o+d*120,d.norm()),0,X);
					}
					c[y*w+x]+=(r/samp).clip()/4;
				}
		}
	}
	FILE*f=fopen(argv[3],"w");
	fprintf(f,"P6\n%d %d\n%d\n", w,h,mcol);
	for(int y=h-1;y>=0;--y)
		for(int x=w-1;x>=0;--x)
			fprintf(f,"%c%c%c",output(c[y*w+x].x),output(c[y*w+x].y),output(c[y*w+x].z));
	return!puts("");
}
