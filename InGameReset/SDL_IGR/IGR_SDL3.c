// CYBERIA:INTERAKTIV - NXDK IGR library by Rachel Bartmoss (2026)

#include "IGR_SDL3.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hal/debug.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <SDL3/SDL.h>
#include <nxdk/mount.h>

#include "libs/stb_image_write.h"

#define CERBIOS_INI_E "E:\\Cerbios\\Cerbios.ini"
#define CERBIOS_INI_C "C:\\Cerbios\\Cerbios.ini" // Fallback for Cerbios versions before v3.0

extern int render_get_framebuffer_rgba(uint8_t **out_pixels, int *out_w, int *out_h, int *out_pitch);

static IGR_Action g_last_action = IGR_ACTION_NONE;
static int g_screen_combo_held = 0;

static void IGR_StbiWriteFunc(void *context, void *data, int size)
{
	FILE *fp = (FILE *)context;
	if (fp && data && size > 0)
	{
		fwrite(data, 1, (size_t)size, fp);
	}
}

static const char *IGR_GetFormatExtension(IGR_ImageFormat format)
{
	switch (format)
	{
		case IGR_FORMAT_JPG:
			return "jpg";
		case IGR_FORMAT_BMP:
			return "bmp";
		case IGR_FORMAT_PNG:
		default:
			return "png";
	}
}

static uint32_t IGR_ParseComboHex(const char *val)
{
	uint32_t mask = 0;
	if (!val)
		return 0;

	for (const char *p = val; *p; ++p)
	{
		char c = (char)toupper((unsigned char)*p);
		int nibble = -1;

		if (c >= '0' && c <= '9')
			nibble = c - '0';
		else if (c >= 'A' && c <= 'F')
			nibble = 10 + (c - 'A');

		if (nibble >= 0 && nibble < IGR_BTN_COUNT)
			mask |= (1u << nibble);
	}
	return mask;
}

static void IGR_SetDefaultConfig(IGR_Config *cfg)
{
	cfg->master_port   = 1;
	cfg->dash_mask     = IGR_ParseComboHex("67CD");
	cfg->game_mask     = IGR_ParseComboHex("467C");
	cfg->full_mask     = IGR_ParseComboHex("467D");
	cfg->cycle_mask    = IGR_ParseComboHex("4678");
	cfg->shutdown_mask = IGR_ParseComboHex("678D");
	cfg->screen_mask   = IGR_ParseComboHex("EF");
}

static void IGR_ApplyMissingIniFallback(IGR_Config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->game_mask = IGR_ParseComboHex("67CD");
	cfg->screen_mask = IGR_ParseComboHex("EF");
}

static void IGR_CleanString(char *str)
{
	if (!str)
		return;

	for (char *p = str; *p; ++p)
	{
		if (*p == ';' || *p == '#')
		{
			*p = '\0';
			break;
		}
	}

	while (*str && isspace((unsigned char)*str))
		memmove(str, str + 1, strlen(str));

	size_t len = strlen(str);
	while (len > 0 && isspace((unsigned char)str[len - 1]))
		str[--len] = '\0';
}

static void IGR_ApplyIniEntry(IGR_Config *cfg, const char *key, const char *val)
{
	typedef struct { const char *name; uint32_t *mask; } ComboEntry;
	const ComboEntry combos[] = {
		{ "IGRDash",     &cfg->dash_mask },
		{ "IGRGame",     &cfg->game_mask },
		{ "IGRFull",     &cfg->full_mask },
		{ "IGRCycle",    &cfg->cycle_mask },
		{ "IGRShutdown", &cfg->shutdown_mask },
		{ "IGRScreen",   &cfg->screen_mask },
	};

	if (_stricmp(key, "IGRMasterPort") == 0)
	{
		int port = atoi(val);
		if (port >= 0 && port <= 4)
			cfg->master_port = port;
		return;
	}

	for (size_t i = 0; i < sizeof(combos) / sizeof(combos[0]); ++i)
	{
		if (_stricmp(key, combos[i].name) == 0)
		{
			*combos[i].mask = IGR_ParseComboHex(val);
			return;
		}
	}
}

