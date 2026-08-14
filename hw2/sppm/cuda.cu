#include <cuda_runtime.h>

#include "texture.hpp"
#include "scene.hpp"

namespace gpu {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-6;
constexpr double kInfinity = 1e20;
constexpr int kMaxObjects = 32;
constexpr int kMaxTextures = 16;
constexpr int kMaxBounces = 64;

#define CUDA_CHECK(expression) do { \
	cudaError_t error = (expression); \
	if (error != cudaSuccess) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(error)); \
		exit(1); \
	} \
} while (0)

struct Vec {
	double x, y, z;
	__host__ __device__ Vec(double x_ = 0, double y_ = 0, double z_ = 0): x(x_), y(y_), z(z_) {}
	__host__ __device__ Vec operator+(Vec a) const { return Vec(x + a.x, y + a.y, z + a.z); }
	__host__ __device__ Vec operator-(Vec a) const { return Vec(x - a.x, y - a.y, z - a.z); }
	__host__ __device__ Vec operator-() const { return Vec(-x, -y, -z); }
	__host__ __device__ Vec operator*(double a) const { return Vec(x * a, y * a, z * a); }
	__host__ __device__ Vec operator/(double a) const { return Vec(x / a, y / a, z / a); }
	__host__ __device__ Vec multiply(Vec a) const { return Vec(x * a.x, y * a.y, z * a.z); }
	__host__ __device__ Vec mix(Vec a, double factor) const { return *this * (1 - factor) + a * factor; }
	__host__ __device__ double dot(Vec a) const { return x * a.x + y * a.y + z * a.z; }
	__host__ __device__ Vec cross(Vec a) const {
		return Vec(y * a.z - z * a.y, z * a.x - x * a.z, x * a.y - y * a.x);
	}
	__host__ __device__ double squared_length() const { return dot(*this); }
	__host__ __device__ Vec normalized() const { return *this / sqrt(squared_length()); }
	__host__ __device__ double maximum() const { return fmax(x, fmax(y, z)); }
};

struct RayData {
	Vec origin, direction;
	__device__ Vec at(double distance) const { return origin + direction * distance; }
};

struct TextureData {
	unsigned char* pixels;
	int width, height, channels;
};

struct ObjectData {
	Vec a, b, color, emission, uv_u, uv_v, uv_offset;
	double radius, ior, roughness, metallic, bump, mixed_specular;
	int shape, reflection, texture, mapping, specular_mask, diffuse_mask;
};

struct BezierData {
	double x[9], y[9], lower[10], upper[10], radius, height;
	int segments;
};

struct Hit {
	Vec position, normal;
	double distance;
	int object;
};

struct Feature {
	Vec color, normal;
	double roughness, metallic, u, v;
	int reflection;
};

struct VisiblePoint {
	Vec position, normal, throughput;
	int pixel, next, cell_x, cell_y, cell_z, object;
};

struct SurfaceGuide {
	Vec position, normal, albedo;
	double directional;
	int object, mixed;
};

struct EmitterData {
	Vec position, normal, emission, extent;
	double radius, area, probability, direct_probability;
	int object, shape;
};

struct PixelState {
	Vec flux, accumulated, volume, specular;
	double radius, photons;
	unsigned int hits;
};

struct RenderConfig {
	double aperture, focus_distance, light_radius, roughness, metallic, bump_strength;
	double medium_density, medium_albedo, anisotropy, volume_density, volume_step;
	double volume_emission, volume_radius, volume_x, volume_y, volume_z, dispersion;
	double camera_x, camera_y, camera_z, direction_x, direction_y, direction_z, camera_offset, camera_scale;
	int antialias, shadow_samples, enable_pbr, texture_filter, hybrid_samples, reconstruction_radius, reuse, guided, cache;
	unsigned int seed;
};

enum Shape { kSphere, kCube, kBezier, kPlane };
enum EmitterShape { kEmitterSphere, kEmitterDisk, kEmitterBox };

__constant__ ObjectData* objects;
__constant__ TextureData textures[kMaxTextures];
__constant__ EmitterData* emitters;
__constant__ BezierData bezier_data;
__constant__ int object_count;
__constant__ int emitter_count;
__constant__ RenderConfig config;

__device__ unsigned int scramble(unsigned int value) {
	value ^= value >> 16;
	value *= 0x7feb352dU;
	value ^= value >> 15;
	value *= 0x846ca68bU;
	return value ^ (value >> 16);
}

__device__ double random(unsigned int& state) {
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return (double(state) + .5) * 2.3283064365386963e-10;
}

__device__ double low_discrepancy(unsigned int index, int iteration, int dimension, bool deterministic = false) {
	const double generator[] = {.8566748838545, .7338918566271, .6287067210378, .5385972572236};
	double shift = (scramble((deterministic ? 0 : config.seed) * 0x85ebca6bu + iteration * 0x9e3779b9u +
		dimension * 0x7feb352du) + .5) * 2.3283064365386963e-10;
	double sample = (index + 1.) * generator[dimension] + shift;
	return sample - floor(sample);
}

__device__ Vec curve_position(double parameter) {
	double x = bezier_data.x[8], y = bezier_data.y[8];
	for (int i = 7; i >= 0; --i) {
		x = x * parameter + bezier_data.x[i];
		y = y * parameter + bezier_data.y[i];
	}
	return Vec(x, y);
}

__device__ Vec curve_derivative(double parameter) {
	double x = 8 * bezier_data.x[8], y = 8 * bezier_data.y[8];
	for (int i = 7; i > 0; --i) {
		x = x * parameter + i * bezier_data.x[i];
		y = y * parameter + i * bezier_data.y[i];
	}
	return Vec(x, y);
}

__device__ double solve_height(double height) {
	double parameter = .5;
	for (int i = 0; i < 12; ++i) {
		parameter = fmax(0., fmin(1., parameter));
		Vec position = curve_position(parameter), derivative = curve_derivative(parameter);
		double error = position.y - height;
		if (fabs(error) < kEpsilon) return parameter;
		if (fabs(derivative.y) < kEpsilon) break;
		parameter -= error / derivative.y;
	}
	return -1;
}

__device__ double intersect_sphere(RayData ray, Vec center, double radius) {
	Vec offset = center - ray.origin;
	double projection = ray.direction.dot(offset);
	double discriminant = projection * projection - offset.squared_length() + radius * radius;
	if (discriminant < 0) return kInfinity;
	double root = sqrt(discriminant);
	if (projection - root > kEpsilon) return projection - root;
	return projection + root > kEpsilon ? projection + root : kInfinity;
}

__device__ double intersect_cube(RayData ray, Vec lower, Vec upper) {
	double entry = -kInfinity, exit = kInfinity;
	const double origin[] = {ray.origin.x, ray.origin.y, ray.origin.z};
	const double direction[] = {ray.direction.x, ray.direction.y, ray.direction.z};
	const double minimum[] = {lower.x, lower.y, lower.z};
	const double maximum[] = {upper.x, upper.y, upper.z};
	for (int axis = 0; axis < 3; ++axis) {
		if (fabs(direction[axis]) < kEpsilon) {
			if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) return kInfinity;
			continue;
		}
		double a = (minimum[axis] - origin[axis]) / direction[axis];
		double b = (maximum[axis] - origin[axis]) / direction[axis];
		entry = fmax(entry, fmin(a, b));
		exit = fmin(exit, fmax(a, b));
		if (entry > exit) return kInfinity;
	}
	if (entry > kEpsilon) return entry;
	return exit > kEpsilon ? exit : kInfinity;
}

__device__ double intersect_plane(RayData ray, Vec normal) {
	double denominator = ray.direction.dot(normal);
	if (fabs(denominator) < kEpsilon) return kInfinity;
	double distance = (1 - ray.origin.dot(normal)) / denominator;
	return distance > kEpsilon ? distance : kInfinity;
}

__device__ void check_bezier_root(
	RayData ray, Vec center, double a, double b, double c,
	double lower, double upper, double initial, double& nearest
) {
	double parameter = initial;
	for (int i = 0; i < 10; ++i) {
		parameter = fmax(lower, fmin(upper, parameter));
		Vec position = curve_position(parameter), derivative = curve_derivative(parameter);
		double distance = sqrt(fmax(0., a * (position.y - b) * (position.y - b) + c));
		if (distance < kEpsilon) return;
		double value = position.x - distance;
		if (fabs(value) < kEpsilon) {
			double ray_distance = (center.y + position.y - ray.origin.y) / ray.direction.y;
			if (ray_distance > kEpsilon && ray_distance < nearest) nearest = ray_distance;
			return;
		}
		double slope = derivative.x - a * (position.y - b) * derivative.y / distance;
		if (fabs(slope) < kEpsilon) return;
		parameter -= value / slope;
	}
}

__device__ double intersect_bezier(RayData ray, Vec center) {
	Vec minimum(center.x - bezier_data.radius, center.y, center.z - bezier_data.radius);
	Vec maximum(center.x + bezier_data.radius, center.y + bezier_data.height, center.z + bezier_data.radius);
	if (intersect_cube(ray, minimum, maximum) >= kInfinity) return kInfinity;

	if (fabs(ray.direction.y) < 5e-4) {
		double parameter = solve_height(ray.origin.y - center.y);
		if (parameter < 0) return kInfinity;
		Vec position = curve_position(parameter);
		double distance = intersect_sphere(ray, Vec(center.x, ray.origin.y, center.z), position.x);
		if (distance >= kInfinity) return kInfinity;
		double height = ray.at(distance).y - center.y;
		if (height <= kEpsilon || height >= bezier_data.height - kEpsilon) return kInfinity;
		parameter = solve_height(height);
		if (parameter < 0) return kInfinity;
		position = curve_position(parameter);
		return intersect_sphere(ray, Vec(center.x, center.y + position.y, center.z), position.x);
	}

	double dx = ray.direction.x / ray.direction.y;
	double dz = ray.direction.z / ray.direction.y;
	double ox = ray.origin.x - center.x - dx * ray.origin.y;
	double oz = ray.origin.z - center.z - dz * ray.origin.y;
	double a = dx * dx + dz * dz;
	if (a < kEpsilon) return kInfinity;
	double b = 2 * (ox * dx + oz * dz);
	double c = ox * ox + oz * oz - b * b / (4 * a);
	b = -b / (2 * a) - center.y;
	double nearest = kInfinity;
	for (int i = 0; i <= bezier_data.segments; ++i) {
		double lower = bezier_data.lower[i], upper = bezier_data.upper[i];
		check_bezier_root(ray, center, a, b, c, lower, upper, (2 * lower + upper) / 3, nearest);
		check_bezier_root(ray, center, a, b, c, lower, upper, (lower + 2 * upper) / 3, nearest);
	}
	return nearest;
}

__device__ Vec object_normal(ObjectData object, Vec point) {
	if (object.shape == kSphere) return (point - object.a).normalized();
	if (object.shape == kPlane) return object.a.normalized();
	if (object.shape == kCube) {
		if (fabs(point.x - object.a.x) < 1e-4) return Vec(-1);
		if (fabs(point.x - object.b.x) < 1e-4) return Vec(1);
		if (fabs(point.y - object.a.y) < 1e-4) return Vec(0, -1);
		if (fabs(point.y - object.b.y) < 1e-4) return Vec(0, 1);
		return Vec(0, 0, fabs(point.z - object.b.z) < 1e-4 ? 1 : -1);
	}
	double parameter = solve_height(point.y - object.a.y);
	Vec derivative = curve_derivative(parameter);
	double angle = atan2(point.z - object.a.z, point.x - object.a.x);
	Vec circle(-sin(angle), 0, cos(angle));
	Vec surface(cos(angle), derivative.y / derivative.x, sin(angle));
	return circle.cross(surface).normalized();
}

