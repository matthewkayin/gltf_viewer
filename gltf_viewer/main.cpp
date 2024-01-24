#ifdef _WIN32
	#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <cstdio>
#include <fstream>
#include <string>
#include <map>
#include <vector>

SDL_Window* window;
SDL_GLContext context;

// Window size
const unsigned int SCREEN_WIDTH = 1280;
const unsigned int SCREEN_HEIGHT = 720;
unsigned int WINDOW_WIDTH = SCREEN_WIDTH;
unsigned int WINDOW_HEIGHT = SCREEN_HEIGHT;

// Timekeeping
const unsigned long FRAME_TIME = (unsigned long)(1000.0 / 60.0);
unsigned long last_time;
unsigned long last_second;
unsigned int frames = 0;
unsigned int fps = 0;
float delta = 0.0f;
bool running = false;

// Rendering resources
GLuint quad_vao;

GLuint sphere_vao;
unsigned int sphere_vao_index_count;
GLuint sphere_albedo;
GLuint sphere_metallic;
GLuint sphere_roughness;
GLuint sphere_normal;
GLuint sphere_ao;

GLuint screen_framebuffer;
GLuint screen_framebuffer_texture;
GLuint screen_intermediate_framebuffer;
GLuint screen_intermediate_framebuffer_texture;

GLuint cube_vao;
GLuint skybox_texture;
GLuint irradiance_map;
GLuint prefilter_map;
GLuint brdf_lookup_texture;

// Shaders
GLuint screen_shader;
GLuint text_shader;
GLuint pbr_shader;
GLuint light_shader;
GLuint cubemap_shader;
GLuint irradiance_map_shader;
GLuint prefilter_shader;
GLuint skybox_shader;
GLuint brdf_shader;

// Fonts
struct Font {
    GLuint atlas;
    unsigned int glyph_width;
    unsigned int glyph_height;
    bool load(const char* path, unsigned int size);
    void render(std::string text, glm::vec2 render_pos, glm::vec3 color);
};

const glm::vec3 FONT_COLOR_WHITE = glm::vec3(1.0f, 1.0f, 1.0f);

static GLuint glyph_vao;
static const int FIRST_CHAR = 32;
Font font_hack10;

bool init();
void quit();
void render_prepare_framebuffer();
void render_flip_framebuffer();
bool shader_compile(GLuint* id, const char* vertex_path, const char* fragment_path);
bool texture_load(GLuint* texture, std::string path);
bool texture_hdr_load(GLuint* skybox_texture, GLuint* irradiance_map, GLuint* prefilter_map, GLuint* brdf_lookup_texture, std::string path);

