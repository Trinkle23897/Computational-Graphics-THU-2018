#include "render.hpp"

int main(int argc, char *argv[])
{
	// Ray ray(P3(427,1000,447),P3(-1,-2,-1.5).norm());
	// find_intersect_simple(ray);
	int w=atoi(argv[1]), h=atoi(argv[2]), samp=atoi(argv[4])/4;
	Ray cam(P3(150,28,260), P3(-0.45,0.001,-1).norm());
	P3 cx=P3(w*.33/h), cy=(cx&P3(cam.d.x, 0,cam.d.z)).norm()*.33, r, *c=new P3[w*h];
	cx *= 1.05;
	ld aperture = .6;
#pragma omp parallel for schedule(dynamic, 1) private(r)
	for(int y=0;y<h;++y){
		fprintf(stderr,"\r%5.2f%%",100.*y/h);
		for(int x=0;x<w;++x){
			for(int sy=0;sy<2;++sy)
				for(int sx=0;sx<2;++sx)
				{
					unsigned short X[3]={y+sx,y*x+sy,y*x*y+sx*sy+time(0)};
					r.x=r.y=r.z=0;
					for(int s=0;s<samp;++s){
						ld r1=2*erand48(X), dx=r1<1 ? sqrt(r1): 2-sqrt(2-r1);
						ld r2=2*erand48(X), dy=r2<1 ? sqrt(r2): 2-sqrt(2-r2);
						P3 d=cx*((sx+dx/2+x)/w-.5)+cy*((sy+dy/2+y)/h-.5)+cam.d;
						P3 pp = cam.o+d*150, loc = cam.o + (P3(erand48(X)*1.05, erand48(X)) - .5) * 2 * aperture;
						r+=pt_render(Ray(pp	, (pp - loc).norm()),0,X);
					}
					c[y*w+x]+=(r/samp).clip()/4;
				}
		}
	}
	FILE*f=fopen(argv[3],"w");
	fprintf(f,"P6\n%d %d\n%d\n", w,h,255);
	for(int y=h-1;y>=0;--y)
		for(int x=w-1;x>=0;--x)
			fprintf(f,"%c%c%c",gamma_trans(c[y*w+x].x),gamma_trans(c[y*w+x].y),gamma_trans(c[y*w+x].z));
	fclose(f);
	char sout[100];
	sprintf(sout,"%s.txt",argv[3]);
	FILE*fout = fopen(sout, "w");
	for (int y = h - 1; y >= 0; --y)
		for (int x = w - 1; x >= 0; --x)
			fprintf(fout, "%.8lf %.8lf %.8lf\n", c[y*w+x].x, c[y*w+x].y, c[y*w+x].z);
	return!puts("");
}