static int IGR_LoadIniFile(const char *path, IGR_Config *cfg)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return 0;

	char line[512];
	while (fgets(line, sizeof(line), fp))
	{
		IGR_CleanString(line);
		if (line[0] == '\0' || line[0] == '[')
			continue;

		char *eq = strchr(line, '=');
		if (!eq)
			continue;

		*eq = '\0';
		char *key = line;
		char *val = eq + 1;
		IGR_CleanString(key);
		IGR_CleanString(val);

		if (*key && *val)
			IGR_ApplyIniEntry(cfg, key, val);
	}

	fclose(fp);
	return 1;
}

static void IGR_MountPartitions(void)
{
	const struct { char drive; const char *device; } mounts[] = {
		{ 'E', "\\Device\\Harddisk0\\Partition1\\" },
		{ 'C', "\\Device\\Harddisk0\\Partition2\\" },
	};

	for (size_t i = 0; i < sizeof(mounts) / sizeof(mounts[0]); ++i)
	{
		nxMountDrive(mounts[i].drive, mounts[i].device);
	}
}

static int IGR_EnsureScreenshotDirReady(void)
{
	if (GetFileAttributesA("E:\\") == INVALID_FILE_ATTRIBUTES)
	{
		if (FAILED(nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\")))
		{
			return 0;
		}
	}

	CreateDirectoryA(IGR_SCREENSHOT_DIR, NULL);
	if (GetFileAttributesA(IGR_SCREENSHOT_DIR) == INVALID_FILE_ATTRIBUTES)
	{
		return 0;
	}

	return 1;
}

static uint32_t IGR_ReadGamepadMask(SDL_Gamepad *pad)
{
	if (!pad)
		return 0;

	const struct { SDL_GamepadButton sdl_btn; IGR_Button igr_btn; } btns[] = {
		{ SDL_GAMEPAD_BUTTON_SOUTH,          IGR_BTN_A },
		{ SDL_GAMEPAD_BUTTON_EAST,           IGR_BTN_B },
		{ SDL_GAMEPAD_BUTTON_WEST,           IGR_BTN_X },
		{ SDL_GAMEPAD_BUTTON_NORTH,          IGR_BTN_Y },
		{ SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, IGR_BTN_BLACK },
		{ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  IGR_BTN_WHITE },
		{ SDL_GAMEPAD_BUTTON_DPAD_UP,        IGR_BTN_DPAD_UP },
		{ SDL_GAMEPAD_BUTTON_DPAD_DOWN,      IGR_BTN_DPAD_DOWN },
		{ SDL_GAMEPAD_BUTTON_DPAD_LEFT,      IGR_BTN_DPAD_LEFT },
		{ SDL_GAMEPAD_BUTTON_DPAD_RIGHT,     IGR_BTN_DPAD_RIGHT },
		{ SDL_GAMEPAD_BUTTON_START,          IGR_BTN_START },
		{ SDL_GAMEPAD_BUTTON_BACK,           IGR_BTN_BACK },
		{ SDL_GAMEPAD_BUTTON_LEFT_STICK,     IGR_BTN_LEFT_THUMB },
		{ SDL_GAMEPAD_BUTTON_RIGHT_STICK,    IGR_BTN_RIGHT_THUMB },
	};

	uint32_t mask = 0;
	for (size_t i = 0; i < sizeof(btns) / sizeof(btns[0]); ++i)
	{
		if (SDL_GetGamepadButton(pad, btns[i].sdl_btn))
			mask |= (1u << btns[i].igr_btn);
	}

	if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0)
		mask |= (1u << IGR_BTN_LEFT_TRIGGER);
	if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0)
		mask |= (1u << IGR_BTN_RIGHT_TRIGGER);

	return mask;
}

static IGR_Action IGR_MatchAction(const IGR_Config *cfg, uint32_t pressed_mask)
{
	if (pressed_mask == 0)
		return IGR_ACTION_NONE;

	const struct { uint32_t mask; IGR_Action action; } rules[] = {
		{ cfg->dash_mask,     IGR_ACTION_DASH },
		{ cfg->game_mask,     IGR_ACTION_GAME },
		{ cfg->full_mask,     IGR_ACTION_FULL },
		{ cfg->cycle_mask,    IGR_ACTION_CYCLE },
		{ cfg->shutdown_mask, IGR_ACTION_SHUTDOWN },
		{ cfg->screen_mask,   IGR_ACTION_SCREEN },
	};

	for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); ++i)
	{
		if (rules[i].mask && pressed_mask == rules[i].mask)
			return rules[i].action;
	}

	return IGR_ACTION_NONE;
}