int main() {
	if (!init()) {
		return -1;
	}

	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);

	glm::vec3 light_positions[] = {
		glm::vec3(0.0f, 0.0f, 10.0f),
	};
	glm::vec3 light_colors[] = {
		glm::vec3(150.0f),
	};
	int light_count = (int)(sizeof(light_positions) / sizeof(glm::vec3));
	light_count = 0;

	glUseProgram(pbr_shader);
	glUniform1i(glGetUniformLocation(pbr_shader, "light_count"), light_count);
	for (int i = 0; i < light_count; i++) {
		glUniform3fv(glGetUniformLocation(pbr_shader, (std::string("light_positions[") + std::to_string(i) + std::string("]")).c_str()), 1, glm::value_ptr(light_positions[i]));
		glUniform3fv(glGetUniformLocation(pbr_shader, (std::string("light_colors[") + std::to_string(i) + std::string("]")).c_str()), 1, glm::value_ptr(light_colors[i]));
	}

	glm::vec3 camera_position = glm::vec3(0.0f, 0.0f, -3.0f);
	glm::vec3 camera_forward = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 camera_right = glm::normalize(glm::cross(camera_forward, camera_up));
	float camera_yaw = -90.0f;
	float camera_pitch = 0.0f;

	const Uint8* keys = SDL_GetKeyboardState(NULL);

	while (running) {
        // Timekeep
		unsigned long current_time = SDL_GetTicks();
		if (current_time - last_time < FRAME_TIME) {
            continue;
		}

		delta = (float)(current_time - last_time) / 60.0f;
		last_time = current_time;

		if (current_time - last_second >= 1000) {
			fps = frames;
			frames = 0;
			last_second += 1000;
		}
          
        // Poll events
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				running = false;
			} else if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
				if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
					SDL_SetRelativeMouseMode(SDL_TRUE);
				}
			} else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
				SDL_SetRelativeMouseMode(SDL_FALSE);
			} else if (e.type == SDL_MOUSEMOTION) {
				camera_yaw += e.motion.xrel * 0.1f;
				camera_pitch -= e.motion.yrel * 0.1f;
				if (camera_pitch > 89.0f) {
					camera_pitch = 89.0f;
				} else if (camera_pitch < -89.0f) {
					camera_pitch = -89.0f;
				}

				camera_forward = glm::normalize(glm::vec3(
					std::cos(glm::radians(camera_yaw)) * std::cos(glm::radians(camera_pitch)),
					std::sin(glm::radians(camera_pitch)),
					std::sin(glm::radians(camera_yaw)) * std::cos(glm::radians(camera_pitch))
				));
				camera_right = glm::normalize(glm::cross(camera_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
				camera_up = glm::normalize(glm::cross(camera_right, camera_forward));
			}
        }

		// Update
		glm::vec3 camera_move_direction = glm::vec3(0.0f);
		if (keys[SDL_SCANCODE_W]) {
			camera_move_direction += camera_forward;
		}
		if (keys[SDL_SCANCODE_S]) {
			camera_move_direction -= camera_forward;
		}
		if (keys[SDL_SCANCODE_A]) {
			camera_move_direction -= camera_right;
		}
		if (keys[SDL_SCANCODE_D]) {
			camera_move_direction += camera_right;
		}
		if (keys[SDL_SCANCODE_E]) {
			camera_move_direction += glm::vec3(0.0f, 1.0f, 0.0f);
		}
		if (keys[SDL_SCANCODE_Q]) {
			camera_move_direction -= glm::vec3(0.0f, 1.0f, 0.0f);
		}
		if (glm::length(camera_move_direction) > 0.0f) {
			camera_position += glm::normalize(camera_move_direction) * 1.0f * delta;
		}

        // RENDER
		// render_prepare_framebuffer();

		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Render main sphere
		glm::mat4 view = glm::lookAt(camera_position, camera_position + camera_forward, camera_up);
		glm::mat4 projection_view = projection * view;

		glUseProgram(pbr_shader);
		glUniformMatrix4fv(glGetUniformLocation(pbr_shader, "projection_view"), 1, GL_FALSE, glm::value_ptr(projection_view));
		glUniform3fv(glGetUniformLocation(pbr_shader, "view_position"), 1, glm::value_ptr(camera_position));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sphere_albedo);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, sphere_normal);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, sphere_metallic);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, sphere_roughness);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, sphere_ao);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_map);
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map);
		glActiveTexture(GL_TEXTURE7);
		glBindTexture(GL_TEXTURE_2D, brdf_lookup_texture);

		int rows = 7;
		int columns = 7;
		float spacing = 2.5f;
		glUniform1i(glGetUniformLocation(pbr_shader, "use_material_maps"), 0);
		glUniform3f(glGetUniformLocation(pbr_shader, "u_albedo"), 0.5f, 0.0f, 0.0f);
		glUniform1f(glGetUniformLocation(pbr_shader, "u_ao"), 1.0f);
		for (int row = 0; row < rows; row++) {
			glUniform1f(glGetUniformLocation(pbr_shader, "u_metallic"), (float)row / (float)rows);
			for (int column = 0; column < columns; column++) {
				glm::mat4 sphere_model = glm::mat4(1.0f);
				sphere_model = glm::translate(sphere_model, glm::vec3(
					(column - (columns / 2)) * spacing,
					(row - (rows / 2)) * spacing,
					0.0f
				));

				glUniform1f(glGetUniformLocation(pbr_shader, "u_roughness"), glm::clamp((float)column / (float)columns, 0.05f, 1.0f));
				glUniformMatrix4fv(glGetUniformLocation(pbr_shader, "model"), 1, GL_FALSE, glm::value_ptr(sphere_model));
				glUniformMatrix3fv(glGetUniformLocation(pbr_shader, "normal_matrix"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(glm::mat3(sphere_model)))));

				glBindVertexArray(sphere_vao);
				glDrawElements(GL_TRIANGLE_STRIP, sphere_vao_index_count, GL_UNSIGNED_INT, 0);
			}
		}


		// Render lights
		for (int i = 0; i < light_count; i++) {
			glm::mat4 light_model = glm::mat4(1.0f);
			light_model = glm::translate(light_model, light_positions[i]);
			light_model = glm::scale(light_model, glm::vec3(0.2f));

			glUseProgram(light_shader);
			glUniformMatrix4fv(glGetUniformLocation(light_shader, "projection_view"), 1, GL_FALSE, glm::value_ptr(projection_view));
			glUniformMatrix4fv(glGetUniformLocation(light_shader, "model"), 1, GL_FALSE, glm::value_ptr(light_model));
			glUniform3fv(glGetUniformLocation(light_shader, "light_color"), 1, glm::value_ptr(light_colors[i]));

			glDrawElements(GL_TRIANGLE_STRIP, sphere_vao_index_count, GL_UNSIGNED_INT, 0);
		}
		glBindVertexArray(0);

		// Render skybox
		glm::mat4 projection_rot_view = projection * glm::mat4(glm::mat3(view));

		glDepthFunc(GL_LEQUAL);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture);
		glUseProgram(skybox_shader);
		glUniformMatrix4fv(glGetUniformLocation(skybox_shader, "projection_rot_view"), 1, GL_FALSE, glm::value_ptr(projection_rot_view));
		glBindVertexArray(cube_vao);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		glDepthFunc(GL_LESS);

		// render_flip_framebuffer();
		SDL_GL_SwapWindow(window);
		frames++;
	}

	quit();
	return 0;
}

