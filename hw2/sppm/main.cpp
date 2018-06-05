#include "render.hpp"

int main(int argc, char const *argv[])
{
	Ray ray(P3(100, 100, 100), P3(3,4,5));
	std::pair<int, ld> ans = find_intersect_simple(ray);
	std::cout << ans.first << " " << ans.second << std::endl;
	return 0;
}
