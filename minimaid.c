#include "minimaid.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

static const int MM_VENDOR_ID  = 0xBEEF;
static const int MM_PRODUCT_ID = 0x5730;

// From the HID spec:
static const int HID_GET_REPORT = 0x01;
static const int HID_SET_REPORT = 0x09;
static const int HID_REPORT_TYPE_INPUT = 0x01;
static const int HID_REPORT_TYPE_OUTPUT = 0x02;
static const int HID_REPORT_TYPE_FEATURE = 0x03;

static const int CONTROL_REQUEST_TYPE_IN  = LIBUSB_ENDPOINT_IN  | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;
static const int CONTROL_REQUEST_TYPE_OUT = LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE;

static const int INPUT_INTERFACE  = 0;
static const int LIGHTS_INTERFACE = 1;

static const unsigned int IO_TIMEOUT_MS = 5000;

#define LUSB_LOG(code) _log_libusb_error((code), __func__)

struct Minimaid {
	int device_ready;

	struct libusb_device_handle *dev_handle;
};

static const unsigned MinimaidLightsStateRawLength = 32;
struct MinimaidLightsState {
	unsigned char *raw_light_data;
};

typedef struct { unsigned index; unsigned offset; } LightMapping;
static LightMapping _PadLightMappings[MinimaidPadLightCount] = {
	[MinimaidPadLightP1Up]    = { 2, (1 << 0) },
	[MinimaidPadLightP1Down]  = { 2, (1 << 1) },
	[MinimaidPadLightP1Left]  = { 2, (1 << 2) },
	[MinimaidPadLightP1Right] = { 2, (1 << 3) },

	[MinimaidPadLightP2Up]    = { 3, (1 << 0) },
	[MinimaidPadLightP2Down]  = { 3, (1 << 1) },
	[MinimaidPadLightP2Left]  = { 3, (1 << 2) },
	[MinimaidPadLightP2Right] = { 3, (1 << 3) },
};

static LightMapping _CabinetLightMappings[MinimaidCabinetLightCount] = {
	[MinimaidCabinetLightMenuLeft]  = { 1, (1 << 2) },
	[MinimaidCabinetLightMenuRight] = { 1, (1 << 3) },

	[MinimaidCabinetLightMarqueeBR] = { 1, (1 << 4) },
	[MinimaidCabinetLightMarqueeTR] = { 1, (1 << 5) },
	[MinimaidCabinetLightMarqueeBL] = { 1, (1 << 6) },
	[MinimaidCabinetLightMarqueeTL] = { 1, (1 << 7) },

	[MinimaidCabinetLightBassLights] = { 4, (1 << 0) },
};

static uint64_t _InputMapping[MinimaidInputCount] = {
	[MinimaidInputP1Up]			= (1ULL << 26),
	[MinimaidInputP1Down]		= (1ULL << 28),
	[MinimaidInputP1Left]		= (1ULL << 30),
	[MinimaidInputP1Right]		= (1ULL << 32),

	[MinimaidInputP1MenuLeft]	= (1ULL << 36),
	[MinimaidInputP1MenuSelect] = (1ULL << 24),
	[MinimaidInputP1MenuRight]  = (1ULL << 38),

	[MinimaidInputP2Up]			= (1ULL << 27),
	[MinimaidInputP2Down]		= (1ULL << 29),
	[MinimaidInputP2Left]		= (1ULL << 31),
	[MinimaidInputP2Right]		= (1ULL << 33),

	[MinimaidInputP2MenuLeft]	= (1ULL << 37),
	[MinimaidInputP2MenuSelect] = (1ULL << 25),
	[MinimaidInputP2MenuRight]  = (1ULL << 39),
};

void _log_libusb_error(int error_code, const char *calling_function) {
	fprintf(stderr, "Minimaid: %s libusb error: %s\n", calling_function, libusb_error_name(error_code));
}

Minimaid* mm_open_connection(void) {
	struct libusb_device_handle *device = NULL;

	int result;
	int device_ready = 0;
	Minimaid *mm_device = NULL;

	result = libusb_init(NULL);
	if (result >= 0) {
		device = libusb_open_device_with_vid_pid(NULL, MM_VENDOR_ID, MM_PRODUCT_ID);
		if (device != NULL) {
			// Detach from HID to use with libusb
			int libusb_error = 0;
			
			libusb_error = libusb_detach_kernel_driver(device, INPUT_INTERFACE);
			LUSB_LOG(libusb_error);

			libusb_detach_kernel_driver(device, LIGHTS_INTERFACE);
			LUSB_LOG(libusb_error);
			
			// Claim the device
			result  = libusb_claim_interface(device, LIGHTS_INTERFACE);
			LUSB_LOG(libusb_error);

			result &= libusb_claim_interface(device, INPUT_INTERFACE);
			LUSB_LOG(libusb_error);
			
			device_ready = (result == 0);
			if (device_ready) {
				mm_device = malloc(sizeof(Minimaid));
				mm_device->device_ready = 1;

				mm_device->dev_handle = device;

				// Reset light state
				MinimaidLightsState *lights = mm_get_lights_state(mm_device);
				mm_set_all_lights_enabled(lights, false);
				mm_set_lights_state(mm_device, lights);
			} else {
				fprintf(stderr, "Minimaid: Could not open device\n");
			}
		} else {
			fprintf(stderr, "Minimaid: Couldn't find device (plugged in?)\n");
		}
	} else {
		LUSB_LOG(result);
	}

	return mm_device;
}