// Used in generating font atlas textures
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

bool init() {
	// Init SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
		return false;
	}

	// Possible TODO, replace SDL_Image dependency with stb_image since we're using stbi for HDR textures anyways
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
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	// STB Image
	stbi_set_flip_vertically_on_load(true);

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

	// Setup sphere VAO
	GLuint sphere_vbo;
	GLuint sphere_ebo;
	std::vector<float> data;
	std::vector<unsigned int> indices;
	const unsigned int X_SEGMENTS = 64;
	const unsigned int Y_SEGMENTS = 64;
	const float PI = 3.14159265359f;

	// generate sphere positions, normals, and texture coordinates
	for (unsigned int x = 0; x <= X_SEGMENTS; x++) {
		for (unsigned int y = 0; y <= Y_SEGMENTS; y++) {
			float x_segment = (float)x / (float)X_SEGMENTS;
			float y_segment = (float)y / (float)Y_SEGMENTS;
			float x_pos = std::cos(x_segment * 2.0f * PI) * std::sin(y_segment * PI);
			float y_pos = std::cos(y_segment * PI);
			float z_pos = std::sin(x_segment * 2.0f * PI) * std::sin(y_segment * PI);

			// position
			data.push_back(x_pos);
			data.push_back(y_pos);
			data.push_back(z_pos);
			// normal
			data.push_back(x_pos);
			data.push_back(y_pos);
			data.push_back(z_pos);
			// texure coordinate
			data.push_back(x_pos);
			data.push_back(y_pos);
		}
	}

	// generate sphere indices
	bool odd_row = false;
	for (unsigned int y = 0; y < Y_SEGMENTS; y++) {
		if (!odd_row) {
			for (unsigned int x = 0; x <= X_SEGMENTS; x++) {
				indices.push_back(y * (X_SEGMENTS + 1) + x);
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
			}
		} else {
			for (int x = X_SEGMENTS; x >= 0; x--) {
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
				indices.push_back(y * (X_SEGMENTS + 1) + x);
			}
		}
		odd_row = !odd_row;
	}
	sphere_vao_index_count = (unsigned int)indices.size();

	// generate buffers and buffer data
	glGenVertexArrays(1, &sphere_vao);
	glGenBuffers(1, &sphere_vbo);
	glGenBuffers(1, &sphere_ebo);
	glBindVertexArray(sphere_vao);
	glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

	glBindVertexArray(0);

	// Setup cube vao
	float cube_vertices[] = {
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		// right face
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
		 // bottom face
		 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		  1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		 -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		 // top face
		 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		  1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		  1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		  1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		 -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
	};

	GLuint cube_vbo;
	glGenVertexArrays(1, &cube_vao);
	glGenBuffers(1, &cube_vbo);
	glBindVertexArray(cube_vao);
	glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

	glBindVertexArray(0);

	// Load sphere textures
	if (!texture_load(&sphere_albedo, "./res/rustediron2_basecolor.png")) {
		return false;
	}
	if (!texture_load(&sphere_metallic, "./res/rustediron2_metallic.png")) {
		return false;
	}
	if (!texture_load(&sphere_roughness, "./res/rustediron2_roughness.png")) {
		return false;
	}
	if (!texture_load(&sphere_normal, "./res/rustediron2_normal.png")) {
		return false;
	}
	if (!texture_load(&sphere_ao, "./res/rustediron2_ao.png")) {
		return false;
	}

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
	if (!shader_compile(&screen_shader, "./shader/screen_vs.glsl", "./shader/screen_fs.glsl")) {
		return false;
	}
	if (!shader_compile(&text_shader, "./shader/text_vs.glsl", "./shader/text_fs.glsl")) {
		return false;
	}
	if (!shader_compile(&light_shader, "./shader/light_vs.glsl", "./shader/light_fs.glsl")) {
		return false;
	}
	if (!shader_compile(&pbr_shader, "./shader/pbr_vs.glsl", "./shader/pbr_fs.glsl")) {
		return false;
	}
	glUseProgram(pbr_shader);
	glUniform1i(glGetUniformLocation(pbr_shader, "albedo_map"), 0);
	glUniform1i(glGetUniformLocation(pbr_shader, "normal_map"), 1);
	glUniform1i(glGetUniformLocation(pbr_shader, "metallic_map"), 2);
	glUniform1i(glGetUniformLocation(pbr_shader, "roughness_map"), 3);
	glUniform1i(glGetUniformLocation(pbr_shader, "ao_map"), 4);
	glUniform1i(glGetUniformLocation(pbr_shader, "irradiance_map"), 5);
	glUniform1i(glGetUniformLocation(pbr_shader, "prefilter_map"), 6);
	glUniform1i(glGetUniformLocation(pbr_shader, "brdf_lookup_texture"), 7);

	if (!shader_compile(&cubemap_shader, "./shader/cubemap_vs.glsl", "./shader/cubemap_fs.glsl")) {
		return false;
	}
	glUseProgram(cubemap_shader);
	glUniform1i(glGetUniformLocation(cubemap_shader, "equirectangular_map"), 0);

	if (!shader_compile(&irradiance_map_shader, "./shader/cubemap_vs.glsl", "./shader/irradiance_convolution_fs.glsl")) {
		return false;
	}
	glUseProgram(irradiance_map_shader);
	glUniform1i(glGetUniformLocation(irradiance_map_shader, "environment_map"), 0);

	if (!shader_compile(&skybox_shader, "./shader/skybox_vs.glsl", "./shader/skybox_fs.glsl")) {
		return false;
	}
	glUseProgram(skybox_shader);
	glUniform1i(glGetUniformLocation(skybox_shader, "environment_map"), 0);

	if (!shader_compile(&prefilter_shader, "./shader/cubemap_vs.glsl", "./shader/prefilter_fs.glsl")) {
		return false;
	}
	glUseProgram(prefilter_shader);
	glUniform1i(glGetUniformLocation(prefilter_shader, "environment_map"), 0);

	if (!shader_compile(&brdf_shader, "./shader/brdf_vs.glsl", "./shader/brdf_fs.glsl")) {
		return false;
	}

	// Load HDR texture
	if (!texture_hdr_load(&skybox_texture, &irradiance_map, &prefilter_map, &brdf_lookup_texture, "./res/small_room_8k.hdr")) {
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
	running = true;

	return true;
}

