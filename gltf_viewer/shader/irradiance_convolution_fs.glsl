#version 410 core

out vec4 color;
in vec3 local_pos;

uniform samplerCube environment_map;

const float PI = 3.14159265359;

void main() {
	vec3 normal = normalize(local_pos);
	vec3 irradiance = vec3(0.0);

	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));

	float sample_delta = 0.025;
	float sample_number = 0.0;
	for (float phi = 0.0; phi < 2.0 * PI; phi += sample_delta) {
		for (float theta = 0.0; theta < 0.5 * PI; theta += sample_delta) {
			// spherical to cartesian
			vec3 tangent_sample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			// tangent space to world
			vec3 sample_vec = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal;
			irradiance += texture(environment_map, sample_vec).rgb * cos(theta) * sin(theta);
			sample_number++;
		}
	}

	irradiance = PI * irradiance * (1.0 / sample_number);

	color = vec4(irradiance, 1.0);
}