__device__ bool intersect(RayData ray, bool include_light, Hit& hit) {
	hit.object = -1;
	hit.distance = kInfinity;
	for (int index = 0; index < object_count; ++index) {
		ObjectData object = objects[index];
		if (!include_light && object.emission.maximum() > 0 && object.shape == kSphere) continue;
		double distance = object.shape == kSphere ? intersect_sphere(ray, object.a, object.radius)
			: object.shape == kCube ? intersect_cube(ray, object.a, object.b)
			: object.shape == kPlane ? intersect_plane(ray, object.a)
			: intersect_bezier(ray, object.a);
		if (distance < hit.distance) {
			hit.distance = distance;
			hit.object = index;
		}
	}
	if (hit.object < 0) return false;
	hit.position = ray.at(hit.distance);
	hit.normal = object_normal(objects[hit.object], hit.position);
	return true;
}

__device__ Vec texel(TextureData texture, int column, int row) {
	column = (column % texture.width + texture.width) % texture.width;
	row = (row % texture.height + texture.height) % texture.height;
	int offset = (row * texture.width + column) * texture.channels;
	return Vec(texture.pixels[offset], texture.pixels[offset + 1], texture.pixels[offset + 2]) / 255.;
}

__device__ Vec texture_sample(TextureData texture, double u, double v, bool filtered) {
	double x = u * texture.width, y = v * texture.height;
	int column = int(floor(x)), row = int(floor(y));
	if (!filtered) return texel(texture, int(x), int(y));
	double dx = x - column, dy = y - row;
	Vec top = texel(texture, column, row).mix(texel(texture, column + 1, row), dx);
	Vec bottom = texel(texture, column, row + 1).mix(texel(texture, column + 1, row + 1), dx);
	return top.mix(bottom, dy);
}

__device__ double luminance(Vec color) {
	return color.dot(Vec(.2126, .7152, .0722));
}

__device__ Vec bump_normal(Hit hit, ObjectData object, TextureData texture, double u, double v) {
	double strength = config.bump_strength * object.bump;
	if (strength <= 0) return hit.normal;
	int offset = object.mapping == UV_BEZIER ? 6 : 3;
	double du = offset / double(texture.width), dv = offset / double(texture.height);
	double slope_u = luminance(texture_sample(texture, u + du, v, true)) -
		luminance(texture_sample(texture, u - du, v, true));
	double slope_v = luminance(texture_sample(texture, u, v + dv, true)) -
		luminance(texture_sample(texture, u, v - dv, true));
	Vec tangent;
	if (object.mapping == UV_BEZIER || object.mapping == UV_CYLINDRICAL) {
		double angle = atan2(hit.position.z - object.a.z, hit.position.x - object.a.x);
		tangent = Vec(-sin(angle), 0, cos(angle));
	} else {
		tangent = (fabs(hit.normal.y) < .9 ? Vec(0, 1) : Vec(1)).cross(hit.normal).normalized();
	}
	Vec bitangent = hit.normal.cross(tangent);
	return (hit.normal - tangent * (slope_u * strength) - bitangent * (slope_v * strength)).normalized();
}

__device__ Feature feature(Hit hit, unsigned int& state, bool stochastic = true) {
	ObjectData object = objects[hit.object];
	Feature result{object.color, hit.normal, object.roughness, object.metallic, 0, 0, object.reflection};
	if (object.texture >= 0) {
		TextureData texture = textures[object.texture];
		double u = hit.position.dot(object.uv_u) + object.uv_offset.x;
		double v = hit.position.dot(object.uv_v) + object.uv_offset.y;
		if (object.mapping == UV_BEZIER || object.mapping == UV_CYLINDRICAL) {
			double angle = atan2(hit.position.z - object.a.z, hit.position.x - object.a.x);
			if (angle < 0) angle += 2 * kPi;
			u = angle / (2 * kPi) + object.uv_offset.x;
			v = object.mapping == UV_BEZIER
				? solve_height(hit.position.y - object.a.y) + object.uv_offset.y
				: hit.position.dot(object.uv_v) + object.uv_offset.y;
		} else if (object.mapping == UV_SPHERICAL) {
			Vec local = (hit.position - object.a).normalized();
			u = atan2(local.z, local.x) / (2 * kPi) + .5 + object.uv_offset.x;
			v = acos(fmax(-1., fmin(1., local.y))) / kPi + object.uv_offset.y;
		} else if (object.mapping == UV_AXIS) {
			u = object.uv_offset.x;
			v = (hit.position - object.a).dot(object.uv_v) /
				fmax(kEpsilon, object.radius) * .5 + .5 + object.uv_offset.y;
		}
		result.color = texture_sample(texture, u, v, config.texture_filter != 0);
		result.normal = bump_normal(hit, object, texture, u, v);
		result.u = u;
		result.v = v;
		if (object.diffuse_mask >= 0) {
			double diffuse = texture_sample(textures[object.diffuse_mask], u, v, false).x;
			if (diffuse > 0) {
				result.reflection = stochastic && random(state) < diffuse ? DIFF : SPEC;
				return result;
			}
		}
		if (object.specular_mask >= 0) {
			double specular = texture_sample(textures[object.specular_mask], u, v, false).x;
			if (specular > 0 && (!stochastic || random(state) < specular)) {
				result.reflection = SPEC;
				return result;
			}
		}
	}
	if (stochastic && object.mixed_specular > 0 && random(state) < object.mixed_specular)
		result.reflection = SPEC;
	return result;
}

__device__ Vec reflect(Vec direction, Vec normal) {
	return direction - normal * (2 * normal.dot(direction));
}

__device__ bool refract(Vec direction, Vec normal, double from, double to, Vec& result) {
	double cosine = direction.normalized().dot(normal);
	double ratio = from / to;
	double squared_cosine = 1 - ratio * ratio * (1 - cosine * cosine);
	if (squared_cosine <= 0) return false;
	double transmitted = sqrt(squared_cosine);
	if (cosine > 0) transmitted = -transmitted;
	result = (direction * ratio - normal * (ratio * cosine + transmitted)).normalized();
	return true;
}

__device__ Vec sample_diffuse(
	Vec normal, unsigned int& state, double first = -1, double second = -1
) {
	double angle = 2 * kPi * (first < 0 ? random(state) : first);
	double radius = second < 0 ? random(state) : second;
	Vec tangent = (fabs(normal.x) > .1 ? Vec(0, 1) : Vec(1)).cross(normal).normalized();
	Vec bitangent = normal.cross(tangent);
	return (tangent * (cos(angle) * sqrt(radius)) +
		bitangent * (sin(angle) * sqrt(radius)) +
		normal * sqrt(1 - radius)).normalized();
}

__device__ Vec schlick(Vec base, double cosine) {
	double weight = pow(fmax(0., 1 - cosine), 5);
	return base + (Vec(1, 1, 1) - base) * weight;
}

__device__ double ggx_distribution(double normal_half, double roughness) {
	double alpha = fmax(.025, roughness * roughness);
	double alpha_squared = alpha * alpha;
	double denominator = normal_half * normal_half * (alpha_squared - 1) + 1;
	return alpha_squared / (kPi * denominator * denominator);
}

__device__ double smith_geometry(double normal_view, double normal_light, double roughness) {
	double k = (roughness + 1) * (roughness + 1) / 8;
	double view = normal_view / (normal_view * (1 - k) + k);
	double light = normal_light / (normal_light * (1 - k) + k);
	return view * light;
}

__device__ Vec pbr_brdf(Feature material, Vec normal, Vec view, Vec light) {
	double normal_view = fmax(0., normal.dot(view));
	double normal_light = fmax(0., normal.dot(light));
	if (normal_view <= kEpsilon || normal_light <= kEpsilon) return Vec();
	Vec half = (view + light).normalized();
	double normal_half = fmax(0., normal.dot(half));
	double view_half = fmax(0., view.dot(half));
	Vec base = Vec(.04, .04, .04).mix(material.color, material.metallic);
	Vec fresnel = schlick(base, view_half);
	double distribution = ggx_distribution(normal_half, material.roughness);
	double geometry = smith_geometry(normal_view, normal_light, material.roughness);
	Vec specular = fresnel * (distribution * geometry / fmax(kEpsilon, 4 * normal_view * normal_light));
	Vec diffuse = (Vec(1, 1, 1) - fresnel).multiply(material.color) * ((1 - material.metallic) / kPi);
	return diffuse + specular;
}

__device__ Vec sample_ggx(Vec normal, double roughness, unsigned int& state) {
	double alpha = fmax(.025, roughness * roughness);
	double angle = 2 * kPi * random(state);
	double sample = random(state);
	double cosine = sqrt((1 - sample) / (1 + (alpha * alpha - 1) * sample));
	double sine = sqrt(fmax(0., 1 - cosine * cosine));
	Vec tangent = (fabs(normal.x) > .1 ? Vec(0, 1) : Vec(1)).cross(normal).normalized();
	Vec bitangent = normal.cross(tangent);
	return (tangent * (cos(angle) * sine) + bitangent * (sin(angle) * sine) + normal * cosine).normalized();
}

__device__ Vec sample_sphere(unsigned int& state) {
	double height = 1 - 2 * random(state), angle = 2 * kPi * random(state);
	double radius = sqrt(fmax(0., 1 - height * height));
	return Vec(radius * cos(angle), height, radius * sin(angle));
}

__device__ EmitterData choose_emitter(unsigned int& state, bool direct = false, double sample = -1) {
	double value = sample < 0 ? random(state) : sample, accumulated = 0;
	for (int index = 0; index < emitter_count - 1; ++index) {
		accumulated += direct ? emitters[index].direct_probability : emitters[index].probability;
		if (value < accumulated) return emitters[index];
	}
	return emitters[emitter_count - 1];
}

__device__ Vec sample_light(EmitterData emitter, Vec& normal, unsigned int& state,
	const Vec* shading_point = nullptr, double* area = nullptr, double first = -1, double second = -1) {
	if (area) *area = emitter.area;
	if (emitter.shape == kEmitterSphere) {
		if (first < 0) {
			normal = sample_sphere(state);
		} else {
			double height = 1 - 2 * first, angle = 2 * kPi * second;
			double radius = sqrt(fmax(0., 1 - height * height));
			normal = Vec(radius * cos(angle), height, radius * sin(angle));
		}
		return emitter.position + normal * emitter.radius;
	}
	if (emitter.shape == kEmitterBox) {
		double xy = emitter.extent.x * emitter.extent.y;
		double xz = emitter.extent.x * emitter.extent.z;
		double yz = emitter.extent.y * emitter.extent.z;
		if (shading_point) {
			Vec upper = emitter.position + emitter.extent;
			double xface = shading_point->x < emitter.position.x || shading_point->x > upper.x ? yz : 0;
			double yface = shading_point->y < emitter.position.y || shading_point->y > upper.y ? xz : 0;
			double zface = shading_point->z < emitter.position.z || shading_point->z > upper.z ? xy : 0;
			double visible = xface + yface + zface;
			if (visible > 0) {
				if (area) *area = visible;
				double side = random(state) * visible;
				if (side < xface) {
					bool positive = shading_point->x > upper.x;
					normal = Vec(positive ? 1 : -1);
					return emitter.position + Vec(positive ? emitter.extent.x : 0,
						random(state) * emitter.extent.y, random(state) * emitter.extent.z);
				}
				if (side < xface + yface) {
					bool positive = shading_point->y > upper.y;
					normal = Vec(0, positive ? 1 : -1);
					return emitter.position + Vec(random(state) * emitter.extent.x,
						positive ? emitter.extent.y : 0, random(state) * emitter.extent.z);
				}
				bool positive = shading_point->z > upper.z;
				normal = Vec(0, 0, positive ? 1 : -1);
				return emitter.position + Vec(random(state) * emitter.extent.x,
					random(state) * emitter.extent.y, positive ? emitter.extent.z : 0);
			}
		}
		double side = random(state) * (xy + xz + yz);
		bool positive = random(state) < .5;
		if (side < yz) {
			normal = Vec(positive ? 1 : -1);
			return emitter.position + Vec(positive ? emitter.extent.x : 0,
				random(state) * emitter.extent.y, random(state) * emitter.extent.z);
		}
		if (side < yz + xz) {
			normal = Vec(0, positive ? 1 : -1);
			return emitter.position + Vec(random(state) * emitter.extent.x,
				positive ? emitter.extent.y : 0, random(state) * emitter.extent.z);
		}
		normal = Vec(0, 0, positive ? 1 : -1);
		return emitter.position + Vec(random(state) * emitter.extent.x,
			random(state) * emitter.extent.y, positive ? emitter.extent.z : 0);
	}
	double radius = emitter.radius * sqrt(first < 0 ? random(state) : first);
	double angle = 2 * kPi * (second < 0 ? random(state) : second);
	normal = emitter.normal;
	Vec tangent = (fabs(normal.x) > .1 ? Vec(0, 1) : Vec(1)).cross(normal).normalized();
	return emitter.position + tangent * (radius * cos(angle)) +
		normal.cross(tangent) * (radius * sin(angle));
}

