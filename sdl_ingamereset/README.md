# SDL_InGameReset
A template for adding simple, native-feeling in-game reset and screenshot functionality to your SDL2 or SDL3 based NXDK projects, using Cerbios bindings first and a fallback quick reset using ```LT+RT+Start+Back``` for non-Cerbios setups.

## Usage
- Put "IGR_SDLx.h", "IGR_SDLx.c" and "lib/stb_image_write.h" in your project folder and add "IGR_SDLx.c" to your "Makefile". (x being your SDL version number)
- Add "-DSTBI_WRITE_NO_STDIO" to your C flags in the "Makefile". "IGR_SDLx.c" only includes "stb_image_write.h" as a header, so it won't clash if another file already defines the implementation.

Once video and SDL are up and running, initialize your state and pick an image format, ie;
```
#include "IGR_SDL3.h"

static IGR_StateSDL3 igr;

// In main(), after video & SDL3 are ready:
IGR_SDL3_Init(&igr);
IGR_SDL3_SetScreenshotFormat(&igr, IGR_FORMAT_PNG); // or IGR_FORMAT_JPG / IGR_FORMAT_BMP
igr.jpg_quality = 90; // optional: 1-100 for JPG
```

Put "IGR_SDLx_UpdateWithScreenshot" right after your frame finishes drawing, ie;
```
	while (!wants_to_exit) {
		pump_events();
		system_update();
		video_present();

		char shotPath[260] = {0};
		if (IGR_SDL3_UpdateWithScreenshot(&igr, NULL, shotPath, sizeof(shotPath)))
		{
			if (igr.matched_action == IGR_ACTION_SCREEN)
			{
				igr.triggered = 0;
				igr.matched_action = IGR_ACTION_NONE;
			}
			else
			{
				IGR_SDL3_Reboot();
			}
		}
	}
```
And you're set! Ensure that your SDL2/3 application has a valid exposed SDL_renderer to fetch screenshots from. If you're not using SDL_renderer by default, you can create a helper with your renderer of choice to send it's current frame to SDL_renderer to be captured.

TODO: Include example helper function to send pbkit data to SDL_renderer
