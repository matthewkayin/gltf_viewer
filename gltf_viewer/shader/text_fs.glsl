#version 410 core

in vec2 fragment_vertex_pos;

out vec4 color;

uniform sampler2D u_texture;
uniform vec2 texture_offset;
uniform vec2 render_size;
uniform vec2 atlas_size;
uniform vec3 font_color;

void main() {
    vec2 texture_coordinate = vec2((texture_offset.x + (render_size.x * fragment_vertex_pos.x)) / atlas_size.x, (texture_offset.y + (render_size.y * fragment_vertex_pos.y)) / atlas_size.y);
    color = vec4(font_color, texture(u_texture, texture_coordinate).r);
}