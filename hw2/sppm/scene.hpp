#ifndef __SCENE_H__
#define __SCENE_H__

#include "obj.hpp"
#include "bezier.hpp"

const ld bezier_div_x = 2.5;
const ld bezier_div_y = 2.5;
int scene_num = 11;
ld control_x[] = {25./bezier_div_x,30./bezier_div_x,30./bezier_div_x,30./bezier_div_x,30./bezier_div_x,25./bezier_div_x,20./bezier_div_x,15./bezier_div_x,27./bezier_div_x};
ld control_y[] = {0./bezier_div_y,0./bezier_div_y,10./bezier_div_y,20./bezier_div_y,30./bezier_div_y,40./bezier_div_y,60./bezier_div_y,70./bezier_div_y,80./bezier_div_y};
BezierCurve2D bezier(control_x, control_y, 9);

Object* smallpt_scene[] = {
	new SphereObject(P3(1e5+1,40.8,81.6),  1e5, DIFF, 1.5, P3(.1,.25,.25)),//Left 
	new SphereObject(P3(-1e5+99,40.8,81.6), 1e5, DIFF, 1.5, P3(.25,.75,.25)),//Rght 
	new SphereObject(P3(50,40.8, 1e5),      1e5, DIFF, 1.5, P3(.75,.75,.75)),//Back 
	new SphereObject(P3(50,40.8,-1e5+190),  1e5, DIFF, 1.5, P3(.25,.25,.25)),//Frnt
	new SphereObject(P3(50, 1e5, 81.6),     1e5, DIFF, 1.5, P3(.75,.75,.75), P3(), "star.png"),//Botm 
	new SphereObject(P3(50,-1e5+81.6,81.6), 1e5, DIFF, 1.5, P3(.75,.75,.75)),//Top 
	new SphereObject(P3(40,16.5,47),       16.5, SPEC, 1.5, P3(1,1,1)*.999),//Mirr
	new CubeObject(P3(0,8,84), P3(34,10,116), DIFF, 1.5, P3(76/255.,34/255.,27/255.)),
	new BezierObject(P3(20, 9.99, 100),      bezier, DIFF, 1.5, P3(1,1,1)*.999, P3(), "vase.png"),
	new SphereObject(P3(73,16.5,78),       16.5, REFR, 1.5, P3(1,1,1)*.999),//Glas 
	// new SphereObject(P3(20,60,100),        16.5, SPEC, 1.5, P3(1,1,1)*.999),//RedBall
	new SphereObject(P3(50,681.6-.27,81.6), 600, DIFF, 1.5, P3(), P3(12,12,12)) //Lite 
};

Object* test_scene[] = {
	new SphereObject(P3(50, 50, 50), 30, DIFF, 1.5, P3(.25, .75, .25)),
	new SphereObject(P3(100, 100, 100), 30, DIFF, 1.5, P3(.25, .25, .75)),
	new PlaneObject(P3(0, 0, 1./20), DIFF, 1.5, P3(), P3(), "star.png"),
	new CubeObject(P3(10, 10, 10), P3(200, 200, 200), DIFF, 1.5, P3(.75, .75, .75)),
	new SphereObject(P3(100, 100, 250-.5), 50, DIFF, 1.5, P3(), P3(12, 12, 12))
};

Object** scene = smallpt_scene;

#endif // __SCENE_H__
