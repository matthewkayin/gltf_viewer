#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

const unsigned int SCREEN_WIDTH = 1280;
const unsigned int SCREEN_HEIGHT = 720;

struct Font {
    GLuint atlas;
    unsigned int glyph_width;
    unsigned int glyph_height;
    bool load(const char* path, unsigned int size);
    void render(std::string text, glm::vec2 render_pos, glm::vec3 color);
};

const glm::vec3 FONT_COLOR_WHITE = glm::vec3(1.0f, 1.0f, 1.0f);

extern Font font_hack10;

extern bool engine_running;
extern float delta;

bool engine_init();
void engine_quit();
bool engine_shader_compile(GLuint* id, const char* vertex_path, const char* fragment_path);
bool engine_timekeep();
void engine_poll_events();
void engine_prepare_render();
void engine_finish_render();
