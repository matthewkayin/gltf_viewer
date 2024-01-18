#include "engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <fstream>
#include <string>
#include <map>

bool engine_running;

static SDL_Window* window;
static SDL_GLContext context;

// Window size
static unsigned int WINDOW_WIDTH = SCREEN_WIDTH;
static unsigned int WINDOW_HEIGHT = SCREEN_HEIGHT;

// Timekeeping
static const unsigned long FRAME_TIME = (unsigned long)(1000.0 / 60.0);
static unsigned long last_time;
static unsigned long last_second;
static unsigned int frames = 0;
static unsigned int fps = 0;
float delta = 0.0f;

// Rendering resources
static GLuint quad_vao;
static GLuint screen_framebuffer;
static GLuint screen_framebuffer_texture;
static GLuint screen_intermediate_framebuffer;
static GLuint screen_intermediate_framebuffer_texture;

// Shaders
static GLuint screen_shader;
static GLuint text_shader;

// Font resources
static GLuint glyph_vao;
static const int FIRST_CHAR = 32;
Font font_hack10;

int next_largest_power_of_two(int number) {
    int power_of_two = 1;
    while (power_of_two < number) {
        power_of_two *= 2;
    }

    return power_of_two;
}

bool Font::load(const char* path, unsigned int size) {
    static const SDL_Color COLOR_WHITE = { 255, 255, 255, 255 };

    // Load the font
    TTF_Font* ttf_font = TTF_OpenFont(path, size);
    if (ttf_font == NULL) {
        printf("Unable to open font at path %s. SDL Error: %s\n", path, TTF_GetError());
        return false;
    }

    // Render each glyph to a surface
    SDL_Surface* glyphs[96];
    int max_width;
    int max_height;
    for (int i = 0; i < 96; i++) {
        char text[2] = { (char)(i + FIRST_CHAR), '\0' };
        glyphs[i] = TTF_RenderText_Blended(ttf_font, text, COLOR_WHITE);
        if (glyphs[i] == NULL) {
            return false;
        }

        if (i == 0 || max_width < glyphs[i]->w) {
            max_width = glyphs[i]->w;
        }
        if (i == 0 || max_height < glyphs[i]->h) {
            max_height = glyphs[i]->h;
        }
    }

    int atlas_width = next_largest_power_of_two(max_width * 96);
    int atlas_height = next_largest_power_of_two(max_height);
    SDL_Surface* atlas_surface = SDL_CreateRGBSurface(0, atlas_width, atlas_height, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    for (int i = 0; i < 96; i++) {
        SDL_Rect dest_rect = {  max_width * i, 0, glyphs[i]->w, glyphs[i]->h };
        SDL_BlitSurface(glyphs[i], NULL, atlas_surface, &dest_rect);
    }

    // Generate OpenGL texture
    glGenTextures(1, &atlas);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas_width, atlas_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, atlas_surface->pixels);

    // Finish setting up font struct
    glyph_width = (unsigned int)max_width;
    glyph_height = (unsigned int)max_height;

    // Cleanup
    glBindTexture(GL_TEXTURE_2D, 0);
    for (int i = 0; i < 96; i++) {
        SDL_FreeSurface(glyphs[i]);
    }
    SDL_FreeSurface(atlas_surface);
    TTF_CloseFont(ttf_font);

    return true;
}

