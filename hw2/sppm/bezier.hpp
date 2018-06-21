#ifndef __BEZIER_H__
#define __BEZIER_H__ 

#include "utils.hpp"
#include "ray.hpp"

class BezierCurve2D
{// f(y)=x, y goes up?
public:
	ld *dx, *dy, max, height, max2, r, num;
	int n;
	struct D{
		ld t0, t1, width, y0, y1, width2;
	}data[20];
	// x(t) = \sum_{i=0}^n dx_i * t^i
	// y(t) = \sum_{i=0}^n dy_i * t^i
	BezierCurve2D(ld* px, ld* py, int n_, int num_, ld r_): num(num_), n(n_), r(r_) {
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
		}
		max = 0;
		ld interval = 1. / (num - 1), c = 0;
		for (int cnt = 0; cnt <= num; c += interval, ++cnt)
		{
			data[cnt].width = 0;
			data[cnt].t0 = std::max(0., c - r);
			data[cnt].t1 = std::min(1., c + r);
			data[cnt].y0 = getpos(data[cnt].t0).y;
			data[cnt].y1 = getpos(data[cnt].t1).y;
			for (ld t = data[cnt].t0; t <= data[cnt].t1; t += 0.00001)
			{
				P3 pos = getpos(t);
				if (data[cnt].width < pos.x)
					data[cnt].width = pos.x;
			}
			if (max < data[cnt].width)
				max = data[cnt].width;
			data[cnt].width += eps;
			data[cnt].width2 = sqr(data[cnt].width);
		}
		max += eps;
		max2 = max * max;
		height = getpos(1).y;
	}
	P3 getpos(ld t)
	{
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
		ld ans_x = 0, ans_y = 0, t_pow = 1;
		for(int i = 1; i <= n; ++i)
		{
			ans_x += dx[i] * i * t_pow;
			ans_y += dy[i] * i * t_pow;
			t_pow *= t;
		}
		return P3(ans_x, ans_y);
	}
	P3 getdir2(ld t)
	{
		ld ans_x = 0, ans_y = 0, t_pow = 1;
		for(int i = 2; i <= n; ++i)
		{
			ans_x += dx[i] * i * (i - 1) * t_pow;
			ans_y += dy[i] * i * (i - 1) * t_pow;
			t_pow *= t;
		}
		return P3(ans_x, ans_y);
	}
};

#endif // __BEZIER_H__