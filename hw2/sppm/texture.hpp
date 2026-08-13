#ifndef __TEXTURE_H__
#define __TEXTURE_H__

#include "vec3.hpp"

enum TextureMapping { UV_AUTO, UV_PLANAR, UV_SPHERICAL, UV_CYLINDRICAL, UV_BEZIER, UV_AXIS };
struct MaterialProperties {
	P3 axis_u, axis_v, offset;
	ld specular, roughness, metallic, bump;
	std::string specular_mask, diffuse_mask;
	TextureMapping mapping;
	MaterialProperties(): axis_u(0, 0, 1), axis_v(1, 0, 0), offset(),
		specular(0), roughness(-1), metallic(-1), bump(0), mapping(UV_AUTO) {}
};

class Texture {
public:
	P3 color, emission;
	Refl_t refl;
	ld brdf;
	MaterialProperties material;
	std::string filename;
	unsigned char *buf;
	int w, h, c;
	Texture(const Texture&t): color(t.color), emission(t.emission), refl(t.refl), brdf(t.brdf),
		material(t.material), filename(t.filename), buf(NULL), w(0), h(0), c(0) {
		if (t.filename != "")
			buf = stbi_load(filename.c_str(), &w, &h, &c, 0);
	}
	Texture(std::string _, ld b, P3 col, P3 e, Refl_t r,
		MaterialProperties properties = MaterialProperties()): color(col), emission(e), refl(r), brdf(b),
		material(properties), filename(_), buf(NULL), w(0), h(0), c(0) {
		if(_ != "")
			buf = stbi_load(filename.c_str(), &w, &h, &c, 0);
	}
	std::pair<Refl_t, P3> getcol(ld a, ld b) {
		if (buf == NULL)
			return std::make_pair(refl, color);
		int pw = (int(a * w) % w + w) % w, ph = (int(b * h) % h + h) % h;
		int idx = ph * w * c + pw * c;
		int x = buf[idx + 0], y = buf[idx + 1], z = buf[idx + 2];
		// printf("find point %d %d %lf %lf\n", ph, pw,a,b);
		return std::make_pair(refl, P3(x, y, z) / 255.);
	}
};

#endif // __TEXTURE_H__
