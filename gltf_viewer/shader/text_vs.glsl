#version 410 core

layout (location = 0) in vec2 vertex_position;

out vec2 fragment_vertex_pos;

uniform vec2 screen_size;
uniform vec2 render_coords;
uniform vec2 render_size;

void main() {
    vec2 render_position = vec2(render_coords.x + (vertex_position.x * render_size.x), render_coords.y + (vertex_position.y * render_size.y));
    render_position = vec2((render_position.x / (screen_size.x / 2)) - 1, 1 - (render_position.y / (screen_size.y / 2)));
    gl_Position = vec4(render_position, 0.0, 1.0);
    fragment_vertex_pos = vertex_position;
}
