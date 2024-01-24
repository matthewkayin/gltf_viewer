#version 410 core

out vec4 color;
in vec3 local_pos;

uniform sampler2D equirectangular_map;

const vec2 inverse_atan = vec2(0.1591, 0.3183);
vec2 sample_spherical_map(vec3 v) {
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= inverse_atan;
	uv += 0.5;
	return uv;
}

void main() {
	vec2 uv = sample_spherical_map(normalize(local_pos));
	color = vec4(texture(equirectangular_map, uv).rgb, 1.0);
}