__device__ double phase_henyey_greenstein(double cosine, double anisotropy) {
	double denominator = 1 + anisotropy * anisotropy - 2 * anisotropy * cosine;
	return (1 - anisotropy * anisotropy) / (4 * kPi * pow(fmax(1e-5, denominator), 1.5));
}

__device__ Vec sample_phase(Vec direction, unsigned int& state) {
	double g = config.anisotropy;
	double sample = random(state);
	double cosine;
	if (fabs(g) < 1e-3) {
		cosine = 1 - 2 * sample;
	} else {
		double ratio = (1 - g * g) / (1 - g + 2 * g * sample);
		cosine = (1 + g * g - ratio * ratio) / (2 * g);
	}
	cosine = fmax(-1., fmin(1., cosine));
	double sine = sqrt(fmax(0., 1 - cosine * cosine));
	double angle = 2 * kPi * random(state);
	Vec tangent = (fabs(direction.x) > .1 ? Vec(0, 1) : Vec(1)).cross(direction).normalized();
	Vec bitangent = direction.cross(tangent);
	return (tangent * (cos(angle) * sine) + bitangent * (sin(angle) * sine) + direction * cosine).normalized();
}

__device__ double noise_value(int x, int y, int z) {
	return scramble(unsigned(x) * 73856093u ^ unsigned(y) * 19349663u ^ unsigned(z) * 83492791u) *
		2.3283064365386963e-10;
}

__device__ double smooth_noise(Vec point) {
	int x = int(floor(point.x)), y = int(floor(point.y)), z = int(floor(point.z));
	double fx = point.x - x, fy = point.y - y, fz = point.z - z;
	fx = fx * fx * (3 - 2 * fx);
	fy = fy * fy * (3 - 2 * fy);
	fz = fz * fz * (3 - 2 * fz);
	double value = 0;
	for (int dz = 0; dz < 2; ++dz)
		for (int dy = 0; dy < 2; ++dy)
			for (int dx = 0; dx < 2; ++dx)
				value += noise_value(x + dx, y + dy, z + dz) *
					(dx ? fx : 1 - fx) * (dy ? fy : 1 - fy) * (dz ? fz : 1 - fz);
	return value;
}

__device__ double volume_density_at(Vec position) {
	Vec center(config.volume_x, config.volume_y, config.volume_z);
	Vec offset = (position - center) / config.volume_radius;
	double distance = offset.squared_length();
	if (distance >= 1) return 0;
	double shape = (1 - distance) * (1 - distance);
	double noise = .55 * smooth_noise(position * .11) + .3 * smooth_noise(position * .23) +
		.15 * smooth_noise(position * .47);
	return config.volume_density * shape * (.2 + 1.6 * noise);
}

__device__ bool shadow_visible(Vec origin, Vec destination, double& transmittance) {
	Vec delta = destination - origin;
	double distance = sqrt(delta.squared_length());
	Vec direction = delta / distance;
	Hit obstacle;
	if (intersect(RayData{origin + direction * 2e-4, direction}, false, obstacle) &&
		obstacle.distance < distance - 4e-4) return false;
	transmittance = exp(-config.medium_density * distance);
	if (config.volume_density > 0) {
		int steps = min(12, max(1, int(distance / fmax(1., config.volume_step * 3))));
		double step = distance / steps;
		double optical_depth = 0;
		for (int i = 0; i < steps; ++i)
			optical_depth += volume_density_at(origin + direction * ((i + .5) * step)) * step;
		transmittance *= exp(-optical_depth);
	}
	return true;
}

__device__ Vec direct_lighting(Hit hit, Feature material, Vec view, unsigned int& state,
	unsigned int sequence = UINT_MAX, unsigned int rotation = 0) {
	if (config.shadow_samples <= 0 || emitter_count <= 0) return Vec();
	Vec normal = material.normal.dot(view) > 0 ? material.normal : -material.normal;
	Vec total;
	for (int sample = 0; sample < config.shadow_samples; ++sample) {
		EmitterData emitter = choose_emitter(state, true);
		if (emitter.direct_probability <= 0) continue;
		Vec emitter_normal;
		double area;
		double first = -1, second = -1;
		if (sequence != UINT_MAX && emitter.shape != kEmitterBox) {
			unsigned int index = sequence * config.shadow_samples + sample;
			first = low_discrepancy(index, rotation, 0, true);
			second = low_discrepancy(index, rotation, 1, true);
		}
		Vec target = sample_light(emitter, emitter_normal, state, &hit.position, &area, first, second);
		Vec delta = target - hit.position;
		double distance_squared = delta.squared_length();
		Vec direction = delta / sqrt(distance_squared);
		double normal_light = fmax(0., normal.dot(direction));
		double emitter_cosine = fmax(0., -direction.dot(emitter_normal));
		if (normal_light <= 0 || emitter_cosine <= 0) continue;
		double transmittance;
		if (!shadow_visible(hit.position + normal * 3e-4, target, transmittance)) continue;
		Vec brdf = config.enable_pbr
			? pbr_brdf(material, normal, view, direction)
			: material.color / kPi;
		double geometry = normal_light * emitter_cosine * area /
			fmax(kEpsilon, distance_squared * emitter.direct_probability);
		total = total + brdf.multiply(emitter.emission) * (geometry * transmittance);
	}
	return total / config.shadow_samples;
}

__device__ Vec medium_lighting(Vec position, Vec direction, unsigned int& state) {
	if (emitter_count <= 0) return Vec();
	EmitterData emitter = choose_emitter(state);
	Vec emitter_normal;
	Vec target = sample_light(emitter, emitter_normal, state);
	Vec delta = target - position;
	double distance_squared = delta.squared_length();
	Vec light = delta / sqrt(distance_squared);
	double cosine = fmax(0., -light.dot(emitter_normal));
	double transmittance;
	if (cosine <= 0 || !shadow_visible(position, target, transmittance)) return Vec();
	double phase = phase_henyey_greenstein(direction.dot(light), config.anisotropy);
	double geometry = cosine * emitter.area / fmax(kEpsilon, distance_squared * emitter.probability);
	return emitter.emission * (phase * geometry * transmittance);
}

__device__ Vec march_volume(
	RayData ray, double surface_distance, unsigned int& state, double& transmittance
) {
	transmittance = 1;
	if (config.volume_density <= 0) return Vec();
	Vec center(config.volume_x, config.volume_y, config.volume_z);
	Vec offset = ray.origin - center;
	double projection = ray.direction.dot(offset);
	double discriminant = projection * projection - offset.squared_length() +
		config.volume_radius * config.volume_radius;
	if (discriminant <= 0) return Vec();
	double root = sqrt(discriminant);
	double start = fmax(0., -projection - root);
	double end = fmin(surface_distance, -projection + root);
	if (start >= end) return Vec();
	double step = fmax(.15, config.volume_step);
	double position = start + random(state) * step;
	Vec result;
	for (int i = 0; i < 96 && position < end; ++i, position += step) {
		Vec sample = ray.at(position);
		double density = volume_density_at(sample);
		if (density <= kEpsilon) continue;
		double attenuation = exp(-density * step);
		Vec emission(1, .48, .16);
		Vec source = emission * config.volume_emission +
			medium_lighting(sample, -ray.direction, state) * config.medium_albedo;
		result = result + source * (transmittance * (1 - attenuation));
		transmittance *= attenuation;
		if (transmittance < 1e-3) break;
	}
	return result;
}

__device__ bool scatter(
	RayData& ray, Hit hit, Feature material, Vec& throughput, unsigned int& state, int depth, int& wavelength,
	bool from_light = false, double first = -1, double second = -1
) {
	Vec normal = material.normal.dot(ray.direction) < 0 ? material.normal : -material.normal;
	bool entering = hit.normal.dot(ray.direction) < 0;
	if (material.color.maximum() < kEpsilon) return false;
	if (depth > (config.guided && from_light ? 0 : 5)) {
		double probability = fmin(.999, material.color.maximum());
		if (random(state) >= probability) return false;
		throughput = throughput / probability;
	}
	if (material.reflection == DIFF) {
		if (!config.enable_pbr) {
			throughput = throughput.multiply(material.color);
			ray = RayData{hit.position, sample_diffuse(normal, state, first, second)};
			return true;
		}
		Vec view = -ray.direction;
		Vec base = Vec(.04, .04, .04).mix(material.color, material.metallic);
		double specular_probability = fmin(.9, fmax(.08, luminance(base) + material.metallic * .35));
		Vec direction;
		if (random(state) < specular_probability) {
			Vec half = sample_ggx(normal, material.roughness, state);
			direction = reflect(ray.direction, half).normalized();
		} else {
			direction = sample_diffuse(normal, state);
		}
		double normal_light = normal.dot(direction);
		if (normal_light <= kEpsilon) return false;
		Vec half = (view + direction).normalized();
		double specular_pdf = ggx_distribution(fmax(0., normal.dot(half)), material.roughness) *
			fmax(0., normal.dot(half)) / fmax(kEpsilon, 4 * fabs(view.dot(half)));
		double diffuse_pdf = normal_light / kPi;
		double probability = specular_probability * specular_pdf + (1 - specular_probability) * diffuse_pdf;
		throughput = throughput.multiply(pbr_brdf(material, normal, view, direction)) *
			(normal_light / fmax(kEpsilon, probability));
		ray = RayData{hit.position, direction};
		return true;
	}
	throughput = throughput.multiply(material.color);
	Vec reflected = reflect(ray.direction, normal);
	if (material.reflection == SPEC) {
		if (config.enable_pbr && material.roughness > .04) {
			Vec half = sample_ggx(normal, material.roughness, state);
			Vec glossy = reflect(ray.direction, half);
			if (glossy.dot(normal) > 0) reflected = glossy.normalized();
		}
		ray = RayData{hit.position, reflected};
		return true;
	}
	Vec transmitted;
	double ior = objects[hit.object].ior;
	if (config.dispersion > 0) {
		if (wavelength < 0) {
			wavelength = min(2, int(random(state) * 3));
			Vec spectral(wavelength == 0 ? 3 : 0, wavelength == 1 ? 3 : 0, wavelength == 2 ? 3 : 0);
			throughput = throughput.multiply(spectral);
		}
		ior += (wavelength - 1) * config.dispersion;
	}
	if (!refract(ray.direction, hit.normal, entering ? 1 : ior, entering ? ior : 1, transmitted)) {
		ray = RayData{hit.position, reflected};
		return true;
	}
	double base = (ior - 1) / (ior + 1);
	base *= base;
	double cosine = 1 - (entering ? -ray.direction.dot(normal) : transmitted.dot(hit.normal));
	double reflection = base + (1 - base) * cosine * cosine * cosine * cosine * cosine;
	double probability = config.reuse ? .1 + .8 * reflection : .25 + .5 * reflection;
	if (random(state) < probability) {
		throughput = throughput * (reflection / probability);
		ray = RayData{hit.position, reflected};
	} else {
		double transport = from_light ? (entering ? 1 / (ior * ior) : ior * ior) : 1;
		throughput = throughput * ((1 - reflection) * transport / (1 - probability));
		ray = RayData{hit.position, transmitted};
	}
	return true;
}

