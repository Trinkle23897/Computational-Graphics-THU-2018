#include "render.hpp"

int pt(int argc, char *argv[])
{
	// Ray ray(P3(427,1000,447),P3(-1,-2,-1.5).norm());
	// find_intersect_simple(ray);
	int w = atoi(argv[1]), h = atoi(argv[2]), samp = atoi(argv[4]);
	Ray cam(P3(150, 28, 260), P3(-0.45, 0.001, -1).norm());
	P3 cx = P3(w * .33 / h), cy=(cx & P3(cam.d.x, 0, cam.d.z)).norm() * .33, r, *c = new P3[w * h];
	cx *= 1.05;
	ld aperture = .0;
#pragma omp parallel for schedule(dynamic, 1) private(r)
	for (int y = 0; y < h; ++y) {
		fprintf(stderr, "\r%5.2f%%", 100. * y / h);
		for (int x = 0; x < w; ++x) {
			for (int sy = 0; sy < 2; ++sy)
				for (int sx = 0; sx < 2; ++sx)
				{
					unsigned short X[3] = {y + sx, y * x + sy, y * x * y + sx * sy + time(0)};
					r.x = r.y = r.z = 0;
					for (int s = 0; s < samp; ++s) {
						ld r1 = 2 * erand48(X), dx = r1 < 1 ? sqrt(r1): 2-sqrt(2-r1);
						ld r2 = 2 * erand48(X), dy = r2 < 1 ? sqrt(r2): 2-sqrt(2-r2);
						P3 d = cx * ((sx + dx / 2 + x) / w - .5) + cy * ((sy + dy / 2 + y) / h - .5) + cam.d;
						P3 pp = cam.o + d * 150, loc = cam.o + (P3(erand48(X) * 1.05, erand48(X)) - .5) * 2 * aperture;
						r += pt_render(Ray(pp, (pp - loc).norm()), 0, X);
					}
					c[y * w + x] += (r / samp).clip()/4;
				}
		}
	}
	FILE* f = fopen(argv[3],"w");
	fprintf(f, "P6\n%d %d\n%d\n", w, h, 255);
	for (int y = h - 1; y >= 0; --y)
		for (int x = w - 1; x >= 0; --x)
			fprintf(f, "%c%c%c", gamma_trans(c[y*w+x].x), gamma_trans(c[y*w+x].y), gamma_trans(c[y*w+x].z));
	fclose(f);
	char sout[100];
	sprintf(sout,"%s.txt",argv[3]);
	FILE*fout = fopen(sout, "w");
	for (int y = h - 1; y >= 0; --y)
		for (int x = w - 1; x >= 0; --x)
			fprintf(fout, "%.8lf %.8lf %.8lf\n", c[y*w+x].x, c[y*w+x].y, c[y*w+x].z);
	return!puts("");
}

int sppm(int argc, char* argv[])
{
	int w = atoi(argv[1]), h = atoi(argv[2]), iter = atoi(argv[4]), samp = atoi(argv[5]);
	Ray cam(P3(150, 28, 260), P3(-0.45, 0.001, -1).norm());
	int nth = omp_get_num_procs(), per = 5; --scene_num;
	P3 cx = P3(w * .33 / h), cy=(cx & P3(cam.d.x, 0, cam.d.z)).norm() * .33;
	IMGbuf **c = new IMGbuf*[nth];
	for (int i = 0; i < nth; ++i) c[i] = new IMGbuf[h * w];
	cx *= 1.05;
	ld aperture = .0;
	for (int _ = iter; _--; ) {
		// build kdtree
		std::vector<SPPMnode> ball[nth];
#pragma omp parallel for schedule(dynamic, 1)
		for (int y = 0; y < h; ++y) {
			int num = omp_get_thread_num();
			fprintf(stderr, "\rbuild kdtree %5.2f%% ... ", 100. * y / h);
			for (int x = 0; x < w; ++x) {
				unsigned short X[3] = {y, y * x, y * x * y + time(0)};
				ld r1 = 2 * erand48(X), dx = r1 < 1 ? sqrt(r1): 2-sqrt(2-r1);
				ld r2 = 2 * erand48(X), dy = r2 < 1 ? sqrt(r2): 2-sqrt(2-r2);
				P3 d = cx * ((dx / 2 + x) / w - .5) + cy * ((dy / 2 + y) / h - .5) + cam.d;
				P3 pp = cam.o + d * 150, loc = cam.o + (P3(erand48(X) * 1.05, erand48(X)) - .5) * 2 * aperture;
				std::vector<SPPMnode> tmp = sppm_backtrace(Ray(pp, (pp - loc).norm()), 0, y * w + x, X);
				for (int i = 0; i < tmp.size(); ++i)
					if (tmp[i].index >= 0)
						ball[num].push_back(tmp[i]);
			}
		}
		fprintf(stderr, "done!\n");
		std::vector<SPPMnode> totball;
		for(int i = 0; i < nth; ++i) {
			totball.insert(totball.end(), ball[i].begin(), ball[i].end());
			printf("%d: %d\n", i, ball[i].size());
		}
		KDTree tree(totball);
#pragma omp parallel for schedule(dynamic, 1)
		for (int t = 0; t < samp / per + 1; ++t) {
			fprintf(stderr, "\rsppm tracing %5.2f%%", 100. * per * t / samp);
			unsigned short X[3] = {t, t * t, (t & (t * t)) + time(0)};
			int num = omp_get_thread_num();
			for (int y = 2; y <= 81; ++y)
				for (int x = 1; x < 100; ++x)
					for (int z = 0; z < 100; ++z)
					tree.query(SPPMnode(P3(x, y, z), P3(z/100., z/100., z/100.), P3(0,0,0)), 1, c[num]);
			// for (int __ = per; __--; ) {
			// 	// gen random light
			// 	P3 o(50 - 18 + erand48(X) * 36, 81.6 - eps, 81.6 - 18 + erand48(X) * 36);
			// 	ld r1 = 2 * PI * erand48(X), r2 = erand48(X), r2s = sqrt(r2);
			// 	P3 w = P3(0, -1, 0), u=(P3(1).cross(w)).norm(), v = w.cross(u);
			// 	P3 d = (u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2)).norm();
			// 	Ray light = Ray(o, d);
			// 	P3 col = P3(8,8,8);
			// 	ld rad = 10;
			// 	sppm_forward(light, 0, col, X, rad * rad, c[num], &tree);
			// }
		}
	}
	P3* final = new P3[h * w];
	FILE* f = fopen(argv[3], "w");
	fprintf(f, "P6\n%d %d\n%d\n", w, h, 255);
	for (int y = h - 1; y >= 0; --y)
		for (int x = w - 1; x >= 0; --x) {
			IMGbuf tmp;
			for (int i = 0; i < nth; ++i) {
				tmp.n += c[i][y * w + x].n;
				tmp.f += c[i][y * w + x].f;
			}
			final[y * w + x] = tmp.f / tmp.n;
			fprintf(f, "%c%c%c", gamma_trans(final[y*w+x].x), gamma_trans(final[y*w+x].y), gamma_trans(final[y*w+x].z));
		}
	fclose(f);
	char sout[100];
	sprintf(sout, "%s.txt", argv[3]);
	FILE* fout = fopen(sout, "w");
	for (int y = h - 1; y >= 0; --y)
		for (int x = w - 1; x >= 0; --x)
			fprintf(fout, "%.8lf %.8lf %.8lf\n", final[y*w+x].x, final[y*w+x].y, final[y*w+x].z);
	return !puts("");
}

int main(int argc, char*argv[])
{
	return sppm(argc, argv);
}