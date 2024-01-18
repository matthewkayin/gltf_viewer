#ifdef _WIN32
	#define SDL_MAIN_HANDLED
#endif

#include "engine.h"

#include <SDL2/SDL.h>
#include <cstdio>

int main() {
	if (!engine_init()) {
		return -1;
	}

	while (engine_running) {
		engine_poll_events();
		float delta = engine_timekeep();
		engine_prepare_render();
		engine_finish_render();
	}

	engine_quit();
	return 0;
}