__device__ RayData camera_ray(int x, int y, int sx, int sy, int width, int height,
	unsigned int& state, double first = -1, double second = -1) {
	Vec origin(config.camera_x, config.camera_y, config.camera_z);
	Vec direction = Vec(config.direction_x, config.direction_y, config.direction_z).normalized();
	Vec horizontal(width * .33 / height * config.camera_scale);
	Vec vertical = Vec(width * .33 / height, 0, 0).cross(Vec(direction.x, 0, direction.z)).normalized() * .33;
	double a = first >= 0 ? 2 * first : config.cache && config.aperture <= 0 ? 1 : 2 * random(state);
	double b = second >= 0 ? 2 * second : config.cache && config.aperture <= 0 ? 1 : 2 * random(state);
	double dx = a < 1 ? sqrt(a) : 2 - sqrt(2 - a);
	double dy = b < 1 ? sqrt(b) : 2 - sqrt(2 - b);
	Vec ray = (horizontal * ((x + (sx + dx) / config.antialias) / width - .5) +
		vertical * ((y + (sy + dy) / config.antialias) / height - .5) + direction).normalized();
	if (config.aperture > 0) {
		double radius = config.aperture * sqrt(random(state));
		double angle = 2 * kPi * random(state);
		Vec right = direction.cross(Vec(0, 1)).normalized();
		Vec up = right.cross(direction).normalized();
		Vec lens = origin + right * (radius * cos(angle)) + up * (radius * sin(angle));
		Vec focus = origin + ray * (config.focus_distance / fmax(kEpsilon, ray.dot(direction)));
		ray = (focus - lens).normalized();
		origin = lens;
	}
	return RayData{origin + ray * config.camera_offset, ray};
}

__device__ Vec path_trace(RayData ray, unsigned int& state, double* directional_events = nullptr,
	double primary_selector = -1, double first = -1, double second = -1) {
	Vec throughput(1, 1, 1), result;
	int wavelength = -1;
	bool specular_path = true;
	if (directional_events) *directional_events = 0;
	for (int depth = 0; depth < kMaxBounces; ++depth) {
		Hit hit;
		if (!intersect(ray, true, hit)) break;
		if (config.medium_density > 0) {
			double distance = -log(fmax(1e-12, 1 - random(state))) / config.medium_density;
			if (distance < hit.distance) {
				Vec position = ray.at(distance);
				throughput = throughput * config.medium_albedo;
				result = result + throughput.multiply(medium_lighting(position, -ray.direction, state));
				ray = RayData{position, sample_phase(ray.direction, state)};
				specular_path = false;
				continue;
			}
		}
		if (config.volume_density > 0 && depth < 3) {
			double transmittance;
			Vec volume = march_volume(ray, hit.distance, state, transmittance);
			result = result + throughput.multiply(volume);
			throughput = throughput * transmittance;
		}
		ObjectData object = objects[hit.object];
		if (object.emission.maximum() > 0) {
			if (specular_path || config.shadow_samples == 0)
				result = result + throughput.multiply(object.emission);
			break;
		}
		Feature material = feature(hit, state);
		if (depth == 0 && primary_selector >= 0 && object.reflection == DIFF &&
			object.mixed_specular > 0 && object.specular_mask < 0 && object.diffuse_mask < 0) {
			material = feature(hit, state, false);
			if (primary_selector < object.mixed_specular) material.reflection = SPEC;
		}
		if (directional_events && material.reflection != DIFF && depth < 3)
			*directional_events += material.reflection == REFR ? 1 : .5;
		if (material.reflection == DIFF)
			result = result + throughput.multiply(direct_lighting(hit, material, -ray.direction, state));
		specular_path = material.reflection != DIFF;
		if (!scatter(ray, hit, material, throughput, state, depth, wavelength, false,
			depth == 0 ? first : -1, depth == 0 ? second : -1)) break;
	}
	return result;
}

__global__ void path_trace_kernel(Vec* image, int width, int height, int samples) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	int x = pixel % width, y = pixel / width;
	unsigned int state = scramble(pixel + 1 + config.seed * 0x9e3779b9u);
	Vec color;
	for (int sy = 0; sy < config.antialias; ++sy)
		for (int sx = 0; sx < config.antialias; ++sx)
			for (int sample = 0; sample < samples; ++sample)
				color = color + path_trace(camera_ray(x, y, sx, sy, width, height, state), state);
	image[pixel] = color / (double(config.antialias * config.antialias) * samples);
}

__global__ void control_path_kernel(Vec* image, int width, int height, int samples) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	int x = pixel % width, y = pixel / width;
	unsigned int state = scramble(pixel + 1 + config.seed * 0x9e3779b9u), probe = state;
	Hit hit;
	if (!intersect(camera_ray(x, y, 0, 0, width, height, probe), true, hit) ||
		objects[hit.object].reflection != DIFF) {
		image[pixel] = Vec();
		return;
	}
	Vec color;
	int subpixels = config.antialias * config.antialias;
	for (int sy = 0; sy < config.antialias; ++sy)
		for (int sx = 0; sx < config.antialias; ++sx)
			for (int sample = 0; sample < samples; ++sample) {
				unsigned int index = sample * subpixels + sy * config.antialias + sx;
				double selector = (sample + random(state)) / samples;
				RayData ray = camera_ray(x, y, sx, sy, width, height, state);
				color = color + path_trace(ray, state, nullptr, selector,
					low_discrepancy(index, pixel, 0), low_discrepancy(index, pixel, 1));
			}
	image[pixel] = color / (double(subpixels) * samples);
}

__global__ void surface_guides_kernel(SurfaceGuide* guides, int width, int height) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	int x = pixel % width, y = pixel / width;
	unsigned int state = scramble(pixel + 1 + (config.cache ? 0 : config.seed * 0x9e3779b9u));
	SurfaceGuide guide{};
	guide.object = -1;
	int samples = 0;
	for (int sy = 0; sy < config.antialias; ++sy)
		for (int sx = 0; sx < config.antialias; ++sx) {
			Hit hit;
			if (!intersect(camera_ray(x, y, sx, sy, width, height, state), true, hit)) {
				guides[pixel].object = -1;
				return;
			}
			Feature material = feature(hit, state, false);
			ObjectData object = objects[hit.object];
			int classification = material.reflection == DIFF && object.reflection == DIFF
				? (object.mixed_specular * config.antialias * config.antialias * 16 > 1 ? 1 : 0)
				: material.reflection == REFR && object.reflection == REFR && object.texture < 0 ? 2
				: config.cache && object.reflection == REFR && object.diffuse_mask >= 0 ? 3 : -1;
			if (classification < 0 ||
				(samples && (hit.object != guide.object ||
					classification != guide.mixed ||
					(material.color - guide.albedo / samples).squared_length() > .0025))) {
				guides[pixel].object = -1;
				return;
			}
			guide.mixed = classification;
			guide.object = hit.object;
			guide.position = guide.position + hit.position;
			guide.normal = guide.normal + material.normal;
			guide.albedo = guide.albedo + material.color;
			++samples;
		}
	if (samples) {
		guide.position = guide.position / samples;
		guide.normal = guide.normal.normalized();
		guide.albedo = guide.albedo / samples;
	}
	guides[pixel] = guide;
}

__global__ void control_residual_kernel(
	Vec* coarse, const Vec* image, const SurfaceGuide* guides,
	int width, int height, int coarse_width, int coarse_height
) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= coarse_width * coarse_height) return;
	int x = pixel % coarse_width, y = pixel / coarse_width;
	int left = x * width / coarse_width, right = (x + 1) * width / coarse_width;
	int bottom = y * height / coarse_height, top = (y + 1) * height / coarse_height;
	SurfaceGuide center = guides[((bottom + top) / 2) * width + (left + right) / 2];
	if (center.object < 0 || center.mixed > 1) {
		coarse[pixel] = Vec();
		return;
	}
	Vec average;
	int count = 0;
	for (int iy = bottom; iy < top; ++iy)
		for (int ix = left; ix < right; ++ix) {
			int index = iy * width + ix;
			SurfaceGuide guide = guides[index];
			if (guide.object != center.object || guide.mixed != center.mixed ||
				center.normal.dot(guide.normal) < .98) continue;
			average = average + image[index];
			++count;
		}
	coarse[pixel] = count ? coarse[pixel] - average / count : Vec();
}

__global__ void control_apply_kernel(
	Vec* image, const Vec* coarse, const SurfaceGuide* guides,
	int width, int height, int coarse_width, int coarse_height
) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	SurfaceGuide center = guides[pixel];
	if (center.object < 0 || center.mixed > 1) return;
	int x = pixel % width, y = pixel / width, step = max(1, width / coarse_width);
	double cx = (x + .5) * coarse_width / width - .5;
	double cy = (y + .5) * coarse_height / height - .5, variation = 0;
	for (int axis = 0; axis < 4; ++axis) {
		int nx = x + (axis == 0 ? -step : axis == 1 ? step : 0);
		int ny = y + (axis == 2 ? -step : axis == 3 ? step : 0);
		if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
		SurfaceGuide neighbor = guides[ny * width + nx];
		if (neighbor.object == center.object && neighbor.mixed == center.mixed &&
			center.normal.dot(neighbor.normal) >= .98)
			variation = fmax(variation, (image[ny * width + nx] - image[pixel]).squared_length());
	}
	Vec correction;
	double total = 0;
	for (int dy = -14; dy <= 14; ++dy)
		for (int dx = -14; dx <= 14; ++dx) {
			int nx = int(floor(cx)) + dx, ny = int(floor(cy)) + dy;
			if (nx < 0 || nx >= coarse_width || ny < 0 || ny >= coarse_height) continue;
			int fine_x = int((nx + .5) * width / coarse_width);
			int fine_y = int((ny + .5) * height / coarse_height);
			SurfaceGuide neighbor = guides[fine_y * width + fine_x];
			if (neighbor.object != center.object || neighbor.mixed != center.mixed ||
				center.normal.dot(neighbor.normal) < .98 ||
				fabs((neighbor.position - center.position).dot(center.normal)) > .09) continue;
			double distance_x = nx - cx, distance_y = ny - cy;
			double difference = (image[fine_y * width + fine_x] - image[pixel]).squared_length();
			double weight = exp(-(distance_x * distance_x + distance_y * distance_y) /
				(variation > 1.5e-6 ? 8. : 220.) - difference * 10000);
			correction = correction + coarse[ny * coarse_width + nx] * weight;
			total += weight;
		}
	if (total > 0) image[pixel] = image[pixel] + correction / total;
}

__global__ void edge_path_kernel(
	const Vec* image, Vec* output, const SurfaceGuide* guides, int width, int height, int samples
) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	int x = pixel % width, y = pixel / width;
	SurfaceGuide center = guides[pixel];
	bool edge = false;
	for (int dy = -2; dy <= 2 && !edge; ++dy)
		for (int dx = -2; dx <= 2 && !edge; ++dx) {
			int nx = x + dx, ny = y + dy;
			if (nx < 0 || nx >= width || ny < 0 || ny >= height || (!dx && !dy)) continue;
			SurfaceGuide neighbor = guides[ny * width + nx];
			if (center.object < 0 && neighbor.object < 0) continue;
			edge = neighbor.object != center.object || neighbor.mixed != center.mixed ||
				(neighbor.albedo - center.albedo).squared_length() > .0025;
		}
	if (!edge) {
		output[pixel] = image[pixel];
		return;
	}
	unsigned int state = scramble(pixel + 1 + config.seed * 0x9e3779b9u);
	Vec color;
	int subpixels = config.antialias * config.antialias;
	for (int sy = 0; sy < config.antialias; ++sy)
		for (int sx = 0; sx < config.antialias; ++sx)
			for (int sample = 0; sample < samples; ++sample) {
				unsigned int index = sample * subpixels + sy * config.antialias + sx;
				RayData ray = camera_ray(x, y, sx, sy, width, height, state,
					low_discrepancy(index, pixel, 2), low_discrepancy(index, pixel, 3));
				double selector = (sample + random(state)) / samples;
				color = color + path_trace(ray, state, nullptr, selector,
					low_discrepancy(index, pixel, 0), low_discrepancy(index, pixel, 1));
			}
	output[pixel] = color / (double(subpixels) * samples);
}

