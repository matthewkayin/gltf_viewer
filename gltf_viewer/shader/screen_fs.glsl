#version 410 core

in vec2 texture_coordinate;

out vec4 color;

uniform sampler2D screen_texture;

void main() {
	color = texture(screen_texture, texture_coordinate);
}
