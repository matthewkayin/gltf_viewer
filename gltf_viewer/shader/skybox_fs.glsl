#version 410 core

out vec4 color;
in vec3 world_position;

uniform samplerCube environment_map;

void main() {
	vec3 environment_color = texture(environment_map, world_position).rgb;

	// HDR tonemap
	environment_color = environment_color / (environment_color + vec3(1.0));
	// gamma correct
	environment_color = pow(environment_color, vec3(1.0 / 2.2));

	color = vec4(environment_color, 1.0);
}