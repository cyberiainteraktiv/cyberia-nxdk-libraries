#include "IGR_SDL3.h"

#include <hal/debug.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <SDL3/SDL.h>
#include <nxdk/mount.h> // TODO: Read Cerbios controller configuration by mounting C:/Cerbios/Cerbios.ini, then E:/Cerbios/Cerbios.ini

void IGR_SDL3_Init(IGR_StateSDL3 *state)
{
	if (!state)
		return;
	state->triggered = 0;
}

int IGR_SDL3_Update(IGR_StateSDL3 *state)
{
	int count = 0;
	SDL_JoystickID *ids;

	if (!state)
		return 0;

	if (state->triggered)
		return 1;

	ids = SDL_GetGamepads(&count);
	if (ids)
	{
		for (int i = 0; i < count; i++)
		{
			SDL_Gamepad *pad = SDL_OpenGamepad(ids[i]);
			float lt, rt;
			bool back, start;

			if (!pad)
				continue;

			lt = (float)SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
			rt = (float)SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
			back = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK);
			start = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START);

			SDL_CloseGamepad(pad);

			if (back && start && lt > 0.0f && rt > 0.0f)
			{
				state->triggered = 1;
				break;
			}
		}
		SDL_free(ids);
	}

	return state->triggered;
}

void IGR_SDL3_Reboot(void)
{
	debugPrint("IGR triggered. Shutting down SDL3 and rebooting...\n");
	SDL_Quit();
	Sleep(IGR_REBOOT_DELAY_MS);
	HalReturnToFirmware(HalQuickRebootRoutine);
}