static IGR_Action IGR_PollCombinedAction(const IGR_Config *cfg, int *out_screen_held)
{
	if (out_screen_held)
		*out_screen_held = 0;

	int count = 0;
	SDL_JoystickID *ids = SDL_GetGamepads(&count);
	if (!ids)
		return IGR_ACTION_NONE;

	IGR_Action matched = IGR_ACTION_NONE;

	for (int i = 0; i < count; i++)
	{
		SDL_Gamepad *pad = SDL_OpenGamepad(ids[i]);
		if (!pad)
			continue;

		uint32_t mask = IGR_ReadGamepadMask(pad);
		SDL_CloseGamepad(pad);

		if (cfg->screen_mask && mask == cfg->screen_mask && out_screen_held)
			*out_screen_held = 1;

		IGR_Action action = IGR_MatchAction(cfg, mask);
		if (action != IGR_ACTION_NONE && matched == IGR_ACTION_NONE)
		{
			matched = action;
		}
	}

	SDL_free(ids);
	return matched;
}

void IGR_SDL3_Init(IGR_StateSDL3 *state)
{
	if (!state)
		return;

	state->triggered = 0;
	state->matched_action = IGR_ACTION_NONE;
	state->screenshot_format = IGR_FORMAT_PNG;
	state->jpg_quality = IGR_JPG_QUALITY;
	state->screenshot_index = 1;
	state->last_screenshot_path[0] = '\0';
	g_last_action = IGR_ACTION_NONE;
	g_screen_combo_held = 0;

	IGR_SetDefaultConfig(&state->config);
	IGR_MountPartitions();

	if (IGR_LoadIniFile(CERBIOS_INI_E, &state->config))
		return;
	else if (IGR_LoadIniFile(CERBIOS_INI_C, &state->config))
		return;
	else
		IGR_ApplyMissingIniFallback(&state->config);
}

void IGR_SDL3_SetScreenshotFormat(IGR_StateSDL3 *state, IGR_ImageFormat format)
{
	if (!state)
		return;

	state->screenshot_format = format;
}

int IGR_SDL3_SaveImage(
	IGR_StateSDL3 *state,
	SDL_Renderer *renderer,
	const char *outPath,
	IGR_ImageFormat format)
{
	if (!outPath || !outPath[0])
		return 0;

	if (!IGR_EnsureScreenshotDirReady())
		return 0;

	SDL_Surface *rgba = NULL;

	if (renderer)
	{
		SDL_Surface *raw = SDL_RenderReadPixels(renderer, NULL);
		if (!raw)
		{
			return 0;
		}

		rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
		SDL_DestroySurface(raw);
		if (!rgba)
		{
			return 0;
		}
	}
	else
	{
		uint8_t *fb_pixels = NULL;
		int w = 0, h = 0, pitch = 0;

		if (!render_get_framebuffer_rgba(&fb_pixels, &w, &h, &pitch))
		{
			return 0;
		}

		SDL_Surface *raw = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ARGB8888, fb_pixels, pitch);
		if (!raw)
		{
			return 0;
		}

		rgba = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32);
		SDL_DestroySurface(raw);
		if (!rgba)
		{
			return 0;
		}
	}

	for (int y = 0; y < rgba->h; ++y)
	{
		uint8_t *row = (uint8_t *)rgba->pixels + (size_t)y * rgba->pitch;
		for (int x = 0; x < rgba->w; ++x)
			row[x * 4 + 3] = 0xFF;
	}

	FILE *fp = fopen(outPath, "wb");
	if (!fp)
	{
		SDL_DestroySurface(rgba);
		return 0;
	}

	int ok = 0;

	switch (format)
	{
		case IGR_FORMAT_BMP:
			ok = stbi_write_bmp_to_func(
				IGR_StbiWriteFunc,
				fp,
				rgba->w,
				rgba->h,
				4,
				rgba->pixels);
			break;

		case IGR_FORMAT_JPG:
		{
			int quality = (state && state->jpg_quality > 0 && state->jpg_quality <= 100)
				? state->jpg_quality
				: IGR_JPG_QUALITY;
			ok = stbi_write_jpg_to_func(
				IGR_StbiWriteFunc,
				fp,
				rgba->w,
				rgba->h,
				4,
				rgba->pixels,
				quality);
			break;
		}

		case IGR_FORMAT_PNG:
		default:
			stbi_write_png_compression_level = IGR_PNG_COMPRESSION_LEVEL;
			ok = stbi_write_png_to_func(
				IGR_StbiWriteFunc,
				fp,
				rgba->w,
				rgba->h,
				4,
				rgba->pixels,
				rgba->pitch);
			break;
	}

	fclose(fp);
	SDL_DestroySurface(rgba);

	if (!ok)
	{
		return 0;
	}

	return 1;
}

