#ifndef __BEZIER_H__
#define __BEZIER_H__ 

#include "utils.hpp"
#include "ray.hpp"

class BezierCurve2D
{// f(y)=x, y goes up?
public:
	ld *dx, *dy, max, height, max2;
	int n;
	ld last_t;
	// x(t) = \sum_{i=0}^n dx_i * t^i
	// y(t) = \sum_{i=0}^n dy_i * t^i
	BezierCurve2D(ld* px, ld* py, int n_): n(n_) {
		dx = new ld[n];
		dy = new ld[n];
		assert(std::abs(py[0]) <= 1e-6);
		--n;
		// preproces
		for(int i = 0; i <= n; ++i)
		{
			dx[i] = px[0];
			dy[i] = py[0];
			for (int j = 0; j <= n - i; ++j)
			{
				px[j] = px[j + 1] - px[j];
				py[j] = py[j + 1] - py[j];
			}
		}
		ld n_down = 1, fac = 1, nxt = n;
		for (int i = 0; i <= n; ++i, --nxt)
		{
			fac = fac * (i == 0 ? 1 : i);
			dx[i] = dx[i] * n_down / fac;
			dy[i] = dy[i] * n_down / fac;
			n_down *= nxt;
			// printf("i=%d %.0f %.0f\n",i,dx[i],dy[i]);
		}
		max = 0;
		height = 0;
		for (ld t = 0; t <= 1; t += 0.0001)
		{
			P3 pos = getpos(t);
			if (max < pos.x)
				max = pos.x;
			if (height < pos.y)
				height = pos.y;
		}
		max += 1e-6;
		height += 1e-6;
		max2 = max * max;
	}
	P3 getpos(ld t)
	{
		last_t = t;
		ld ans_x = 0, ans_y = 0, t_pow = 1;
		for (int i = 0; i <= n; ++i)
		{
			ans_x += dx[i] * t_pow;
			ans_y += dy[i] * t_pow;
			t_pow *= t;
		}
		return P3(ans_x, ans_y);
	}
	P3 getdir(ld t)
	{
		last_t = t;
		ld ans_x = 0, ans_y = 0, t_pow = 1;
		for(int i = 1; i <= n; ++i)
		{
			ans_x += dx[i] * i * t_pow;
			ans_y += dy[i] * i * t_pow;
			t_pow *= t;
		}
		return P3(ans_x, ans_y);
	}
};

#endif // __BEZIER_H__