__global__ void reconstruct_diffuse_kernel(
	const Vec* input, Vec* output, const SurfaceGuide* guides, int width, int height
) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	SurfaceGuide center = guides[pixel];
	if (center.object < 0 || center.mixed) {
		output[pixel] = input[pixel];
		return;
	}
	int x = pixel % width, y = pixel / width, radius = config.reconstruction_radius;
	Vec weighted;
	double total = 0, footprint = radius * radius * .5 + 1;
	for (int dy = -radius; dy <= radius; ++dy)
		for (int dx = -radius; dx <= radius; ++dx) {
			int nx = x + dx, ny = y + dy;
			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			int neighbor_index = ny * width + nx;
			SurfaceGuide neighbor = guides[neighbor_index];
			if (neighbor.object != center.object) continue;
			double albedo = (neighbor.albedo - center.albedo).squared_length();
			if (albedo > .01) continue;
			double alignment = center.normal.dot(neighbor.normal);
			if (alignment < .96) continue;
			Vec offset = neighbor.position - center.position;
			double plane = fabs(offset.dot(center.normal));
			if (plane > .12) continue;
			double weight = exp(-(dx * dx + dy * dy) / footprint - albedo * 40) *
				pow(fmax(0., alignment), 24) * exp(-plane * 20);
			Vec value = input[neighbor_index];
			Vec irradiance(
				value.x / fmax(.04, neighbor.albedo.x),
				value.y / fmax(.04, neighbor.albedo.y),
				value.z / fmax(.04, neighbor.albedo.z)
			);
			weighted = weighted + irradiance * weight;
			total += weight;
		}
	output[pixel] = total > 0 ? center.albedo.multiply(weighted / total) : input[pixel];
}

__global__ void reconstruct_mixed_kernel(
	const Vec* input, Vec* output, const SurfaceGuide* guides, int width, int height
) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	SurfaceGuide center = guides[pixel];
	if (center.object < 0 || !center.mixed ||
		(config.cache && center.mixed > 1 && center.directional < .2)) {
		output[pixel] = input[pixel];
		return;
	}
	Vec center_value = input[pixel], weighted;
	int x = pixel % width, y = pixel / width, radius = max(1, config.reconstruction_radius);
	double variation = 0;
	int probes = 0;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx) {
			int nx = x + dx, ny = y + dy;
			if (nx < 0 || nx >= width || ny < 0 || ny >= height || (!dx && !dy)) continue;
			int index = ny * width + nx;
			if (guides[index].object != center.object || guides[index].mixed != center.mixed) continue;
			variation += (input[index] - center_value).squared_length();
			++probes;
		}
	variation = probes ? variation / probes : 0;
	double detail = (center.mixed == 2 ? 120 : 45) /
		(1 + fmin(3., variation * (center.mixed == 2 ? 240 : 120)));
	double total = 0, footprint = radius * radius * .5 + 1;
	for (int dy = -radius; dy <= radius; ++dy)
		for (int dx = -radius; dx <= radius; ++dx) {
			int nx = x + dx, ny = y + dy;
			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			int index = ny * width + nx;
			SurfaceGuide neighbor = guides[index];
			if (neighbor.mixed != center.mixed || neighbor.object != center.object) continue;
			double albedo = (neighbor.albedo - center.albedo).squared_length();
			double normal = center.normal.dot(neighbor.normal);
			double plane = fabs((neighbor.position - center.position).dot(center.normal));
			if (albedo > .0025 || normal < .985 || plane > .05) continue;
			Vec sample = input[index];
			double difference = (sample - center_value).squared_length();
			double weight = exp(-(dx * dx + dy * dy) / footprint - albedo * 80 -
				difference * detail - plane * 30);
			weighted = weighted + sample * weight;
			total += weight;
		}
	output[pixel] = total > 0 ? weighted / total : center_value;
}

__global__ void radiance_cache_kernel(
	const Vec* input, Vec* output, const SurfaceGuide* guides, int width, int height, int stride
) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	SurfaceGuide center = guides[pixel];
	if (center.object < 0 || (config.cache && center.mixed > 1 && center.directional < .2)) {
		output[pixel] = input[pixel];
		return;
	}
	Vec value = input[pixel], weighted;
	bool factor = center.mixed <= 1;
	Vec reference = factor ? Vec(value.x / fmax(.04, center.albedo.x),
		value.y / fmax(.04, center.albedo.y), value.z / fmax(.04, center.albedo.z)) : value;
	double total = 0;
	int x = pixel % width, y = pixel / width;
	for (int dy = -1; dy <= 1; ++dy)
		for (int dx = -1; dx <= 1; ++dx) {
			int nx = x + dx * stride, ny = y + dy * stride;
			if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
			int index = ny * width + nx;
			SurfaceGuide neighbor = guides[index];
			if (neighbor.object != center.object || neighbor.mixed != center.mixed) continue;
			double alignment = center.normal.dot(neighbor.normal);
			double plane = fabs((neighbor.position - center.position).dot(center.normal));
			if (alignment < .98 || plane > .09) continue;
			Vec sample = input[index];
			if (config.hybrid_samples >= 128 && center.directional > .5 && luminance(sample) >
				1.2 * fmax(1e-4, luminance(value))) continue;
			double albedo = (neighbor.albedo - center.albedo).squared_length();
			if (albedo > (factor ? .16 : .0025)) continue;
			if (factor)
				sample = Vec(sample.x / fmax(.04, neighbor.albedo.x),
					sample.y / fmax(.04, neighbor.albedo.y), sample.z / fmax(.04, neighbor.albedo.z));
			double difference = (sample - reference).squared_length();
			double sensitivity = center.mixed == 2 ? 55 : config.shadow_samples > 0
				? (center.mixed ? 30 : 8) : (center.mixed ? 60 : 64);
			double weight = (dx ? 1. : 2.) * (dy ? 1. : 2.) *
				exp(-difference * sensitivity - plane * 20);
			weighted = weighted + sample * weight;
			total += weight;
		}
	output[pixel] = total > 0 ? (factor ? center.albedo.multiply(weighted / total) :
		weighted / total) : value;
}

__global__ void rare_specular_kernel(Vec* image, int width, int height, int samples) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	int x = pixel % width, y = pixel / width;
	unsigned int state = scramble(pixel + 1 + config.seed * 0x9e3779b9u), primary_state = state;
	Hit primary;
	if (!intersect(camera_ray(x, y, 0, 0, width, height, primary_state), true, primary)) return;
	ObjectData surface = objects[primary.object];
	if (surface.reflection != DIFF || surface.mixed_specular <= 0 ||
		(!config.cache && surface.mixed_specular * 16 * config.antialias * config.antialias > 1)) return;
	Vec reflected;
	for (int sy = 0; sy < config.antialias; ++sy)
		for (int sx = 0; sx < config.antialias; ++sx)
			for (int sample = 0; sample < samples; ++sample) {
				RayData ray = camera_ray(x, y, sx, sy, width, height, state);
				Hit hit;
				if (!intersect(ray, true, hit)) continue;
				ObjectData object = objects[hit.object];
				if (object.reflection != DIFF || object.mixed_specular <= 0 || object.mixed_specular >= 1) return;
				Feature material = feature(hit, state, false);
				if (material.reflection != DIFF) return;
				Vec normal = material.normal.dot(ray.direction) < 0 ? material.normal : -material.normal;
				RayData mirror{hit.position, reflect(ray.direction, normal)};
				reflected = reflected + path_trace(mirror, state).multiply(material.color) * object.mixed_specular;
			}
	image[pixel] = image[pixel] + reflected / (double(config.antialias * config.antialias) * samples);
}

__global__ void hybrid_specular_kernel(
	Vec* image, SurfaceGuide* guides, int width, int height, int samples
) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= width * height) return;
	int x = pixel % width, y = pixel / width;
	unsigned int state = scramble(pixel + 1 + config.seed * 0x9e3779b9u), primary_state = state;
	RayData primary = camera_ray(x, y, 0, 0, width, height, primary_state);
	Hit first;
	if (!intersect(primary, true, first)) return;
	Feature primary_material = feature(first, primary_state, false);
	bool stratified = primary_material.reflection == DIFF;
	if (primary_material.reflection == DIFF) {
		if (config.cache) return;
		ObjectData object = objects[first.object];
		if (object.mixed_specular * 16 * config.antialias * config.antialias <= 1) return;
	}
	Vec color, squared;
	double events = 0;
	int subpixels = config.antialias * config.antialias;
	int pilot = max(2, min(8, samples / 4));
	for (int sy = 0; sy < config.antialias; ++sy)
		for (int sx = 0; sx < config.antialias; ++sx)
			for (int sample = 0; sample < pilot; ++sample) {
				double directional;
				RayData ray = camera_ray(x, y, sx, sy, width, height, state);
				double selector = config.reuse && stratified
					? (sample + random(state)) / pilot : -1;
				Vec value = path_trace(ray, state, &directional, selector);
				events += directional > 0;
				color = color + value;
				squared = squared + value.multiply(value);
			}
	double count = double(subpixels) * pilot;
	Vec average = color / count;
	double variance = fmax(0., luminance(squared / count - average.multiply(average)));
	double scale = fmax(.02, luminance(average));
	double directional_fraction = events / count;
	double uncertainty = sqrt(variance / count) / scale;
	if (guides) guides[pixel].directional = directional_fraction;
	if (directional_fraction < .03 && uncertainty < .9) return;
	double score = directional_fraction + uncertainty * .2;
	int target = max(1, min(samples, int(ceil(samples * fmin(1., fmax(.3, score))))));
	int extra = max(0, target - (config.cache ? 0 : pilot));
	if (config.cache) color = Vec();
	for (int sy = 0; sy < config.antialias; ++sy)
		for (int sx = 0; sx < config.antialias; ++sx)
			for (int sample = 0; sample < extra; ++sample) {
				RayData ray = camera_ray(x, y, sx, sy, width, height, state);
				double selector = config.reuse && stratified
					? (sample + random(state)) / extra : -1;
				color = color + path_trace(ray, state, nullptr, selector);
			}
	image[pixel] = color / (double(subpixels) * (config.cache ? extra : pilot + extra));
}

__global__ void initialize_pixels(PixelState* pixels, int count, double radius) {
	int pixel = blockIdx.x * blockDim.x + threadIdx.x;
	if (pixel >= count) return;
	pixels[pixel].radius = radius;
	pixels[pixel].photons = 0;
	pixels[pixel].hits = 0;
	pixels[pixel].flux = Vec();
	pixels[pixel].accumulated = Vec();
	pixels[pixel].volume = Vec();
	pixels[pixel].specular = Vec();
}

__device__ unsigned int cell_hash(int x, int y, int z, unsigned int mask) {
	unsigned int a = unsigned(x) * 73856093u;
	unsigned int b = unsigned(y) * 19349663u;
	unsigned int c = unsigned(z) * 83492791u;
	return (a ^ b ^ c) & mask;
}

