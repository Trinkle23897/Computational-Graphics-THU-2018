#ifndef __SCENE_H__
#define __SCENE_H__

#include "obj.hpp"
#include "bezier.hpp"

const ld bezier_div_x = 3;
const ld bezier_div_y = 2.5;
ld control_x[] = {20./bezier_div_x,27./bezier_div_x,30./bezier_div_x,30./bezier_div_x,30./bezier_div_x,25./bezier_div_x,20./bezier_div_x,15./bezier_div_x,30./bezier_div_x};
ld control_y[] = {0./bezier_div_y,0./bezier_div_y,10./bezier_div_y,20./bezier_div_y,30./bezier_div_y,40./bezier_div_y,60./bezier_div_y,70./bezier_div_y,80./bezier_div_y};
BezierCurve2D bezier(control_x, control_y, 9, 9, .365);

MaterialProperties planar_material(P3 axis_u, P3 axis_v, ld roughness = -1, ld bump = 0) {
	MaterialProperties material;
	material.mapping = UV_PLANAR;
	material.axis_u = axis_u;
	material.axis_v = axis_v;
	material.roughness = roughness;
	material.bump = bump;
	return material;
}

MaterialProperties vase_material() {
	MaterialProperties material;
	material.mapping = UV_BEZIER;
	material.offset.x = .5;
	material.specular = .2;
	material.roughness = .2;
	material.bump = .85;
	return material;
}

MaterialProperties stars_material() {
	MaterialProperties material = planar_material(P3(0, 0, 1. / 15), P3(1. / 15, 0, 0));
	material.specular_mask = "star-specular-mask.png";
	return material;
}

MaterialProperties rainbow_material() {
	MaterialProperties material = planar_material(
		P3(sin(-.3) * .6 / 16.5, cos(-.3) * .6 / 16.5, 0), P3(0, 0, 1), .28);
	material.offset.x = -.25 - (P3(73, 16.5, 0) | material.axis_u);
	material.metallic = .1;
	return material;
}

Texture mapped_texture(std::string filename, Refl_t reflection, ld ior,
	P3 color, MaterialProperties material) {
	return Texture(filename, ior, color, P3(), reflection, material);
}

Object* vase_front[] = {
	new SphereObject(P3(1e5+1,40.8,81.6),   1e5, DIFF, 1.5, P3(.1,.25,.25)),//Left
	new SphereObject(P3(-1e5+99,40.8,81.6), 1e5, DIFF, 1.5, P3(.25,.75,.25)),//Right
	new SphereObject(P3(50,40.8, 1e5),      1e5, DIFF, 1.5, P3(.75,.75,.75)),//Back
	new SphereObject(P3(50,40.8,-1e5+190),  1e5, DIFF, 1.5, P3(.25,.25,.25)),//Front
	new SphereObject(P3(50, 1e5, 81.6), 1e5,
		mapped_texture("star-color.png", DIFF, 1.5, P3(.75,.75,.75), stars_material())),//Bottom
	new SphereObject(P3(50,-1e5+81.6,81.6), 1e5, DIFF, 1.5, P3(.75,.75,.75)),//Top 
	new SphereObject(P3(40,16.5,47),       16.5, SPEC, 1.5, P3(1,1,1)*.999),//Mirror
	new CubeObject(P3(0,8,84),    P3(34,10,116), DIFF, 1.5, P3(76/255.,34/255.,27/255.)),
	new BezierObject(P3(20, 9.99, 100), bezier,
		mapped_texture("vase.png", DIFF, 1.5, P3(1,1,1)*.999, vase_material())),
	new SphereObject(P3(73,16.5,78),       16.5, REFR, 1.5, P3(1,1,1)*.999),//Glas 
	// new SphereObject(P3(20,60,100),        16.5, SPEC, 1.5, P3(1,1,1)*.999),//RedBall
	new SphereObject(P3(50,681.6-.27,81.6), 600, DIFF, 1.5, P3(), P3(12,12,12)) //Lite 
};

