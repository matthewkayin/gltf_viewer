#version 410 core

layout (location = 0) in vec3 a_pos;

uniform mat4 projection_rot_view;

out vec3 world_position;

void main() {
	world_position = a_pos;
	gl_Position = (projection_rot_view * vec4(world_position, 1.0)).xyww;
}