void quit() {
	TTF_Quit();
	IMG_Quit();
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void render_prepare_framebuffer() {
	// Prepare framebuffer for rendering
	glBindFramebuffer(GL_FRAMEBUFFER, screen_framebuffer);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glBlendFunc(GL_ONE, GL_ZERO);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.05f, 0.05f, 0.05f, 0.05f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void render_flip_framebuffer() {
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
	glBindTexture(GL_TEXTURE_2D, screen_intermediate_framebuffer_texture);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	// Render fps
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	font_hack10.render("FPS: " + std::to_string(fps), glm::vec2(0.0f), glm::vec3(1.0f));

	// Swap window
	SDL_GL_SwapWindow(window);
	frames++;
}

bool shader_compile(GLuint* id, const char* vertex_path, const char* fragment_path) {
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

bool texture_load(GLuint* texture, std::string path) {
	SDL_Surface* texture_surface = IMG_Load(path.c_str());
	if (texture_surface == NULL) {
		printf("Unable to load model texture at path %s: %s\n", path.c_str(), IMG_GetError());
		return false;
	}

	GLenum texture_format;
	if (path.find(".hdr") != std::string::npos) {
		texture_format = GL_RGB16F;
	} else if (texture_surface->format->BytesPerPixel == 1) {
		texture_format = GL_RED;
	} else if (texture_surface->format->BytesPerPixel == 3) {
		texture_format = GL_RGB;
	} else if (texture_surface->format->BytesPerPixel == 4) {
		texture_format = GL_RGBA;
	} else {
		printf("Texture format of texture %s not recognized\n", path.c_str());
		return false;
	}

	glGenTextures(1, texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, texture_format, texture_surface->w, texture_surface->h, 0, texture_format, GL_UNSIGNED_BYTE, texture_surface->pixels);
	glBindTexture(GL_TEXTURE_2D, 0);

	SDL_FreeSurface(texture_surface);

	return true;
}

bool texture_hdr_load(GLuint* skybox_texture, GLuint* irradiance_map, GLuint* prefilter_map, GLuint* brdf_lookup_texture, std::string path) {
	// Load file
	int width, height, number_of_components;
	float* data = stbi_loadf(path.c_str(), &width, &height, &number_of_components, 0);
	if (!data) {
		printf("Failed to load HDR texture at path %s\n", path.c_str());
		return false;
	}

	// Convert file to GL texture
	GLuint hdr_texture;
	glGenTextures(1, &hdr_texture);
	glBindTexture(GL_TEXTURE_2D, hdr_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

	stbi_image_free(data);

	// Setup framebuffer
	GLuint capture_fbo;
	GLuint capture_rbo;
	glGenFramebuffers(1, &capture_fbo);
	glGenRenderbuffers(1, &capture_rbo);
	glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, capture_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo);

	// Initialize projection and view matrices
	glm::mat4 capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 capture_views[] = {
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	// Setup skybox cubemap
	glGenTextures(1, skybox_texture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *skybox_texture);
	for (unsigned int i = 0; i < 6; i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, NULL);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Render HDR texture onto skybox texture
	glUseProgram(cubemap_shader);
	glViewport(0, 0, 512, 512);
	glBindVertexArray(cube_vao);
	glBindTexture(GL_TEXTURE_2D, hdr_texture);
	for (unsigned int i = 0; i < 6; i++) {
		glm::mat4 projection_view = capture_projection * capture_views[i];
		glUniformMatrix4fv(glGetUniformLocation(cubemap_shader, "projection_view"), 1, GL_FALSE, glm::value_ptr(projection_view));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *skybox_texture, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
	glBindTexture(GL_TEXTURE_CUBE_MAP, *skybox_texture);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	
	// Setup irradiance cubemap
	glGenTextures(1, irradiance_map);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *irradiance_map);
	for (unsigned int i = 0; i < 6; i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, NULL);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Render HDR texture onto irradiance map
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
	glUseProgram(irradiance_map_shader);
	glViewport(0, 0, 32, 32);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *skybox_texture);
	for (unsigned int i = 0; i < 6; i++) {
		glm::mat4 projection_view = capture_projection * capture_views[i];
		glUniformMatrix4fv(glGetUniformLocation(irradiance_map_shader, "projection_view"), 1, GL_FALSE, glm::value_ptr(projection_view));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *irradiance_map, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// Setup prefilter map
	glGenTextures(1, prefilter_map);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *prefilter_map);
	for (unsigned int i = 0; i < 6; i++) {
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, NULL);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	// Capture the prefilter mipmap levels
	glUseProgram(prefilter_shader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, *skybox_texture);
	unsigned int max_mip_levels = 5;
	for (unsigned int mip = 0; mip < max_mip_levels; mip++) {
		unsigned int mip_width = 128 * std::pow(0.5, mip);
		unsigned int mip_height = 128 * std::pow(0.5, mip);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mip_width, mip_height);
		glViewport(0, 0, mip_width, mip_height);

		float roughness = (float)mip / (float)(max_mip_levels - 1);
		glUniform1f(glGetUniformLocation(prefilter_shader, "roughness"), roughness);
		for (unsigned int i = 0; i < 6; i++) {
			glm::mat4 projection_view = capture_projection * capture_views[i];
			glUniformMatrix4fv(glGetUniformLocation(prefilter_shader, "projection_view"), 1, GL_FALSE, glm::value_ptr(projection_view));
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *prefilter_map, mip);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
	}

	// Generate BRDF lookup texture
	glGenTextures(1, brdf_lookup_texture);
	glBindTexture(GL_TEXTURE_2D, *brdf_lookup_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *brdf_lookup_texture, 0);
	glViewport(0, 0, 512, 512);
	glUseProgram(brdf_shader);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glBindVertexArray(quad_vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return true;
}