__global__ void visible_points_kernel(
	VisiblePoint* points, int* buckets, unsigned int mask,
	PixelState* pixels, int width, int height, double cell_size, int iteration
) {
	int index = blockIdx.x * blockDim.x + threadIdx.x;
	int subpixels = config.antialias * config.antialias;
	if (index >= width * height * subpixels) return;
	int pixel = index / subpixels, subpixel = index % subpixels;
	int x = pixel % width, y = pixel / width;
	unsigned int state = scramble(index + 1 + iteration * 0x9e3779b9u + config.seed * 0x85ebca6bu);
	RayData ray = camera_ray(
		x, y, subpixel % config.antialias, subpixel / config.antialias, width, height, state);
	Vec throughput(1, 1, 1);
	int wavelength = -1;
	points[index].pixel = -1;
	for (int depth = 0; depth < kMaxBounces; ++depth) {
		Hit hit;
		if (!intersect(ray, true, hit)) break;
		ObjectData object = objects[hit.object];
		if (object.emission.maximum() > 0) {
			Vec contribution = throughput.multiply(object.emission);
			atomicAdd(&pixels[pixel].specular.x, contribution.x);
			atomicAdd(&pixels[pixel].specular.y, contribution.y);
			atomicAdd(&pixels[pixel].specular.z, contribution.z);
			break;
		}
		if (config.medium_density > 0)
			throughput = throughput * exp(-config.medium_density * hit.distance);
		if (config.volume_density > 0 && depth < 2) {
			double transmittance;
			Vec volume = throughput.multiply(march_volume(ray, hit.distance, state, transmittance));
			atomicAdd(&pixels[pixel].volume.x, volume.x);
			atomicAdd(&pixels[pixel].volume.y, volume.y);
			atomicAdd(&pixels[pixel].volume.z, volume.z);
			throughput = throughput * transmittance;
		}
		Feature material = feature(hit, state);
		if (depth == 0 && config.hybrid_samples > 0 && object.reflection == DIFF &&
			object.mixed_specular > 0 && (config.cache || object.mixed_specular *
				16 * config.antialias * config.antialias <= 1)) {
			material = feature(hit, state, false);
			if (material.reflection == DIFF)
				throughput = throughput * (1 - object.mixed_specular);
		}
		if (material.reflection == DIFF) {
			if (config.guided && config.shadow_samples > 0) {
				unsigned int sequence = config.cache ? (iteration - 1) * subpixels + subpixel : UINT_MAX;
				Vec light = throughput.multiply(direct_lighting(hit, material, -ray.direction, state, sequence, pixel));
				atomicAdd(&pixels[pixel].volume.x, light.x);
				atomicAdd(&pixels[pixel].volume.y, light.y);
				atomicAdd(&pixels[pixel].volume.z, light.z);
			}
			Vec normal = material.normal.dot(ray.direction) < 0 ? material.normal : -material.normal;
			VisiblePoint point;
			point.position = hit.position;
			point.normal = normal;
			point.throughput = throughput.multiply(material.color);
			point.pixel = pixel;
			point.cell_x = int(floor(hit.position.x / cell_size));
			point.cell_y = int(floor(hit.position.y / cell_size));
			point.cell_z = int(floor(hit.position.z / cell_size));
			if (config.reuse && object.shape == kCube) {
				if (fabs(hit.normal.x) > .9)
					point.cell_x = int(floor((hit.normal.x < 0 ? object.a.x : object.b.x) / cell_size));
				else if (fabs(hit.normal.y) > .9)
					point.cell_y = int(floor((hit.normal.y < 0 ? object.a.y : object.b.y) / cell_size));
				else
					point.cell_z = int(floor((hit.normal.z < 0 ? object.a.z : object.b.z) / cell_size));
			}
			point.object = hit.object;
			unsigned int bucket = cell_hash(point.cell_x, point.cell_y, point.cell_z, mask);
			point.next = atomicExch(&buckets[bucket], index);
			points[index] = point;
			break;
		}
		if (!scatter(ray, hit, material, throughput, state, depth, wavelength)) break;
	}
}

__device__ void collect(
	Vec position, Vec normal, Vec power, int object, const VisiblePoint* points, const int* buckets,
	unsigned int mask, PixelState* pixels, double cell_size
) {
	int x = int(floor(position.x / cell_size));
	int y = int(floor(position.y / cell_size));
	int z = int(floor(position.z / cell_size));
	ObjectData surface = objects[object];
	Vec geometric = config.reuse && surface.shape == kCube ? object_normal(surface, position) : normal;
	int flat = config.reuse && surface.shape == kCube
		? (fabs(geometric.x) > .9 ? 0 : fabs(geometric.y) > .9 ? 1 : 2) : -1;
	if (flat == 0) x = int(floor((geometric.x < 0 ? surface.a.x : surface.b.x) / cell_size));
	if (flat == 1) y = int(floor((geometric.y < 0 ? surface.a.y : surface.b.y) / cell_size));
	if (flat == 2) z = int(floor((geometric.z < 0 ? surface.a.z : surface.b.z) / cell_size));
	for (int dx = flat == 0 ? 0 : -1; dx <= (flat == 0 ? 0 : 1); ++dx)
		for (int dy = flat == 1 ? 0 : -1; dy <= (flat == 1 ? 0 : 1); ++dy)
			for (int dz = flat == 2 ? 0 : -1; dz <= (flat == 2 ? 0 : 1); ++dz) {
				int cx = x + dx, cy = y + dy, cz = z + dz;
				int index = buckets[cell_hash(cx, cy, cz, mask)];
				while (index >= 0) {
					VisiblePoint point = points[index];
					if (point.object == object && point.cell_x == cx && point.cell_y == cy && point.cell_z == cz) {
						PixelState* pixel = &pixels[point.pixel];
						if ((point.position - position).squared_length() <= pixel->radius * pixel->radius &&
							point.normal.dot(normal) >= 0) {
							Vec contribution = power.multiply(point.throughput);
							atomicAdd(&pixel->flux.x, contribution.x);
							atomicAdd(&pixel->flux.y, contribution.y);
							atomicAdd(&pixel->flux.z, contribution.z);
							atomicAdd(&pixel->hits, 1u);
						}
					}
					index = point.next;
				}
			}
}

__global__ void photons_kernel(
	const VisiblePoint* points, const int* buckets, unsigned int mask,
	PixelState* pixels, double cell_size, int photons, int iteration
) {
	int index = blockIdx.x * blockDim.x + threadIdx.x;
	if (index >= photons) return;
	unsigned int state = scramble(index + 1 + iteration * 0x85ebca6bu + config.seed * 0x9e3779b9u);
	double emitter_sample = -1;
	if (config.reuse) {
		double shift = (scramble(iteration * 0x9e3779b9u + config.seed) + .5) * 2.3283064365386963e-10;
		emitter_sample = (index + random(state)) / photons + shift;
		if (emitter_sample >= 1) emitter_sample -= 1;
	}
	EmitterData emitter = choose_emitter(state, false, emitter_sample);
	Vec emitter_normal;
	Vec origin, direction;
	if (config.guided && emitter.shape != kEmitterBox) {
		double first = low_discrepancy(index, iteration, 0);
		double second = low_discrepancy(index, iteration, 1);
		if (emitter.shape == kEmitterSphere) {
			double height = 1 - 2 * first, angle = 2 * kPi * second;
			double radius = sqrt(fmax(0., 1 - height * height));
			emitter_normal = Vec(radius * cos(angle), height, radius * sin(angle));
			origin = emitter.position + emitter_normal * emitter.radius;
		} else {
			emitter_normal = emitter.normal;
			Vec tangent = (fabs(emitter_normal.x) > .1 ? Vec(0, 1) : Vec(1)).cross(emitter_normal).normalized();
			double radius = emitter.radius * sqrt(first), angle = 2 * kPi * second;
			origin = emitter.position + tangent * (radius * cos(angle)) +
				emitter_normal.cross(tangent) * (radius * sin(angle));
		}
		double angle = 2 * kPi * low_discrepancy(index, iteration, 2);
		double radius = low_discrepancy(index, iteration, 3);
		Vec tangent = (fabs(emitter_normal.x) > .1 ? Vec(0, 1) : Vec(1)).cross(emitter_normal).normalized();
		direction = (tangent * (cos(angle) * sqrt(radius)) +
			emitter_normal.cross(tangent) * (sin(angle) * sqrt(radius)) +
			emitter_normal * sqrt(1 - radius)).normalized();
	} else {
		origin = sample_light(emitter, emitter_normal, state);
		direction = sample_diffuse(emitter_normal, state);
	}
	RayData ray{origin, direction};
	Vec power = emitter.emission * (emitter.area / emitter.probability);
	int wavelength = -1;
	for (int depth = 0; depth < kMaxBounces; ++depth) {
		Hit hit;
		if (!intersect(ray, false, hit)) break;
		if (config.medium_density > 0) {
			double distance = -log(fmax(1e-12, 1 - random(state))) / config.medium_density;
			if (distance < hit.distance) {
				power = power * config.medium_albedo;
				ray = RayData{ray.at(distance), sample_phase(ray.direction, state)};
				continue;
			}
		}
		Feature material = feature(hit, state);
		Vec normal = material.normal.dot(ray.direction) < 0 ? material.normal : -material.normal;
		if (material.reflection == DIFF && !(config.guided && config.shadow_samples > 0 &&
			depth == 0 && emitter.direct_probability > 0))
			collect(hit.position, normal, power, hit.object, points, buckets, mask, pixels, cell_size);
		if (!scatter(ray, hit, material, power, state, depth, wavelength, true)) break;
	}
}

__global__ void update_pixels(
	PixelState* pixels, Vec* image, int count, int photons, int iteration, double alpha
) {
	int index = blockIdx.x * blockDim.x + threadIdx.x;
	if (index >= count) return;
	PixelState pixel = pixels[index];
	if (pixel.hits > 0) {
		double ratio = (pixel.photons + alpha * pixel.hits) / (pixel.photons + pixel.hits);
		pixel.radius *= sqrt(ratio);
		pixel.accumulated = (pixel.accumulated + pixel.flux) * ratio;
		pixel.photons += alpha * pixel.hits;
	}
	int subpixels = config.antialias * config.antialias;
	image[index] = pixel.accumulated / (subpixels * kPi * pixel.radius * pixel.radius * photons * iteration) +
		(pixel.volume + pixel.specular) / (double(subpixels) * iteration);
	pixel.flux = Vec();
	pixel.hits = 0;
	pixels[index] = pixel;
}

Vec convert(P3 value) { return Vec(value.x, value.y, value.z); }

