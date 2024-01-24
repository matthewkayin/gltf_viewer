#version 410 core

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_texture_coordinates;

out vec2 texture_coordinates;

void main() {
	texture_coordinates = a_texture_coordinates;
	gl_Position = vec4(a_pos, 1.0);
}