void Font::render(std::string text, glm::vec2 render_pos, glm::vec3 color) {
    glUseProgram(text_shader);
    glm::vec2 atlas_size = glm::vec2((float)next_largest_power_of_two(glyph_width * 96), (float)next_largest_power_of_two(glyph_height));
    glUniform2fv(glGetUniformLocation(text_shader, "atlas_size"), 1, glm::value_ptr(atlas_size));
    glm::vec2 render_size = glm::vec2((float)glyph_width, (float)glyph_height);
    glUniform2fv(glGetUniformLocation(text_shader, "render_size"), 1, glm::value_ptr(render_size));
    glUniform3fv(glGetUniformLocation(text_shader, "font_color"), 1, glm::value_ptr(color));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glBindVertexArray(glyph_vao);

    glm::vec2 render_coords = render_pos;
    glm::vec2 texture_offset;
    for (char c : text) {
        int glyph_index = (int)c - FIRST_CHAR;
        texture_offset.x = (float)(glyph_width * glyph_index);
        glUniform2fv(glGetUniformLocation(text_shader, "render_coords"), 1, glm::value_ptr(render_coords));
        glUniform2fv(glGetUniformLocation(text_shader, "texture_offset"), 1, glm::value_ptr(texture_offset));

        glDrawArrays(GL_TRIANGLES, 0, 6);

        render_coords.x += glyph_width;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool engine_init() {
	// Init SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		return false;
	}

	int img_flags = IMG_INIT_PNG;
	if (!(IMG_Init(img_flags) & img_flags)) {
		printf("Error initializing SDL_image: %s\n", IMG_GetError());
		return false;
	}

	if (TTF_Init() == -1) {
		printf("Error initializing SDL_ttf: %s\n", TTF_GetError());
		return false;
	}

	// Set GL version
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_LoadLibrary(NULL);

	// Create SDL window 
	window = SDL_CreateWindow("gltf viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
	if (window == NULL) {
		printf("Error creating window: %s\n", SDL_GetError());
		return false;
	}

	// Create GL context
	context = SDL_GL_CreateContext(window);
	if (context == NULL) {
		printf("Error creating GL context: %s\n", SDL_GetError());
		return false;
	}

	// Setup GLAD
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	if (glGenVertexArrays == NULL) {
		printf("Error loading OpenGL.\n");
		return false;
	}

	// Set GL flags
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	// Setup quad VAO
	float quad_vertices[] = {
		// positions   // texCoords
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f
	};
	GLuint quad_vbo;

	glGenVertexArrays(1, &quad_vao);
	glGenBuffers(1, &quad_vbo);
	glBindVertexArray(quad_vao);
	glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

	glBindVertexArray(0);

	// Setup screen framebuffer
	glGenFramebuffers(1, &screen_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, screen_framebuffer);

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &screen_framebuffer_texture);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, screen_framebuffer_texture);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, SCREEN_WIDTH, SCREEN_HEIGHT, GL_TRUE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, screen_framebuffer_texture, 0);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

	GLuint rbo;
	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, SCREEN_WIDTH, SCREEN_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Error, framebuffer not complete.\n");
		return false;
	}
	
	glGenFramebuffers(1, &screen_intermediate_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, screen_intermediate_framebuffer);
	glGenTextures(1, &screen_intermediate_framebuffer_texture);
	glBindTexture(GL_TEXTURE_2D, screen_intermediate_framebuffer_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screen_intermediate_framebuffer_texture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		printf("Framebuffer not complete!\n");
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Init shaders
	if (!engine_shader_compile(&screen_shader, "./shader/screen_vs.glsl", "./shader/screen_fs.glsl")) {
		return false;
	}
	if (!engine_shader_compile(&text_shader, "./shader/text_vs.glsl", "./shader/text_fs.glsl")) {
		return false;
	}

	// Buffer glyph vertex data
	float glyph_vertices[12] = {
		0.0f, 0.0f,
		1.0f, 0.0f,
		0.0f, 1.0f,
		0.0f, 1.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
	};

	GLuint glyph_vbo;
	glGenVertexArrays(1, &glyph_vao);
	glGenBuffers(1, &glyph_vbo);
	glBindVertexArray(glyph_vao);
	glBindBuffer(GL_ARRAY_BUFFER, glyph_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glyph_vertices), glyph_vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// Setup font shader
	glUseProgram(text_shader);
	float screen_size[2] = { SCREEN_WIDTH, SCREEN_HEIGHT };
	glUniform2fv(glGetUniformLocation(text_shader, "screen_size"), 1, &screen_size[0]);
	glUniform1ui(glGetUniformLocation(text_shader, "u_texture"), 0);

	// Init fonts
	if (!font_hack10.load("./res/hack.ttf", 10)) {
		return false;
	}

	// Init timekeep values
	last_time = SDL_GetTicks();
	last_second = last_time;
	engine_running = true;

	return true;
}

void engine_quit() {
	TTF_Quit();
	IMG_Quit();
	SDL_DestroyWindow(window);
	SDL_Quit();
}

bool engine_shader_compile(GLuint* id, const char* vertex_path, const char* fragment_path) {
	// Read vertex shader
	std::string vertex_source;
	std::ifstream vertex_file;
	std::string line;
	vertex_file.open(vertex_path);
	if (!vertex_file.is_open()) {
		printf("Error opening vertex shader at path %s\n", vertex_path);
		return false;
	}
	while (std::getline(vertex_file, line)) {
		vertex_source += line + "\n";
	}
	vertex_file.close();

	// Compile vertex shader
	int success;
	char info_log[512];
	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	const char* vertex_source_cstr = vertex_source.c_str();
	glShaderSource(vertex_shader, 1, &vertex_source_cstr, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex_shader, 512, NULL, info_log);
		printf("Error: vertex shader %s failed to compile: %s\n", vertex_path, info_log);
		return false;
	}

	// Read fragment shader
	std::string fragment_source;
	std::ifstream fragment_file;
	fragment_file.open(fragment_path);
	if (!fragment_file.is_open()) {
		printf("Error opening fragment shader at path %s\n", fragment_path);
		return false;
	}
	while (std::getline(fragment_file, line)) {
		fragment_source += line + "\n";
	}
	fragment_file.close();

	// Compile fragment shader
	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	const char* fragment_source_cstr = fragment_source.c_str();
	glShaderSource(fragment_shader, 1, &fragment_source_cstr, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment_shader, 512, NULL, info_log);
		printf("Error: vertex shader %s failed to compile: %s\n", vertex_path, info_log);
		return false;
	}

	// link program
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, info_log);
		printf("Error linking shader program. Vertex: %s Fragment: %s.\n%s\n", vertex_path, fragment_path, info_log);
		return false;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	*id = program;
	return true;
}

bool engine_timekeep() {
	unsigned long current_time = SDL_GetTicks();
	if (current_time - last_time < FRAME_TIME) {
		return false;
	}

	delta = (float)(current_time - last_time) / 60.0f;
	last_time = current_time;

	if (current_time - last_second >= 1000) {
		fps = frames;
		frames = 0;
		last_second += 1000;
	}

	return true;
}

void engine_poll_events() {
	SDL_Event e;
	while (SDL_PollEvent(&e) != 0) {
		if (e.type == SDL_QUIT) {
			engine_running = false;
		}
	}
}

void engine_prepare_render() {
	glBindFramebuffer(GL_FRAMEBUFFER, screen_framebuffer);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glBlendFunc(GL_ONE, GL_ZERO);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.05f, 0.05f, 0.05f, 0.05f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void engine_finish_render() {
	// Blit multisample buffer to intermediate buffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER, screen_framebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, screen_intermediate_framebuffer);
	glBlitFramebuffer(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	// Render framebuffer to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_DEPTH_TEST);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(screen_shader);
	glBindVertexArray(quad_vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, screen_framebuffer_texture);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	// Render fps
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	font_hack10.render("FPS: " + std::to_string(fps), glm::vec2(0.0f), glm::vec3(1.0f));

	SDL_GL_SwapWindow(window);
	frames++;
}