bool has_exposed_emitter() {
	for (int index = 0; index < scene_num; ++index) {
		if (scene[index]->texture.emission.max() <= 0) continue;
		SphereObject* light = dynamic_cast<SphereObject*>(scene[index]);
		if (!light) return true;
		bool enclosed = false;
		for (int other = 0; light && other < scene_num; ++other) {
			SphereObject* shell = dynamic_cast<SphereObject*>(scene[other]);
			enclosed |= shell && shell != light && shell->texture.refl == REFR &&
				(shell->o - light->o).len() + light->r < shell->r + kEpsilon;
		}
		if (!enclosed) return true;
	}
	return false;
}
std::vector<unsigned char*> upload_scene(const RenderConfig& settings) {
	std::vector<ObjectData> flat;
	std::vector<TextureData> images;
	std::vector<EmitterData> lights;
	std::vector<unsigned char*> allocations;
	std::map<std::string, int> texture_ids;
	auto upload_texture = [&](const std::string& filename, Texture* original = nullptr) {
		if (filename.empty()) return -1;
		auto found = texture_ids.find(filename);
		if (found != texture_ids.end()) return found->second;
		int width = 0, height = 0, channels = 0;
		unsigned char* source = original ? original->buf : stbi_load(filename.c_str(), &width, &height, &channels, 0);
		if (original) width = original->w, height = original->h, channels = original->c;
		if (!source || channels < 3) {
			fprintf(stderr, "Failed to load RGB/RGBA texture: %s\n", filename.c_str());
			exit(1);
		}
		unsigned char* pixels;
		size_t bytes = size_t(width) * height * channels;
		CUDA_CHECK(cudaMalloc(&pixels, bytes));
		CUDA_CHECK(cudaMemcpy(pixels, source, bytes, cudaMemcpyHostToDevice));
		if (!original) stbi_image_free(source);
		allocations.push_back(pixels);
		int index = images.size();
		texture_ids[filename] = index;
		images.push_back(TextureData{pixels, width, height, channels});
		return index;
	};

	for (int index = 0; index < scene_num; ++index) {
		Object* source = scene[index];
		ObjectData object{};
		if (SphereObject* sphere = dynamic_cast<SphereObject*>(source)) {
			object.shape = kSphere;
			object.a = convert(sphere->o);
			object.radius = sphere->r;
		} else if (CubeObject* cube = dynamic_cast<CubeObject*>(source)) {
			object.shape = kCube;
			object.a = convert(cube->m0);
			object.b = convert(cube->m1);
		} else if (PlaneObject* plane = dynamic_cast<PlaneObject*>(source)) {
			object.shape = kPlane;
			object.a = convert(plane->n);
		} else if (BezierObject* vase = dynamic_cast<BezierObject*>(source)) {
			object.shape = kBezier;
			object.a = convert(vase->pos);
			BezierData curve{};
			for (int i = 0; i < 9; ++i) {
				curve.x[i] = vase->curve.dx[i];
				curve.y[i] = vase->curve.dy[i];
			}
			curve.segments = vase->curve.num;
			for (int i = 0; i <= curve.segments; ++i) {
				curve.lower[i] = vase->curve.data[i].t0;
				curve.upper[i] = vase->curve.data[i].t1;
			}
			curve.radius = vase->curve.max;
			curve.height = vase->curve.height;
			CUDA_CHECK(cudaMemcpyToSymbol(bezier_data, &curve, sizeof curve));
		} else {
			fprintf(stderr, "Unsupported scene object %d\n", index);
			exit(1);
		}
		MaterialProperties material = source->texture.material;
		if (material.mapping == UV_AUTO)
			material.mapping = object.shape == kBezier ? UV_BEZIER : UV_PLANAR;
		if (material.specular < 0 || material.specular > 1) {
			fprintf(stderr, "Invalid material probabilities for scene object %d\n", index);
			exit(1);
		}
		object.color = convert(source->texture.color);
		object.emission = convert(source->texture.emission);
		object.ior = source->texture.brdf;
		object.reflection = source->texture.refl;
		object.roughness = material.roughness >= 0 ? material.roughness : settings.roughness;
		object.metallic = material.metallic >= 0 ? fmax(settings.metallic, material.metallic) : settings.metallic;
		object.bump = material.bump;
		object.mixed_specular = settings.enable_pbr && object.reflection == DIFF ? 0 : material.specular;
		object.uv_u = convert(material.axis_u);
		object.uv_v = convert(material.axis_v);
		object.uv_offset = convert(material.offset);
		object.mapping = material.mapping;
		object.texture = upload_texture(source->texture.filename, &source->texture);
		object.specular_mask = upload_texture(material.specular_mask);
		object.diffuse_mask = upload_texture(material.diffuse_mask);
		if (object.emission.maximum() > 0) {
			if (object.shape != kSphere && object.shape != kCube) {
				fputs("Only spherical or box-shaped emissive scene objects are supported\n", stderr);
				exit(1);
			}
			if (object.shape == kCube) {
				Vec extent = object.b - object.a;
				double area = 2 * (extent.x * extent.y + extent.x * extent.z + extent.y * extent.z);
				lights.push_back(EmitterData{
					object.a, Vec(), object.emission, extent, 0, area, 0, 0, index, kEmitterBox
				});
			} else {
				bool ceiling = object.radius > 100;
				double radius = ceiling ? settings.light_radius : object.radius;
				Vec position = ceiling ? object.a + Vec(0, -object.radius, 0) : object.a;
				lights.push_back(EmitterData{
					position, Vec(0, -1), object.emission, Vec(), radius,
					(ceiling ? 1 : 4) * kPi * radius * radius, 0, 0, index,
					ceiling ? kEmitterDisk : kEmitterSphere
				});
			}
		}
		flat.push_back(object);
	}
	if (flat.size() > kMaxObjects || images.size() > kMaxTextures) {
		fputs("Scene exceeds CUDA constant-memory limits\n", stderr);
		exit(1);
	}
	int count = flat.size();
	ObjectData* device_objects;
	CUDA_CHECK(cudaMalloc(&device_objects, flat.size() * sizeof(ObjectData)));
	CUDA_CHECK(cudaMemcpy(device_objects, flat.data(), flat.size() * sizeof(ObjectData), cudaMemcpyHostToDevice));
	CUDA_CHECK(cudaMemcpyToSymbol(objects, &device_objects, sizeof device_objects));
	allocations.push_back(reinterpret_cast<unsigned char*>(device_objects));
	CUDA_CHECK(cudaMemcpyToSymbol(textures, images.data(), images.size() * sizeof(TextureData)));
	CUDA_CHECK(cudaMemcpyToSymbol(object_count, &count, sizeof count));
	if (lights.empty()) {
		fputs("Scene contains no emissive objects\n", stderr);
		exit(1);
	}
	double weight = 0, direct_weight = 0;
	for (EmitterData& light: lights) {
		double power = light.area * (light.emission.x * .2126 + light.emission.y * .7152 + light.emission.z * .0722);
		bool enclosed = false;
		if (light.shape == kEmitterSphere)
			for (const ObjectData& object: flat)
				enclosed |= object.shape == kSphere && object.reflection == REFR &&
					sqrt((object.a - light.position).squared_length()) + light.radius < object.radius + kEpsilon;
		weight += power;
		light.probability = power;
		light.direct_probability = enclosed ? 0 : power;
		direct_weight += light.direct_probability;
	}
	for (EmitterData& light: lights) {
		light.probability /= weight;
		light.direct_probability = direct_weight > 0 ? light.direct_probability / direct_weight : 0;
	}
	count = lights.size();
	EmitterData* device_emitters;
	CUDA_CHECK(cudaMalloc(&device_emitters, lights.size() * sizeof(EmitterData)));
	CUDA_CHECK(cudaMemcpy(device_emitters, lights.data(), lights.size() * sizeof(EmitterData), cudaMemcpyHostToDevice));
	CUDA_CHECK(cudaMemcpyToSymbol(emitters, &device_emitters, sizeof device_emitters));
	allocations.push_back(reinterpret_cast<unsigned char*>(device_emitters));
	CUDA_CHECK(cudaMemcpyToSymbol(emitter_count, &count, sizeof count));
	fprintf(stderr, "Scene: %s, %d objects, %d emissive objects\n", scene_name, scene_num, count);
	return allocations;
}

void write_image(const char* filename, const std::vector<Vec>& image, int width, int height) {
	FILE* file = fopen(filename, "wb");
	if (!file) {
		perror(filename);
		exit(1);
	}
	fprintf(file, "P6\n%d %d\n255\n", width, height);
	for (int y = height - 1; y >= 0; --y)
		for (int x = width - 1; x >= 0; --x) {
			Vec color = image[y * width + x];
			fputc(gamma_trans(color.x), file);
			fputc(gamma_trans(color.y), file);
			fputc(gamma_trans(color.z), file);
		}
	fclose(file);
}

void cinematic(RenderConfig& settings) {
	settings.aperture = .45;
	settings.focus_distance = 205;
	settings.roughness = .34;
	settings.metallic = .08;
	settings.bump_strength = 1.4;
	settings.medium_density = .0022;
	settings.medium_albedo = .72;
	settings.anisotropy = .55;
	settings.dispersion = .035;
	settings.shadow_samples = 2;
	settings.enable_pbr = 1;
	settings.texture_filter = 1;
}

bool parse_settings(int argc, char** argv, int start, RenderConfig& settings) {
	for (int index = start; index < argc; ++index) {
		std::string option = argv[index];
		if (option == "--cinematic") {
			cinematic(settings);
			continue;
		}
		if (option == "--pbr") {
			settings.enable_pbr = 1;
			continue;
		}
		if (option == "--nearest") {
			settings.texture_filter = 0;
			continue;
		}
		if (option == "--reuse") {
			settings.reuse = 1;
			continue;
		}
		if (option == "--guided") {
			settings.reuse = settings.guided = 1;
			continue;
		}
		if (option == "--cache") {
			settings.reuse = settings.guided = settings.cache = 1;
			continue;
		}
		if (index + 1 >= argc) return false;
		const char* value = argv[++index];
		if (option == "--scene") {
			if (!select_scene(value)) {
				fprintf(stderr, "Unknown scene: %s (expected vase or balls)\n", value);
				return false;
			}
			settings.camera_x = scene_camera.origin.x;
			settings.camera_y = scene_camera.origin.y;
			settings.camera_z = scene_camera.origin.z;
			settings.direction_x = scene_camera.direction.x;
			settings.direction_y = scene_camera.direction.y;
			settings.direction_z = scene_camera.direction.z;
			settings.camera_offset = scene_camera.offset;
			settings.camera_scale = scene_camera.scale;
		}
		else if (option == "--aperture") settings.aperture = atof(value);
		else if (option == "--focus") settings.focus_distance = atof(value);
		else if (option == "--light-radius") settings.light_radius = atof(value);
		else if (option == "--camera-origin") {
			if (sscanf(value, "%lf,%lf,%lf", &settings.camera_x, &settings.camera_y, &settings.camera_z) != 3)
				return false;
		}
		else if (option == "--camera-direction") {
			if (sscanf(value, "%lf,%lf,%lf", &settings.direction_x, &settings.direction_y, &settings.direction_z) != 3)
				return false;
		}
		else if (option == "--camera-offset") settings.camera_offset = atof(value);
		else if (option == "--camera-scale") settings.camera_scale = atof(value);
		else if (option == "--roughness") settings.roughness = atof(value), settings.enable_pbr = 1;
		else if (option == "--metallic") settings.metallic = atof(value), settings.enable_pbr = 1;
		else if (option == "--bump") settings.bump_strength = atof(value);
		else if (option == "--medium-density") settings.medium_density = atof(value);
		else if (option == "--medium-albedo") settings.medium_albedo = atof(value);
		else if (option == "--anisotropy") settings.anisotropy = atof(value);
		else if (option == "--volume-density") settings.volume_density = atof(value);
		else if (option == "--volume-step") settings.volume_step = atof(value);
		else if (option == "--volume-emission") settings.volume_emission = atof(value);
		else if (option == "--volume-radius") settings.volume_radius = atof(value);
		else if (option == "--volume-center") {
			if (sscanf(value, "%lf,%lf,%lf", &settings.volume_x, &settings.volume_y, &settings.volume_z) != 3)
				return false;
		}
		else if (option == "--dispersion") settings.dispersion = atof(value);
		else if (option == "--aa") settings.antialias = atoi(value);
		else if (option == "--shadow-samples") settings.shadow_samples = atoi(value);
		else if (option == "--hybrid-samples") settings.hybrid_samples = atoi(value);
		else if (option == "--reconstruction-radius") settings.reconstruction_radius = atoi(value);
		else if (option == "--seed") {
			char* end;
			unsigned long parsed = strtoul(value, &end, 10);
			if (*value == '-' || *end || parsed > UINT_MAX) return false;
			settings.seed = unsigned(parsed);
		}
		else return false;
	}
	return settings.aperture >= 0 && settings.focus_distance > 0 && settings.light_radius > 0 &&
		settings.camera_offset > 0 && settings.camera_scale > 0 &&
		(settings.direction_x * settings.direction_x + settings.direction_y * settings.direction_y +
			settings.direction_z * settings.direction_z > 0) &&
		settings.roughness > 0 && settings.roughness <= 1 && settings.metallic >= 0 && settings.metallic <= 1 &&
		settings.bump_strength >= 0 && settings.medium_density >= 0 && settings.medium_albedo >= 0 &&
		settings.medium_albedo <= 1 && fabs(settings.anisotropy) < 1 && settings.volume_density >= 0 &&
		settings.volume_step > 0 && settings.volume_radius > 0 && settings.dispersion >= 0 &&
		settings.antialias >= 1 && settings.antialias <= 4 && settings.shadow_samples >= 0 &&
		settings.hybrid_samples >= 0 && settings.reconstruction_radius >= 0 && settings.reconstruction_radius <= 8;
}

