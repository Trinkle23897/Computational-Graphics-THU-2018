#ifndef __BEZIER_H__
#define __BEZIER_H__ 

#include "utils.hpp"
#include "ray.hpp"

class BezierCurve2D
{// f(y)=x, y goes up?
public:
	ld *dx, *dy, max, height, max2, width[20], ckpt[20];
	int n;
	P3 p0, p1, dp0, dp1, ddp0, ddp1;
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
		for (ld t = 0; t <= 1; t += 0.0001)
		{
			P3 pos = getpos(t);
			if (max < pos.x)
				max = pos.x;
		}
		for(int i = 0; i <= 10; ++i)
		{
			ckpt[i] = getpos(i * .1).y;
			width[i] = 0;
			P3 pos;
			for (ld t = std::max(i * .1 - 0.01, 0.); t <= i * .1 + 1.01 && t <= 1; t += 0.0001)
			{
				pos = getpos(t);
				if (width[i] < pos.x)
					width[i] = pos.x + eps;
			}
		}
		max += eps;
		max2 = max * max;
		p0 = getpos(0), p1 = getpos(1);
		height = p1.y;
		dp0 = getdir(eps), dp1 = getdir(1 - eps);
		ddp0 = getdir2(eps), ddp1 = getdir2(1 - eps);
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