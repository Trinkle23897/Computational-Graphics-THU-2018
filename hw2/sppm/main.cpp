#include "render.hpp"

int baseline(int argc, char *argv[])
{
	// Ray ray(P3(427,1000,447),P3(-1,-2,-1.5).norm());
	// find_intersect_simple(ray);
	int w = atoi(argv[1]), h = atoi(argv[2]), samp = atoi(argv[4]);
	Ray cam(scene_camera.origin, scene_camera.direction.norm());
	P3 cx = P3(w * .33 / h), cy=(cx & P3(cam.d.x, 0, cam.d.z)).norm() * .33, r, *c = new P3[w * h];
	cx *= scene_camera.scale;
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
						P3 pp = cam.o + d * scene_camera.offset,
							loc = cam.o + (P3(erand48(X) * scene_camera.scale, erand48(X)) - .5) * 2 * aperture;
						r += basic_render(Ray(pp, (pp - loc).norm()), 0, X);
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
	int w = atoi(argv[1]), h = atoi(argv[2]), iter = atoi(argv[4]);
	ld rad = atof(argv[6]), alpha = atof(argv[7]);
	int photons = std::max(1, int(ceil(atof(argv[5]) * w * h)));
	Ray cam(scene_camera.origin, scene_camera.direction.norm());
	int nth = omp_get_max_threads();
	P3 power = scene[scene_num - 1]->texture.emission * (PI * sqr(18));
	--scene_num;
	P3 cx = P3(w * .33 / h), cy=(cx & P3(cam.d.x, 0, cam.d.z)).norm() * .33;
	std::vector<std::vector<IMGbuf>> c(nth, std::vector<IMGbuf>(h * w));
	std::vector<IMGbuf> final(h * w), now(h * w);
	std::vector<ld> radius(h * w, rad);
	cx *= scene_camera.scale;
	ld aperture = .0;
	std::vector<std::vector<SPPMnode>> ball(nth);
	KDTree tree;
	for (int _ = 1; _ <= iter; fprintf(stderr, "\riter %d done!\n", _), ++_) {
		for (int i = 0; i < nth; ++i) ball[i].clear();
		#pragma omp parallel for num_threads(nth) schedule(dynamic, 1)
		for (int y = 0; y < h; ++y) {
			int num = omp_get_thread_num();
			fprintf(stderr, "\rbuild kdtree %5.2f%% ... ", 100. * y / h);
			for (int x = 0; x < w; ++x)
			for (int sy = 0; sy < 2; ++sy)
			for (int sx = 0; sx < 2; ++sx) {
				unsigned short X[3] = {y + sy, y * x * time(0) + sx, y * x * y + time(0) + sy * 2 + sx + _};
				ld r1 = 2 * erand48(X), dx = r1 < 1 ? sqrt(r1): 2-sqrt(2-r1);
				ld r2 = 2 * erand48(X), dy = r2 < 1 ? sqrt(r2): 2-sqrt(2-r2);
				P3 d = cx * ((dx / 2 + x + sx) / w - .5) + cy * ((dy / 2 + y + sy) / h - .5) + cam.d;
				P3 pp = cam.o + d * scene_camera.offset,
					loc = cam.o + (P3(erand48(X) * scene_camera.scale, erand48(X)) - .5) * 2 * aperture;
				std::vector<SPPMnode> tmp = sppm_backtrace(Ray(pp, (pp - loc).norm()), 0, y * w + x, X);
				for (SPPMnode& node: tmp)
					if (node.index >= 0) {
						node.r = radius[node.index];
						ball[num].push_back(node);
					}
			}
		}
		std::vector<SPPMnode> totball;
		for (int i = 0; i < nth; ++i)
			totball.insert(totball.end(), ball[i].begin(), ball[i].end());
		tree.init(totball);
		fprintf(stderr, "\rbuild tree ... done! photons = %d\n", photons);
		#pragma omp parallel for num_threads(nth) schedule(dynamic, 1)
		for (int t = 0; t < nth; ++t) {
			unsigned short X[3] = {t, t * t, (t & (t * t)) + _ + time(0)};
			int num = omp_get_thread_num();
			for (int p = t; p < photons; p += nth) {
				ld rc = sqrt(erand48(X)) * 18, tht = erand48(X) * 2 * PI;
				P3 o(50 + rc * cos(tht), 81.59, 81.6 + rc * sin(tht));
				ld r1 = 2 * PI * erand48(X), r2 = erand48(X), r2s = sqrt(r2);
				P3 w = P3(0, -1, 0), u=(P3(1).cross(w)).norm(), v = w.cross(u);
				P3 d = (u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1 - r2)).norm();
				sppm_forward(Ray(o, d), 0, power, X, c[num].data(), &tree);
			}
		}
		for (IMGbuf& pixel: now) pixel.reset();
		for (int i = 0; i < nth; ++i)
			for (int j = h * w - 1; j >= 0; --j) {
				now[j] += c[i][j];
				c[i][j].reset();
			}
		for (int i = h * w - 1; i >= 0; --i)
			if (now[i].n > 0) {
				ld ratio = (final[i].n + alpha * now[i].n) / (final[i].n + now[i].n);
				radius[i] *= sqrt(ratio);
				final[i].f = (final[i].f + now[i].f) * ratio;
				final[i].n += alpha * now[i].n;
			}
		char sout[1024];
		snprintf(sout, sizeof sout, "%s%03d.ppm", argv[3], _);
		FILE* f = fopen(sout, "wb");
		if (!f) return perror(sout), 1;
		fprintf(f, "P6\n%d %d\n%d\n", w, h, 255);
		for (int y = h - 1; y >= 0; --y)
			for (int x = w - 1; x >= 0; --x) {
				int index = y * w + x;
				P3 col = final[index].f / (4 * PI * sqr(radius[index]) * photons * _);
				fprintf(f, "%c%c%c", gamma_trans(col.x), gamma_trans(col.y), gamma_trans(col.z));
			}
		fclose(f);
	}
	return !puts("");
}

int main(int argc, char*argv[])
{
	int positional = argc;
	if (argc >= 3 && std::string(argv[argc - 2]) == "--scene") {
		if (!select_scene(argv[argc - 1])) {
			fprintf(stderr, "Unknown scene: %s (expected vase or balls)\n", argv[argc - 1]);
			return 1;
		}
		positional -= 2;
	}
	if (positional != 5 && positional != 8) {
		fprintf(stderr, "Usage: %s WIDTH HEIGHT OUTPUT SAMPLES [PHOTONS_PER_PIXEL RADIUS ALPHA] [--scene vase|balls]\n", argv[0]);
		return 1;
	}
	if (atoi(argv[1]) <= 0 || atoi(argv[2]) <= 0 || atoi(argv[4]) <= 0 ||
		(positional == 8 && (atof(argv[5]) <= 0 || atof(argv[6]) <= 0 || atof(argv[7]) <= 0 || atof(argv[7]) > 1))) {
		fputs("Invalid rendering parameters\n", stderr);
		return 1;
	}
	if (positional == 8 && scene == balls) {
		fputs("The CPU SPPM backend supports one light; use CUDA SPPM for the balls scene\n", stderr);
		return 1;
	}
	return positional == 8 ? sppm(positional, argv) : baseline(positional, argv);
}
