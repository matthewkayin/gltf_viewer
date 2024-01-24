#version 410 core

layout (location = 0) in vec3 a_pos;

out vec3 local_pos;

uniform mat4 projection_view;

void main() {
	local_pos = a_pos;
	gl_Position = projection_view * vec4(local_pos, 1.0);
}