Object* vase_back[] = {
	new SphereObject(P3(1e5+1,40.8,81.6),   1e5, DIFF, 1.5, P3(.1,.25,.25)),//Left
	new SphereObject(P3(-1e5+99,40.8,81.6), 1e5, DIFF, 1.5, P3(.25,.75,.25)),//Right
	new SphereObject(P3(50,40.8, 1e5),      1e5, DIFF, 1.5, P3(.75,.75,.75)),//Back
	new SphereObject(P3(50,40.8,-1e5+190),  1e5, DIFF, 1.5, P3(.25,.25,.25)),//Front
	new SphereObject(P3(50, 1e5, 81.6), 1e5,
		mapped_texture("star-color.png", DIFF, 1.5, P3(.75,.75,.75), stars_material())),//Botrom
	new SphereObject(P3(50,-1e5+81.6,81.6), 1e5, DIFF, 1.5, P3(.75,.75,.75)),//Top
	// new SphereObject(P3(27,16.5,47),       16.5, SPEC, 1.5, P3(1,1,1)*.999),//Mirror
	new   CubeObject(P3(0,8,0),    P3(30,10,30), DIFF, 1.5, P3(76/255.,34/255.,27/255.)),
	new BezierObject(P3(15, 9.99, 15), bezier,
		mapped_texture("vase.png", DIFF, 1.7, P3(1,1,1)*.999, vase_material())),
	new SphereObject(P3(73,16.5,40), 16.5,
		mapped_texture("rainbow.png", DIFF, 1.7, P3(1,1,1)*.999, rainbow_material())),//Main Ball
	new SphereObject(P3(45,6,45),             6, REFR, 1.7, P3(.5,.5,1)*.999),//SmallBall0
	new SphereObject(P3(44,4,95),             4, REFR, 1.7, P3(1,.5,.5)*.999),//SmallBall1
	new SphereObject(P3(56,4,105),            4, REFR, 1.7, P3(.5,1,.5)*.999),//SmallBall2
	new SphereObject(P3(67,4,112),            4, REFR, 1.7, P3(1,1,.5)*.999),//SmallBall3
	new SphereObject(P3(16,60,100),          12, REFR, 1.5, P3(1,1,1)*.999),//FlyBall
	new SphereObject(P3(50,681.6-.27,81.6), 600, DIFF, 1.5, P3(), P3(12,12,12)) //Lite
};

Object* camera_left[] = {
	new SphereObject(P3(1e5+1,40.8,81.6), 1e5, mapped_texture("wallls.com_156455.png", DIFF,
		1.5, P3(.1,.25,.25), planar_material(P3(0, 0, -1. / 150), P3(0, -1. / 100, 0), .7, .55))),//Left
	new SphereObject(P3(-1e5+299,40.8,81.6), 1e5, DIFF, 1.5, P3(.25,.75,.25)),//Right
	new SphereObject(P3(50,40.8, 1e5), 1e5, []() {
		MaterialProperties material = planar_material(P3(-1. / 125, 0, 0), P3(0, -1. / 80, 0), .82, .35);
		material.offset.y = -.05;
		return mapped_texture("greenbg.jpg", DIFF, 1.5, P3(1,1,1)*.999, material);
	}()),//Back
	new SphereObject(P3(50,40.8,-1e5+190),  1e5, DIFF, 1.5, P3(.25,.25,.25)),//Front
	new SphereObject(P3(50, 1e5, 81.6), 1e5,
		mapped_texture("star-color.png", DIFF, 1.5, P3(.75,.75,.75), stars_material())),//Botrom
	new SphereObject(P3(50,-1e5+81.6,81.6), 1e5, DIFF, 1.5, P3(.75,.75,.75)),//Top
	// new SphereObject(P3(27,16.5,47),       16.5, SPEC, 1.5, P3(1,1,1)*.999),//Mirror
	new CubeObject(P3(0,8,0), P3(30,10,30), mapped_texture("wood.jpg", DIFF, 1.5,
		P3(76/255.,34/255.,27/255.), planar_material(P3(1. / 30, 0, 0), P3(0, 0, 1. / 30), .72, 1.1))),
	new BezierObject(P3(15, 9.99, 15), bezier,
		mapped_texture("vase.png", DIFF, 1.7, P3(1,1,1)*.999, vase_material())),
	new SphereObject(P3(73,16.5,40), 16.5,
		mapped_texture("rainbow.png", DIFF, 1.7, P3(1,1,1)*.999, rainbow_material())),//Main Ball
	new SphereObject(P3(45,6,45),             6, REFR, 1.7, P3(.5,.5,1)*.999),//SmallBall0
	new SphereObject(P3(52,3,75),             3, REFR, 1.7, P3(1,.5,.5)*.999),//SmallBall1
	new SphereObject(P3(65.5,3,88),           3, REFR, 1.7, P3(.5,1,.5)*.999),//SmallBall2
	new SphereObject(P3(77,3,92),             3, REFR, 1.7, P3(1,1,.5)*.999),//SmallBall3
	// new SphereObject(P3(16,60,100),          12, REFR, 1.5, P3(1,1,1)*.999),//FlyBall
	new SphereObject(P3(50,681.6-.27,81.6), 600, DIFF, 1.5, P3(), P3(1,1,1)*20) //Lite
};

MaterialProperties balls_floor_material() {
	MaterialProperties material = planar_material(P3(1, 0, 0), P3(0, 1, 0));
	material.specular = .01;
	return material;
}

Texture balls_texture(const char* filename, P3 axis) {
	MaterialProperties material;
	material.mapping = UV_AXIS;
	material.axis_v = axis.norm();
	material.specular = .2;
	material.diffuse_mask = std::string(filename) + "-diffuse-mask.png";
	return Texture(std::string(filename) + "-color.png", 1.5,
		P3(1, 1, 1) * .999, P3(), REFR, material);
}

