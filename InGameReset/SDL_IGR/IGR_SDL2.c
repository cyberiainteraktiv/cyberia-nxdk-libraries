#include "IGR_SDL2.h"

#include <hal/debug.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <SDL.h>
#include <nxdk/mount.h> // TODO: Read Cerbios controller configuration by mounting C:/Cerbios/Cerbios.ini, then E:/Cerbios/Cerbios.ini

void IGR_SDL2_Init(IGR_StateSDL2 *state)
{
	if (!state)
		return;
	state->triggered = 0;
}

int IGR_SDL2_Update(IGR_StateSDL2 *state)
{
	int numJoysticks;

	if (!state)
		return 0;

	if (state->triggered)
		return 1;

	numJoysticks = SDL_NumJoysticks();
	for (int i = 0; i < numJoysticks; i++)
	{
		SDL_GameController *pad;
		float lt, rt;
		Uint8 back, start;

		if (!SDL_IsGameController(i))
			continue;

		pad = SDL_GameControllerOpen(i);
		if (!pad)
			continue;

		lt = (float)SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f;
		rt = (float)SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f;
		back = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK);
		start = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);

		SDL_GameControllerClose(pad);

		if (back && start && lt > 0.0f && rt > 0.0f)
		{
			state->triggered = 1;
			break;
		}
	}

	return state->triggered;
}

void IGR_SDL2_Reboot(void)
{
	debugPrint("IGR triggered. Shutting down SDL2 and rebooting...\n");
	SDL_Quit();
	Sleep(IGR_REBOOT_DELAY_MS);
	HalReturnToFirmware(HalQuickRebootRoutine);
}
