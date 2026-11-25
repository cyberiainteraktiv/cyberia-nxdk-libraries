// CYBERIA:INTERAKTIV - NXDK IGR library by Rachel Bartmoss (2026)
// TODO: Add SDL screenshot feature

#include "IGR_SDL2.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <SDL.h>
#include <nxdk/mount.h>

#define CERBIOS_INI_E "E:\\Cerbios\\Cerbios.ini"
#define CERBIOS_INI_C "C:\\Cerbios\\Cerbios.ini" // Fallback for Cerbios versions before v3.0

static IGR_Action g_last_action = IGR_ACTION_NONE;

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

static uint32_t IGR_ReadGamepadMask(SDL_GameController *pad)
{
	if (!pad)
		return 0;

	const struct { SDL_GameControllerButton sdl_btn; IGR_Button igr_btn; } btns[] = {
		{ SDL_CONTROLLER_BUTTON_A,             IGR_BTN_A },
		{ SDL_CONTROLLER_BUTTON_B,             IGR_BTN_B },
		{ SDL_CONTROLLER_BUTTON_X,             IGR_BTN_X },
		{ SDL_CONTROLLER_BUTTON_Y,             IGR_BTN_Y },
		{ SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, IGR_BTN_BLACK },
		{ SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  IGR_BTN_WHITE },
		{ SDL_CONTROLLER_BUTTON_DPAD_UP,       IGR_BTN_DPAD_UP },
		{ SDL_CONTROLLER_BUTTON_DPAD_DOWN,     IGR_BTN_DPAD_DOWN },
		{ SDL_CONTROLLER_BUTTON_DPAD_LEFT,     IGR_BTN_DPAD_LEFT },
		{ SDL_CONTROLLER_BUTTON_DPAD_RIGHT,    IGR_BTN_DPAD_RIGHT },
		{ SDL_CONTROLLER_BUTTON_START,         IGR_BTN_START },
		{ SDL_CONTROLLER_BUTTON_BACK,          IGR_BTN_BACK },
		{ SDL_CONTROLLER_BUTTON_LEFTSTICK,     IGR_BTN_LEFT_THUMB },
		{ SDL_CONTROLLER_BUTTON_RIGHTSTICK,    IGR_BTN_RIGHT_THUMB },
	};

	uint32_t mask = 0;
	for (size_t i = 0; i < sizeof(btns) / sizeof(btns[0]); ++i)
	{
		if (SDL_GameControllerGetButton(pad, btns[i].sdl_btn))
			mask |= (1u << btns[i].igr_btn);
	}

	if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 0)
		mask |= (1u << IGR_BTN_LEFT_TRIGGER);
	if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0)
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

void IGR_SDL2_Init(IGR_StateSDL2 *state)
{
	if (!state)
		return;

	state->triggered = 0;
	state->matched_action = IGR_ACTION_NONE;
	g_last_action = IGR_ACTION_NONE;

	if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
		SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);

	IGR_SetDefaultConfig(&state->config);
	IGR_MountPartitions();

	if (IGR_LoadIniFile(CERBIOS_INI_E, &state->config))
		return;
	else if (IGR_LoadIniFile(CERBIOS_INI_C, &state->config))
		return;
	else
		IGR_ApplyMissingIniFallback(&state->config);
}

int IGR_SDL2_Update(IGR_StateSDL2 *state)
{
	if (!state)
		return 0;

	if (state->triggered)
		return 1;

	SDL_GameControllerUpdate();

	int count = SDL_NumJoysticks();
	for (int i = 0; i < count; i++)
	{
		if (!SDL_IsGameController(i))
			continue;

		SDL_GameController *pad = SDL_GameControllerOpen(i);
		if (!pad)
			continue;

		uint32_t mask = IGR_ReadGamepadMask(pad);
		SDL_GameControllerClose(pad);

		IGR_Action action = IGR_MatchAction(&state->config, mask);
		if (action != IGR_ACTION_NONE)
		{
			state->matched_action = action;
			g_last_action = action;
			state->triggered = 1;
			break;
		}
	}

	return state->triggered;
}

void IGR_SDL2_Reboot(void)
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