Object* balls[] = {
	new CubeObject(P3(-300, -100, -300), P3(200, 300, 150),
		Texture("230.png", 1.5, P3(.9, .9, .9), P3(), DIFF, balls_floor_material())),
	new SphereObject(P3(96, 294, 37), 6,
		balls_texture("w0", P3(-.79370194, -.94928056, -.54563412))),
	new SphereObject(P3(31, 292, 47), 8,
		balls_texture("w1", P3(.80076862, -.46119098, .097985))),
	new SphereObject(P3(85, 297, 37), 3,
		balls_texture("w2", P3(.60936399, .61811709, -.7216741))),
	new SphereObject(P3(21, 289, 7), 11,
		balls_texture("w3", P3(.20347676, -.98533796, -.60770924))),
	new SphereObject(P3(56, 284, 61), 16,
		balls_texture("w4", P3(-.37134236, -.62023902, .41387378))),
	new SphereObject(P3(96, 294, 37), 5.99, DIFF, 1.5, P3(), P3(9, 9, 9)),
	new SphereObject(P3(31, 292, 47), 7.99, DIFF, 1.5, P3(), P3(9, 9, 9)),
	new SphereObject(P3(85, 297, 37), 2.99, DIFF, 1.5, P3(), P3(9, 9, 9)),
	new SphereObject(P3(21, 289, 7), 10.99, DIFF, 1.5, P3(), P3(9, 9, 9)),
	new SphereObject(P3(56, 284, 61), 15.99, DIFF, 1.5, P3(), P3(9, 9, 9)),
	new SphereObject(P3(10, 277, 70), 23, REFR, 1.5, P3(.999, .999, .999)),
};

struct SceneCamera {
	P3 origin, direction;
	ld offset, scale;
};

Object** scene = camera_left;
int scene_num = sizeof camera_left / sizeof camera_left[0];
const char* scene_name = "vase";
SceneCamera scene_camera = {P3(150, 28, 260), P3(-.45, .001, -1), 150, 1.05};

bool select_scene(const std::string& name) {
	if (name == "vase") {
		scene = camera_left;
		scene_num = sizeof camera_left / sizeof camera_left[0];
		scene_name = "vase";
		scene_camera = {P3(150, 28, 260), P3(-.45, .001, -1), 150, 1.05};
	} else if (name == "balls") {
		scene = balls;
		scene_num = sizeof balls / sizeof balls[0];
		scene_name = "balls";
		scene_camera = {P3(55, 230, -220), P3(0, .2, 1), 100, 1};
	} else return false;
	return true;
}

std::pair<Refl_t, P3> get_feature(Object* obj, Texture&texture, P3 x, unsigned short *X) {
	const MaterialProperties& material = texture.material;
	ld u = x.dot(material.axis_u) + material.offset.x;
	ld v = x.dot(material.axis_v) + material.offset.y;
	if (material.mapping == UV_BEZIER) {
		P3 coordinates = obj->change_for_bezier(x);
		u = coordinates.x / (2 * PI) + material.offset.x;
		v = coordinates.y + material.offset.y;
	} else if (material.mapping == UV_SPHERICAL || material.mapping == UV_AXIS) {
		SphereObject* sphere = dynamic_cast<SphereObject*>(obj);
		if (sphere) {
			P3 local = (x - sphere->o) / sphere->r;
			if (material.mapping == UV_AXIS) {
				u = material.offset.x;
				v = local.dot(material.axis_v) * .5 + .5 + material.offset.y;
			} else {
				u = atan2(local.z, local.x) / (2 * PI) + .5 + material.offset.x;
				v = acos(std::max(-1., std::min(1., local.y))) / PI + material.offset.y;
			}
		}
	}
	std::pair<Refl_t, P3> feature = texture.getcol(u, v);
	static std::map<std::string, Texture> masks;
	if (!material.diffuse_mask.empty()) {
		auto found = masks.find(material.diffuse_mask);
		if (found == masks.end())
			found = masks.emplace(material.diffuse_mask,
				Texture(material.diffuse_mask, 1.5, P3(), P3(), DIFF)).first;
		ld diffuse = found->second.getcol(u, v).second.x;
		if (diffuse > 0) {
			feature.first = erand48(X) < diffuse ? DIFF : SPEC;
			return feature;
		}
	}
	if (!material.specular_mask.empty()) {
		auto found = masks.find(material.specular_mask);
		if (found == masks.end())
			found = masks.emplace(material.specular_mask,
				Texture(material.specular_mask, 1.5, P3(), P3(), DIFF)).first;
		if (found->second.getcol(u, v).second.x > erand48(X))
			feature.first = SPEC;
	}
	if (material.specular > 0 && erand48(X) < material.specular) {
		feature.first = SPEC;
	}
	return feature;
}

#endif // __SCENE_H__
