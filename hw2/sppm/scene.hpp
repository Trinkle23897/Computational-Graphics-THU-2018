#ifndef __SCENE_H__
#define __SCENE_H__

#include "obj.hpp"
#include "bezier.hpp"

const ld bezier_div_x = 3;
const ld bezier_div_y = 2.5;
ld control_x[] = {20./bezier_div_x,27./bezier_div_x,30./bezier_div_x,30./bezier_div_x,30./bezier_div_x,25./bezier_div_x,20./bezier_div_x,15./bezier_div_x,30./bezier_div_x};
ld control_y[] = {0./bezier_div_y,0./bezier_div_y,10./bezier_div_y,20./bezier_div_y,30./bezier_div_y,40./bezier_div_y,60./bezier_div_y,70./bezier_div_y,80./bezier_div_y};
BezierCurve2D bezier(control_x, control_y, 9);

Object* vase_front[] = {
	new SphereObject(P3(1e5+1,40.8,81.6),   1e5, DIFF, 1.5, P3(.1,.25,.25)),//Left
	new SphereObject(P3(-1e5+99,40.8,81.6), 1e5, DIFF, 1.5, P3(.25,.75,.25)),//Right
	new SphereObject(P3(50,40.8, 1e5),      1e5, DIFF, 1.5, P3(.75,.75,.75)),//Back
	new SphereObject(P3(50,40.8,-1e5+190),  1e5, DIFF, 1.5, P3(.25,.25,.25)),//Front
	new SphereObject(P3(50, 1e5, 81.6),     1e5, DIFF, 1.5, P3(.75,.75,.75), P3(), "star.png"),//Bottom
	new SphereObject(P3(50,-1e5+81.6,81.6), 1e5, DIFF, 1.5, P3(.75,.75,.75)),//Top 
	new SphereObject(P3(40,16.5,47),       16.5, SPEC, 1.5, P3(1,1,1)*.999),//Mirror
	new CubeObject(P3(0,8,84),    P3(34,10,116), DIFF, 1.5, P3(76/255.,34/255.,27/255.)),
	new BezierObject(P3(20, 9.99, 100),  bezier, DIFF, 1.5, P3(1,1,1)*.999, P3(), "vase.png"),
	new SphereObject(P3(73,16.5,78),       16.5, REFR, 1.5, P3(1,1,1)*.999),//Glas 
	// new SphereObject(P3(20,60,100),        16.5, SPEC, 1.5, P3(1,1,1)*.999),//RedBall
	new SphereObject(P3(50,681.6-.27,81.6), 600, DIFF, 1.5, P3(), P3(12,12,12)) //Lite 
};

Object* vase_back[] = {
	new SphereObject(P3(1e5+1,40.8,81.6),   1e5, DIFF, 1.5, P3(.1,.25,.25)),//Left
	new SphereObject(P3(-1e5+99,40.8,81.6), 1e5, DIFF, 1.5, P3(.25,.75,.25)),//Right
	new SphereObject(P3(50,40.8, 1e5),      1e5, DIFF, 1.5, P3(.75,.75,.75)),//Back
	new SphereObject(P3(50,40.8,-1e5+190),  1e5, DIFF, 1.5, P3(.25,.25,.25)),//Front
	new SphereObject(P3(50, 1e5, 81.6),     1e5, DIFF, 1.5, P3(.75,.75,.75), P3(), "star.png"),//Botrom
	new SphereObject(P3(50,-1e5+81.6,81.6), 1e5, DIFF, 1.5, P3(.75,.75,.75)),//Top
	// new SphereObject(P3(27,16.5,47),       16.5, SPEC, 1.5, P3(1,1,1)*.999),//Mirror
	new   CubeObject(P3(0,8,0),    P3(30,10,30), DIFF, 1.5, P3(76/255.,34/255.,27/255.)),
	new BezierObject(P3(15, 9.99, 15),   bezier, DIFF, 1.7, P3(1,1,1)*.999, P3(), "vase.png"),
	new SphereObject(P3(73,16.5,40),       16.5, DIFF, 1.7, P3(1,1,1)*.999, P3(), "rainbow.png"),//Main Ball
	new SphereObject(P3(45,6,45),             6, REFR, 1.7, P3(.5,.5,1)*.999),//SmallBall0
	new SphereObject(P3(44,4,95),             4, REFR, 1.7, P3(1,.5,.5)*.999),//SmallBall1
	new SphereObject(P3(56,4,105),            4, REFR, 1.7, P3(.5,1,.5)*.999),//SmallBall2
	new SphereObject(P3(67,4,112),            4, REFR, 1.7, P3(1,1,.5)*.999),//SmallBall3
	new SphereObject(P3(16,60,100),          12, REFR, 1.5, P3(1,1,1)*.999),//FlyBall
	new SphereObject(P3(50,681.6-.27,81.6), 600, DIFF, 1.5, P3(), P3(12,12,12)) //Lite
};

Object* camera_left[] = {
	new SphereObject(P3(1e5+1,40.8,81.6),   1e5, DIFF, 1.5, P3(.1,.25,.25), P3(), "pure2.png"),//Left
	new SphereObject(P3(-1e5+299,40.8,81.6), 1e5, DIFF, 1.5, P3(.25,.75,.25)),//Right
	new SphereObject(P3(50,40.8, 1e5),      1e5, DIFF, 1.5, P3(1,1,1)*.999, P3(), "pure.png"),//Back
	new SphereObject(P3(50,40.8,-1e5+190),  1e5, DIFF, 1.5, P3(.25,.25,.25)),//Front
	new SphereObject(P3(50, 1e5, 81.6),     1e5, DIFF, 1.5, P3(.75,.75,.75), P3(), "star.png"),//Botrom
	new SphereObject(P3(50,-1e5+81.6,81.6), 1e5, DIFF, 1.5, P3(.75,.75,.75)),//Top
	// new SphereObject(P3(27,16.5,47),       16.5, SPEC, 1.5, P3(1,1,1)*.999),//Mirror
	new   CubeObject(P3(0,8,0),    P3(30,10,30), DIFF, 1.5, P3(76/255.,34/255.,27/255.)),
	new BezierObject(P3(15, 9.99, 15),   bezier, DIFF, 1.7, P3(1,1,1)*.999, P3(), "vase.png"),
	new SphereObject(P3(73,16.5,40),       16.5, DIFF, 1.7, P3(1,1,1)*.999, P3(), "rainbow.png"),//Main Ball
	new SphereObject(P3(45,6,45),             6, REFR, 1.7, P3(.5,.5,1)*.999),//SmallBall0
	new SphereObject(P3(52,3,75),             3, REFR, 1.7, P3(1,.5,.5)*.999),//SmallBall1
	new SphereObject(P3(65.5,3,88),           3, REFR, 1.7, P3(.5,1,.5)*.999),//SmallBall2
	new SphereObject(P3(77,3,92),             3, REFR, 1.7, P3(1,1,.5)*.999),//SmallBall3
	// new SphereObject(P3(16,60,100),          12, REFR, 1.5, P3(1,1,1)*.999),//FlyBall
	new SphereObject(P3(50,681.6-.27,81.6), 600, DIFF, 1.5, P3(), P3(1,1,1)*13) //Lite
};

Object** scene = camera_left;
int scene_num = 14;

#endif // __SCENE_H__