int run(int argc, char** argv) {
	int positional = 1;
	while (positional < argc && strncmp(argv[positional], "--", 2) != 0) ++positional;
	if (positional != 5 && positional != 8) {
		fprintf(stderr, "Usage: %s WIDTH HEIGHT OUTPUT SAMPLES [PHOTONS_PER_PIXEL RADIUS ALPHA] [OPTIONS]\n", argv[0]);
		fputs("Options: --scene vase|balls --cinematic --pbr --aperture F --focus F --light-radius F\n", stderr);
		fputs("         --shadow-samples N\n", stderr);
		fputs("         --camera-origin X,Y,Z --camera-direction X,Y,Z --camera-offset F --camera-scale F\n", stderr);
		fputs("         --aa N --roughness F --metallic F --bump F --dispersion F --nearest\n", stderr);
		fputs("         --hybrid-samples N --reconstruction-radius N --reuse --guided --cache --seed N\n", stderr);
		fputs("         --medium-density F --medium-albedo F --anisotropy F\n", stderr);
		fputs("         --volume-density F --volume-step F --volume-emission F --volume-radius F\n", stderr);
		fputs("         --volume-center X,Y,Z\n", stderr);
		return 1;
	}
	int width = atoi(argv[1]), height = atoi(argv[2]), passes = atoi(argv[4]);
	if (width <= 0 || height <= 0 || passes <= 0 ||
		(positional == 8 && (atof(argv[5]) <= 0 || atof(argv[6]) <= 0 || atof(argv[7]) <= 0 || atof(argv[7]) > 1))) {
		fputs("Invalid rendering parameters\n", stderr);
		return 1;
	}
	RenderConfig settings{};
	settings.focus_distance = 205;
	settings.light_radius = 18;
	settings.roughness = .35;
	settings.medium_albedo = .8;
	settings.anisotropy = .35;
	settings.volume_step = 1;
	settings.volume_radius = 13;
	settings.volume_x = 38;
	settings.volume_y = 27;
	settings.volume_z = 70;
	settings.camera_x = 150;
	settings.camera_y = 28;
	settings.camera_z = 260;
	settings.direction_x = -.45;
	settings.direction_y = .001;
	settings.direction_z = -1;
	settings.camera_offset = 150;
	settings.camera_scale = 1.05;
	settings.antialias = 2;
	settings.texture_filter = 1;
	if (!parse_settings(argc, argv, positional, settings)) {
		fputs("Invalid rendering effect options\n", stderr);
		return 1;
	}
	if (settings.guided && has_exposed_emitter()) settings.shadow_samples = std::max(1, settings.shadow_samples);

	cudaDeviceProp device;
	CUDA_CHECK(cudaGetDeviceProperties(&device, 0));
	fprintf(stderr, "GPU: %s, compute capability %d.%d\n", device.name, device.major, device.minor);
	fprintf(stderr, "Effects: AA=%dx%d aperture=%.2f focus=%.1f light=%.1f shadows=%d pbr=%d "
		"bump=%.2f medium=%.4f volume=%.4f dispersion=%.3f\n", settings.antialias, settings.antialias,
		settings.aperture, settings.focus_distance, settings.light_radius, settings.shadow_samples,
		settings.enable_pbr, settings.bump_strength, settings.medium_density, settings.volume_density,
		settings.dispersion);
	CUDA_CHECK(cudaMemcpyToSymbol(config, &settings, sizeof settings));
	std::vector<unsigned char*> textures = upload_scene(settings);
	int count = width * height;
	Vec* image;
	CUDA_CHECK(cudaMalloc(&image, size_t(count) * sizeof(Vec)));
	const int threads = 128;
	cudaEvent_t start, stop;
	CUDA_CHECK(cudaEventCreate(&start));
	CUDA_CHECK(cudaEventCreate(&stop));
	CUDA_CHECK(cudaEventRecord(start));

	if (positional == 5) {
		path_trace_kernel<<<(count + threads - 1) / threads, threads>>>(image, width, height, passes);
		CUDA_CHECK(cudaGetLastError());
	} else {
		double radius = atof(argv[6]), alpha = atof(argv[7]);
		double density = atof(argv[5]);
		if (settings.cache) {
			double scale = fmin(1., sqrt(640. * 360 / count));
			radius *= fmax(.24, scale);
			density *= scale;
		}
		int photons = std::max(1, int(ceil(density * count)));
		size_t visible_count = size_t(count) * settings.antialias * settings.antialias;
		if (visible_count > size_t(UINT_MAX) / 2) {
			fputs("Image has too many visible points for the CUDA spatial hash\n", stderr);
			return 1;
		}
		unsigned int bucket_count = 1;
		while (bucket_count < visible_count * 2) bucket_count <<= 1;

		VisiblePoint* visible;
		PixelState* pixels;
		int* buckets;
		CUDA_CHECK(cudaMalloc(&visible, size_t(visible_count) * sizeof(VisiblePoint)));
		CUDA_CHECK(cudaMalloc(&pixels, size_t(count) * sizeof(PixelState)));
		CUDA_CHECK(cudaMalloc(&buckets, size_t(bucket_count) * sizeof(int)));
		initialize_pixels<<<(count + threads - 1) / threads, threads>>>(pixels, count, radius);
		CUDA_CHECK(cudaGetLastError());
		for (int iteration = 1; iteration <= passes; ++iteration) {
			CUDA_CHECK(cudaMemset(buckets, 0xff, size_t(bucket_count) * sizeof(int)));
			visible_points_kernel<<<(visible_count + threads - 1) / threads, threads>>>(
				visible, buckets, bucket_count - 1, pixels, width, height, radius, iteration);
			CUDA_CHECK(cudaGetLastError());
			photons_kernel<<<(photons + threads - 1) / threads, threads>>>(
				visible, buckets, bucket_count - 1, pixels, radius, photons, iteration);
			CUDA_CHECK(cudaGetLastError());
			update_pixels<<<(count + threads - 1) / threads, threads>>>(
				pixels, image, count, photons, iteration, alpha);
			CUDA_CHECK(cudaGetLastError());
			fprintf(stderr, "\rSPPM iteration %d/%d, photons per iteration: %d", iteration, passes, photons);
		}
		fputc('\n', stderr);
		SurfaceGuide* guides = nullptr;
		if (settings.reconstruction_radius > 0) {
			Vec* reconstructed;
			CUDA_CHECK(cudaMalloc(&guides, size_t(count) * sizeof(SurfaceGuide)));
			CUDA_CHECK(cudaMalloc(&reconstructed, size_t(count) * sizeof(Vec)));
			surface_guides_kernel<<<(count + threads - 1) / threads, threads>>>(guides, width, height);
			CUDA_CHECK(cudaGetLastError());
			reconstruct_diffuse_kernel<<<(count + threads - 1) / threads, threads>>>(
				image, reconstructed, guides, width, height);
			CUDA_CHECK(cudaGetLastError());
			if (settings.reuse) {
				reconstruct_diffuse_kernel<<<(count + threads - 1) / threads, threads>>>(
					reconstructed, image, guides, width, height);
				CUDA_CHECK(cudaGetLastError());
				CUDA_CHECK(cudaFree(reconstructed));
			} else {
				CUDA_CHECK(cudaFree(image));
				image = reconstructed;
			}
		}
		bool detailed = settings.cache && settings.hybrid_samples >= 128;
		int footprint = std::max(1, settings.reconstruction_radius *
			std::max(1, width / (detailed ? 320 : 160)));
		auto reconstruct_cache = [&](int radius) {
			if (!settings.cache || !guides) return;
			Vec* cached;
			CUDA_CHECK(cudaMalloc(&cached, size_t(count) * sizeof(Vec)));
			for (int stride = 1; stride <= radius; stride <<= 1) {
				radiance_cache_kernel<<<(count + threads - 1) / threads, threads>>>(
					image, cached, guides, width, height, stride);
				CUDA_CHECK(cudaGetLastError());
				std::swap(image, cached);
			}
			CUDA_CHECK(cudaFree(cached));
		};
		if (detailed) reconstruct_cache(footprint);
		if (settings.hybrid_samples > 0) {
			RenderConfig hybrid = settings;
			if (settings.shadow_samples > 0 || has_exposed_emitter())
				hybrid.shadow_samples = std::max(1, settings.shadow_samples);
			CUDA_CHECK(cudaMemcpyToSymbol(config, &hybrid, sizeof hybrid));
			rare_specular_kernel<<<(count + threads - 1) / threads, threads>>>(
				image, width, height, std::max(16, std::min(128, settings.hybrid_samples / 24)));
			CUDA_CHECK(cudaGetLastError());
			hybrid_specular_kernel<<<(count + threads - 1) / threads, threads>>>(
				image, guides, width, height, settings.hybrid_samples);
			CUDA_CHECK(cudaGetLastError());
			if (guides) {
				Vec* reconstructed;
				CUDA_CHECK(cudaMalloc(&reconstructed, size_t(count) * sizeof(Vec)));
				reconstruct_mixed_kernel<<<(count + threads - 1) / threads, threads>>>(
					image, reconstructed, guides, width, height);
				CUDA_CHECK(cudaGetLastError());
				if (settings.reuse) {
					reconstruct_mixed_kernel<<<(count + threads - 1) / threads, threads>>>(
						reconstructed, image, guides, width, height);
					CUDA_CHECK(cudaGetLastError());
					CUDA_CHECK(cudaFree(reconstructed));
				} else {
					CUDA_CHECK(cudaFree(image));
					image = reconstructed;
				}
			}
		}
		reconstruct_cache(detailed ? 2 : footprint);
		if (detailed && guides && width >= 1280) {
			int coarse_width = width, coarse_height = height;
			int coarse_count = coarse_width * coarse_height;
			Vec* coarse;
			CUDA_CHECK(cudaMalloc(&coarse, size_t(coarse_count) * sizeof(Vec)));
			RenderConfig unbiased = settings;
			unbiased.cache = 0;
			CUDA_CHECK(cudaMemcpyToSymbol(config, &unbiased, sizeof unbiased));
			control_path_kernel<<<(coarse_count + threads - 1) / threads, threads>>>(
				coarse, coarse_width, coarse_height,
				std::max(192, std::min(settings.hybrid_samples >= 768 ? 320 : 256,
					settings.hybrid_samples / 2)));
			CUDA_CHECK(cudaGetLastError());
			CUDA_CHECK(cudaMemcpyToSymbol(config, &settings, sizeof settings));
			control_residual_kernel<<<(coarse_count + threads - 1) / threads, threads>>>(
				coarse, image, guides, width, height, coarse_width, coarse_height);
			CUDA_CHECK(cudaGetLastError());
			control_apply_kernel<<<(count + threads - 1) / threads, threads>>>(
				image, coarse, guides, width, height, coarse_width, coarse_height);
			CUDA_CHECK(cudaGetLastError());
			Vec* corrected;
			CUDA_CHECK(cudaMalloc(&corrected, size_t(count) * sizeof(Vec)));
			CUDA_CHECK(cudaMemcpyToSymbol(config, &unbiased, sizeof unbiased));
			edge_path_kernel<<<(count + threads - 1) / threads, threads>>>(
				image, corrected, guides, width, height,
				std::max(128, std::min(256, settings.hybrid_samples / 2)));
			CUDA_CHECK(cudaGetLastError());
			CUDA_CHECK(cudaMemcpyToSymbol(config, &settings, sizeof settings));
			CUDA_CHECK(cudaFree(image));
			image = corrected;
			CUDA_CHECK(cudaFree(coarse));
		}
		if (guides) CUDA_CHECK(cudaFree(guides));
		CUDA_CHECK(cudaFree(visible));
		CUDA_CHECK(cudaFree(pixels));
		CUDA_CHECK(cudaFree(buckets));
	}

	CUDA_CHECK(cudaEventRecord(stop));
	CUDA_CHECK(cudaEventSynchronize(stop));
	float milliseconds;
	CUDA_CHECK(cudaEventElapsedTime(&milliseconds, start, stop));
	fprintf(stderr, "%s render: %.3f ms, %dx%d, %d %s\n",
		positional == 5 ? "CUDA PT" : "CUDA SPPM", milliseconds, width, height, passes,
		positional == 5 ? "samples per subpixel" : "iterations");

	std::vector<Vec> result(count);
	CUDA_CHECK(cudaMemcpy(result.data(), image, size_t(count) * sizeof(Vec), cudaMemcpyDeviceToHost));
	write_image(argv[3], result, width, height);
	for (unsigned char* pixels: textures) CUDA_CHECK(cudaFree(pixels));
	CUDA_CHECK(cudaFree(image));
	CUDA_CHECK(cudaEventDestroy(start));
	CUDA_CHECK(cudaEventDestroy(stop));
	return 0;
}

} // namespace gpu

int main(int argc, char** argv) {
	return gpu::run(argc, argv);
}