void mm_close_connection(Minimaid *c) {
	// Turn off all lights first
	MinimaidLightsState *lights = mm_get_lights_state(c);
	mm_set_all_lights_enabled(lights, false);
	mm_set_lights_state(c, lights);

	libusb_attach_kernel_driver(c->dev_handle, INPUT_INTERFACE);
	libusb_attach_kernel_driver(c->dev_handle, LIGHTS_INTERFACE);

	libusb_close(c->dev_handle);
	libusb_exit(NULL);

	free(c);
}

/* Lights */

MinimaidLightsState* mm_get_lights_state(Minimaid *c) {
	unsigned char *raw_state = calloc(MinimaidLightsStateRawLength, 1);
	int bytes_transferred = libusb_control_transfer(
		c->dev_handle,
		CONTROL_REQUEST_TYPE_OUT,
		HID_GET_REPORT,
		(HID_REPORT_TYPE_INPUT << 8) | 0x00,
		LIGHTS_INTERFACE,
		raw_state,
		MinimaidLightsStateRawLength,
		IO_TIMEOUT_MS
	);

	MinimaidLightsState *state = malloc(sizeof(MinimaidLightsState));
	state->raw_light_data = raw_state;

	return state;
}

bool mm_set_lights_state(Minimaid *c, MinimaidLightsState *state) {
	state->raw_light_data[2] |= 0x10;
	state->raw_light_data[3] |= 0x10;
	state->raw_light_data[6] |= 0x10;

	int bytes_transferred = libusb_control_transfer(
		c->dev_handle,
		CONTROL_REQUEST_TYPE_OUT,
		HID_SET_REPORT,
		(HID_REPORT_TYPE_OUTPUT << 8) | 0x00,
		LIGHTS_INTERFACE,
		state->raw_light_data,
		MinimaidLightsStateRawLength,
		IO_TIMEOUT_MS
	);

	return (bytes_transferred > 0);
}

void mm_set_cabinet_light_enabled(MinimaidLightsState *state, MinimaidCabinetLights light, bool enabled) {
	LightMapping mapping = _CabinetLightMappings[light];
	if (enabled) {
		state->raw_light_data[mapping.index] |= mapping.offset;
	} else {
		state->raw_light_data[mapping.index] &= ~(mapping.offset);
	}
}

void mm_set_pad_light_enabled(MinimaidLightsState *state, MinimaidPadLights light, bool enabled) {
	LightMapping mapping = _PadLightMappings[light];
	if (enabled) {
		state->raw_light_data[mapping.index] |= mapping.offset;
	} else {
		state->raw_light_data[mapping.index] &= ~(mapping.offset);
	}
}

void mm_set_all_lights_enabled(MinimaidLightsState *state, bool enabled) {
	for (unsigned i = 0; i < MinimaidCabinetLightCount; i++) {
		mm_set_cabinet_light_enabled(state, i, enabled);
	}

	for (unsigned i = 0; i < MinimaidPadLightCount; i++) {
		mm_set_pad_light_enabled(state, i, enabled);
	}
}

bool mm_get_cabinet_light_enabled(MinimaidLightsState *state, MinimaidCabinetLights light) {
	return (state->raw_light_data[_CabinetLightMappings[light].index] & _CabinetLightMappings[light].offset);
}

bool mm_get_pad_light_enabled(MinimaidLightsState *state, MinimaidPadLights light) {
	return (state->raw_light_data[_PadLightMappings[light].index] & _PadLightMappings[light].offset);
}

/* Input */

static unsigned byte_to_offset(char byte) {
	if (byte == 0) return 0;

	unsigned offset;
	for (offset = 0; !((byte >> offset) & 0x1); offset++);

	return offset;
}

static unsigned int64_to_offset(int64_t integer) {
	if (integer == 0) return 0;

	unsigned offset;
	for (offset = 0; !((integer >> offset) & (uint64_t)0x1); offset++);

	return offset;
}

uint64_t mm_get_current_keyfield(Minimaid *c) {
	uint64_t keys = 0;
	int bytes_transferred = libusb_control_transfer(
		c->dev_handle,
		CONTROL_REQUEST_TYPE_IN,
		HID_GET_REPORT,
		(HID_REPORT_TYPE_INPUT << 8) | 0x00,
		INPUT_INTERFACE,
		(unsigned char*)&keys,
		sizeof(keys),
		IO_TIMEOUT_MS
	);

	return keys;
}

MinimaidInputState mm_get_input_state(Minimaid *c) {
	return mm_get_current_keyfield(c);
}

bool mm_get_input_enabled(MinimaidInputState state, MinimaidInput input) {
	return (state & _InputMapping[input]);
}

