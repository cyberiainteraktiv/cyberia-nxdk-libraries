#ifndef IGR_SDL3_H
#define IGR_SDL3_H

#include <stdint.h>
#include <SDL3/SDL.h>

#ifndef IGR_REBOOT_DELAY_MS
#define IGR_REBOOT_DELAY_MS 250
#endif

#ifndef IGR_SCREENSHOT_DIR
#define IGR_SCREENSHOT_DIR "E:\\Screenshots"
#endif

#ifndef IGR_SCREENSHOT_PREFIX
#define IGR_SCREENSHOT_PREFIX "shot"
#endif

#ifndef IGR_PNG_COMPRESSION_LEVEL
#define IGR_PNG_COMPRESSION_LEVEL 2
#endif

#ifndef IGR_JPG_QUALITY
#define IGR_JPG_QUALITY 100
#endif

typedef enum IGR_ImageFormat
{
	IGR_FORMAT_PNG = 0,
	IGR_FORMAT_JPG,
	IGR_FORMAT_BMP
} IGR_ImageFormat;

typedef enum IGR_Button
{
	IGR_BTN_A             = 0x0,
	IGR_BTN_B             = 0x1,
	IGR_BTN_X             = 0x2,
	IGR_BTN_Y             = 0x3,
	IGR_BTN_BLACK         = 0x4,
	IGR_BTN_WHITE         = 0x5,
	IGR_BTN_LEFT_TRIGGER  = 0x6,
	IGR_BTN_RIGHT_TRIGGER = 0x7,
	IGR_BTN_DPAD_UP       = 0x8,
	IGR_BTN_DPAD_DOWN     = 0x9,
	IGR_BTN_DPAD_LEFT     = 0xA,
	IGR_BTN_DPAD_RIGHT    = 0xB,
	IGR_BTN_START         = 0xC,
	IGR_BTN_BACK          = 0xD,
	IGR_BTN_LEFT_THUMB    = 0xE,
	IGR_BTN_RIGHT_THUMB   = 0xF,
	IGR_BTN_COUNT         = 16
} IGR_Button;

typedef enum IGR_Action
{
	IGR_ACTION_NONE = 0,
	IGR_ACTION_DASH,
	IGR_ACTION_GAME,
	IGR_ACTION_FULL,
	IGR_ACTION_CYCLE,
	IGR_ACTION_SHUTDOWN,
	IGR_ACTION_SCREEN
} IGR_Action;

typedef struct IGR_Config
{
	int master_port;
	uint32_t dash_mask;
	uint32_t game_mask;
	uint32_t full_mask;
	uint32_t cycle_mask;
	uint32_t shutdown_mask;
	uint32_t screen_mask;
} IGR_Config;

typedef struct IGR_StateSDL3
{
	int triggered;
	IGR_Action matched_action;
	IGR_Config config;
	IGR_ImageFormat screenshot_format;
	int jpg_quality;
	unsigned int screenshot_index;
	char last_screenshot_path[260];
} IGR_StateSDL3;

void IGR_SDL3_Init(IGR_StateSDL3 *state);
void IGR_SDL3_SetScreenshotFormat(IGR_StateSDL3 *state, IGR_ImageFormat format);
int IGR_SDL3_Update(IGR_StateSDL3 *state);
int IGR_SDL3_UpdateWithScreenshot(
	IGR_StateSDL3 *state,
	SDL_Renderer *renderer,
	char *outPathBuf,
	unsigned int outPathBufSize);
int IGR_SDL3_SaveImage(
	IGR_StateSDL3 *state,
	SDL_Renderer *renderer,
	const char *outPath,
	IGR_ImageFormat format);
int IGR_SDL3_SavePNG(
	IGR_StateSDL3 *state,
	SDL_Renderer *renderer,
	const char *outPath);
int IGR_SDL3_CaptureScreenshot(
	IGR_StateSDL3 *state,
	SDL_Renderer *renderer,
	char *outPathBuf,
	unsigned int outPathBufSize);
void IGR_SDL3_Reboot(void);

#endif
