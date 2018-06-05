#ifndef __SCENE_H__
#define __SCENE_H__

#include "obj.hpp"

int scene_num = 5;
Object* scene[] = {
	new SphereObject(P3(50, 50, 50), 30, DIFF, P3(.25, .75, .25), P3()),
	new SphereObject(P3(100, 100, 100), 30, DIFF, P3(.25, .25, .75), P3()),
	new PlaneObject(P3(0, 0, 1./20), DIFF, P3(), P3(), "star.png"),
	new CubeObject(P3(10, 10, 10), P3(200, 200, 200), DIFF, P3(.75, .75, .75), P3()),
	new SphereObject(P3(100, 100, 250-.5), 50, DIFF, P3(), P3(12, 12, 12))
};

#endif // __SCENE_H__
