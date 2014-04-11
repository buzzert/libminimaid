#include "minimaid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static Minimaid *__mm = NULL;

void cycle_cabinet_lights(Minimaid *mm) {
	MinimaidLightsState *lights = mm_get_lights_state(mm);

	unsigned int i = 0;
	MinimaidCabinetLights currentLight = MinimaidCabinetLightMarqueeBR;
	for (;;) {
		mm_set_cabinet_light_enabled(lights, currentLight, false);
		currentLight = (MinimaidCabinetLightMarqueeBR + i);
		mm_set_cabinet_light_enabled(lights, currentLight, true);
		mm_set_lights_state(mm, lights);

		i = (i + 1) % 4;

		usleep(1000000);
	}
}

void random_pad_lights(Minimaid *mm) {
	MinimaidLightsState *lights = mm_get_lights_state(mm);

	unsigned int i = 0;
	MinimaidPadLights currentLight = MinimaidPadLightP1Up;
	for (;;) {
		mm_set_pad_light_enabled(lights, currentLight, false);
		currentLight = rand() % (MinimaidPadLightP1Right + 1);
		mm_set_pad_light_enabled(lights, currentLight, true);
		mm_set_lights_state(mm, lights);

		usleep(100000);
	}
}

void input_to_lights(Minimaid *mm) {
	MinimaidLightsState *lights = mm_get_lights_state(mm);

	MinimaidPadLights mapping[MinimaidInputCount] = {
		[MinimaidInputP1Up]    = MinimaidPadLightP1Up,
		[MinimaidInputP1Down]  = MinimaidPadLightP1Down,
		[MinimaidInputP1Left]  = MinimaidPadLightP1Left,
		[MinimaidInputP1Right] = MinimaidPadLightP1Right,

		[MinimaidInputP2Up]    = MinimaidPadLightP2Up,
		[MinimaidInputP2Down]  = MinimaidPadLightP2Down,
		[MinimaidInputP2Left]  = MinimaidPadLightP2Left,
		[MinimaidInputP2Right] = MinimaidPadLightP2Right,
	};

	// Clear lights state
	mm_set_all_lights_enabled(lights, false);

	for (;;) {
		MinimaidInputState inputState = mm_get_input_state(mm);

		for (MinimaidInput input = 0; input < MinimaidInputCount; input++) {
			mm_set_pad_light_enabled(lights, mapping[input], mm_get_input_enabled(inputState, input));
		}

		mm_set_lights_state(mm, lights);

		usleep(500);
	}
}

void signal_handler(int signal) {
	if (signal == SIGINT) {
		mm_close_connection(__mm);
	}
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: minimaidtool [tool]\n");
		printf("    Tools:\n");
		printf("        cycle    - cycles through all cabinet marquee lights\n");
		printf("        random   - randomizes lights on 1P side\n");
		printf("        input    - turns on lights for panels that are stepped on\n");

		return 1;
	}

	__mm = mm_open_connection();
	if (!__mm) {
		fprintf(stderr, "Couldn't get device\n");
		return 1;
	}

	signal(SIGINT, signal_handler);

	if (strcmp(argv[1], "cycle") == 0)
		cycle_cabinet_lights(__mm);

	if (strcmp(argv[1], "random") == 0)
		random_pad_lights(__mm);

	if (strcmp(argv[1], "input") == 0)
		input_to_lights(__mm);

	mm_close_connection(__mm);

	return 0;
}