#version 410 core

out vec2 color;
in vec2 texture_coordinates;

const float PI = 3.14159265359;

vec2 integrate_brdf(float n_dot_v, float roughness);

void main() {
	color = integrate_brdf(texture_coordinates.x, texture_coordinates.y);
}

float geometry_shlick_ggx(float n_dot_v, float roughness) {
	float a = roughness;
	float k = (a * a) / 2.0;

	float denominator = n_dot_v * (1.0 - k) + k;

	return n_dot_v / denominator;
}

float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness) {
	float n_dot_v = max(dot(n, v), 0.0);
	float n_dot_l = max(dot(n, l), 0.0);
	float ggx2 = geometry_shlick_ggx(n_dot_v, roughness);
	float ggx1 = geometry_shlick_ggx(n_dot_l, roughness);

	return ggx1 * ggx2;
}

float radical_inverse_vdc(uint bits) {
	bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 hammersley(uint i, uint n) {
	return vec2(float(i) / float(n), radical_inverse_vdc(i));
}

vec3 importance_sample_ggx(vec2 xi, vec3 normal, float roughness) {
	float roughness2 = roughness * roughness;

	float phi = 2.0 * PI * xi.x;
	float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (roughness2 * roughness2 - 1.0) * xi.y));
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

	vec3 h = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

	vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 tangent = normalize(cross(up, normal));
	vec3 bitangent = cross(normal, tangent);

	vec3 sample_vec = (tangent * h.x) + (bitangent * h.y) + (normal * h.z);
	return normalize(sample_vec);
}

vec2 integrate_brdf(float n_dot_v, float roughness) {
	vec3 v = vec3(sqrt(1.0 - n_dot_v * n_dot_v), 0.0, n_dot_v);
	float a = 0.0;
	float b = 0.0;
	vec3 n = vec3(0.0, 0.0, 1.0);

	const uint SAMPLE_COUNT = 1024u;
	for (uint i = 0u; i < SAMPLE_COUNT; i++) {
		vec2 xi = hammersley(i, SAMPLE_COUNT);
		vec3 h = importance_sample_ggx(xi, n, roughness);
		vec3 l = normalize(2.0 * dot(v, h) * h - v);

		float n_dot_l = max(l.z, 0.0);
		float n_dot_h = max(h.z, 0.0);
		float v_dot_h = max(dot(v, h), 0.0);

		if (n_dot_l > 0.0) {
			float g = geometry_smith(n, v, l, roughness);
			float g_vis = (g * v_dot_h) / (n_dot_h * n_dot_v);
			float fc = pow(1.0 - v_dot_h, 5.0);

			a += (1.0 - fc) * g_vis;
			b += fc * g_vis;
		}
	}

	return vec2(a / float(SAMPLE_COUNT), b / float(SAMPLE_COUNT));
}