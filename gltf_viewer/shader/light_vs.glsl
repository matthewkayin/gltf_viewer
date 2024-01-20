#version 410 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texture_coordinates;

uniform mat4 projection_view;
uniform mat4 model;

void main() {
	gl_Position = projection_view * model * vec4(a_position, 1.0);
}