int IGR_SDL3_SavePNG(IGR_StateSDL3 *state, SDL_Renderer *renderer, const char *outPath)
{
	return IGR_SDL3_SaveImage(state, renderer, outPath, IGR_FORMAT_PNG);
}

int IGR_SDL3_CaptureScreenshot(
	IGR_StateSDL3 *state,
	SDL_Renderer *renderer,
	char *outPathBuf,
	unsigned int outPathBufSize)
{
	unsigned int idx = (state && state->screenshot_index > 0) ? state->screenshot_index : 1;
	IGR_ImageFormat fmt = state ? state->screenshot_format : IGR_FORMAT_PNG;
	const char *ext = IGR_GetFormatExtension(fmt);
	char filePath[260];

	snprintf(
		filePath,
		sizeof(filePath),
		"%s\\%s_%04u.%s",
		IGR_SCREENSHOT_DIR,
		IGR_SCREENSHOT_PREFIX,
		idx,
		ext);

	if (!IGR_SDL3_SaveImage(state, renderer, filePath, fmt))
		return 0;

	if (state)
	{
		snprintf(state->last_screenshot_path, sizeof(state->last_screenshot_path), "%s", filePath);
		state->screenshot_index = idx + 1;
	}

	if (outPathBuf && outPathBufSize > 0)
	{
		snprintf(outPathBuf, outPathBufSize, "%s", filePath);
	}

	return 1;
}

int IGR_SDL3_Update(IGR_StateSDL3 *state)
{
	if (!state)
		return 0;

	if (state->triggered)
		return 1;

	int screen_held_now = 0;
	IGR_Action action = IGR_PollCombinedAction(&state->config, &screen_held_now);
	g_screen_combo_held = screen_held_now;

	if (action != IGR_ACTION_NONE && action != IGR_ACTION_SCREEN)
	{
		state->matched_action = action;
		g_last_action = action;
		state->triggered = 1;
	}

	return state->triggered;
}

int IGR_SDL3_UpdateWithScreenshot(
	IGR_StateSDL3 *state,
	SDL_Renderer *renderer,
	char *outPathBuf,
	unsigned int outPathBufSize)
{
	if (!state)
		return 0;

	if (state->triggered)
		return 1;

	int screen_held_now = 0;
	IGR_Action action = IGR_PollCombinedAction(&state->config, &screen_held_now);
	int screen_just_pressed = (screen_held_now && !g_screen_combo_held);
	g_screen_combo_held = screen_held_now;

	if (action == IGR_ACTION_SCREEN)
	{
		if (screen_just_pressed)
		{
			state->matched_action = IGR_ACTION_SCREEN;
			g_last_action = IGR_ACTION_SCREEN;
			return IGR_SDL3_CaptureScreenshot(state, renderer, outPathBuf, outPathBufSize);
		}
		return 0;
	}

	if (action != IGR_ACTION_NONE)
	{
		state->matched_action = action;
		g_last_action = action;
		state->triggered = 1;
	}

	return state->triggered;
}

void IGR_SDL3_Reboot(void)
{
	SDL_Quit();
	Sleep(IGR_REBOOT_DELAY_MS);

	switch (g_last_action)
	{
		case IGR_ACTION_SHUTDOWN:
			HalReturnToFirmware(HalHaltRoutine);
			break;

		case IGR_ACTION_DASH:
		case IGR_ACTION_GAME:
			HalReturnToFirmware(HalQuickRebootRoutine);
			break;

		case IGR_ACTION_CYCLE:
		case IGR_ACTION_FULL:
		default:
			HalReturnToFirmware(HalRebootRoutine);
			break;
	}
}
