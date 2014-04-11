#include <stdbool.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef enum {
	MinimaidPadLightP1Up = 1,
	MinimaidPadLightP1Down,
	MinimaidPadLightP1Left,
	MinimaidPadLightP1Right,

	MinimaidPadLightP2Up,
	MinimaidPadLightP2Down,
	MinimaidPadLightP2Left,
	MinimaidPadLightP2Right,

	MinimaidPadLightCount
} MinimaidPadLights;

typedef enum {
	MinimaidCabinetLightMenuLeft = 1,
	MinimaidCabinetLightMenuRight,

	MinimaidCabinetLightMarqueeBR, // Bottom Right
	MinimaidCabinetLightMarqueeTR, // Top Right
	MinimaidCabinetLightMarqueeBL, // Bottom Left
	MinimaidCabinetLightMarqueeTL, // Top Left

	MinimaidCabinetLightBassLights,

	MinimaidCabinetLightCount
} MinimaidCabinetLights;

typedef enum {
	MinimaidInputP1Up = 1,
	MinimaidInputP1Down,
	MinimaidInputP1Left,
	MinimaidInputP1Right,

	MinimaidInputP1MenuLeft,
	MinimaidInputP1MenuSelect,
	MinimaidInputP1MenuRight,

	MinimaidInputP2Up,
	MinimaidInputP2Down,
	MinimaidInputP2Left,
	MinimaidInputP2Right,

	MinimaidInputP2MenuLeft,
	MinimaidInputP2MenuSelect,
	MinimaidInputP2MenuRight,

	MinimaidInputCount
} MinimaidInput;

typedef struct Minimaid Minimaid;
typedef struct MinimaidLightsState MinimaidLightsState;
typedef uint64_t MinimaidInputState;

Minimaid* mm_open_connection(void);
void mm_close_connection(Minimaid *c);

/* Lights */
MinimaidLightsState* mm_get_lights_state(Minimaid *c);
bool mm_set_lights_state(Minimaid *c, MinimaidLightsState *state);

void mm_set_cabinet_light_enabled(MinimaidLightsState *state, MinimaidCabinetLights light, bool enabled);
void mm_set_pad_light_enabled(MinimaidLightsState *state, MinimaidPadLights light, bool enabled);
void mm_set_all_lights_enabled(MinimaidLightsState *state, bool enabled);

bool mm_get_cabinet_light_enabled(MinimaidLightsState *state, MinimaidCabinetLights light);
bool mm_get_pad_light_enabled(MinimaidLightsState *state, MinimaidPadLights light);

/* Input */
MinimaidInputState mm_get_input_state(Minimaid *c);
bool mm_get_input_enabled(MinimaidInputState state, MinimaidInput input);
