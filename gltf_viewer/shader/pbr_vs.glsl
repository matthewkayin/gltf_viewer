#version 410 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texture_coordinates;

out vec2 texture_coordinates;
out vec3 world_position;
out vec3 normal_in;

uniform mat4 projection_view;
uniform mat4 model;
uniform mat3 normal_matrix;

void main() {
	texture_coordinates = a_texture_coordinates;
	world_position = vec3(model * vec4(a_position, 1.0));
	normal_in = normal_matrix * a_normal;

	gl_Position = projection_view * vec4(world_position, 1.0);
}