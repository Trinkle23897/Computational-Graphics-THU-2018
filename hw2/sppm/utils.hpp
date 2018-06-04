#ifndef __UTILS_H__
#define __UTILS_H__

#include <bits/stdc++.h>
#include "stb_image.h"

typedef double ld;
const ld PI = acos(-1);
const ld eps = 1e-6;
const ld INF = 1 << 20;

const ld min_p[3] = {100, 100, 100};
const ld max_p[3] = {10000, 10000, 10000};

enum Refl_t { DIFF, SPEC, REFR };

int gamma(ld x) {return int(.5+255*pow(x<0?0:x>1?1:x,1/2.2));}

#endif // __UTILS_H__
