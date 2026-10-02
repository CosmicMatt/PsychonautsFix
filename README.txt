========================================================================
Psychonauts Fix (PsychoControllerFix v1.0)
========================================================================

WHAT THIS MOD FIXES:

1. Completely fixes the endlessly spinning camera issue caused by 
   Psychonauts' legacy DirectInput engine misinterpreting XInput triggers (LT/RT)
   as continuous camera rotation inputs.
2. Adds radial stick deadzones to eliminate camera drift.
3. DirectInput to Xinput passthrough for seamless plug-and-play support for Xbox, PlayStation (DualShock/DualSense),
   Switch Pro, and Steam Deck controllers via standard XInput/SteamInput.
4. In-memory patch to render clean Xbox UI button prompts in tutorials and menus.
5. Enables borderless window for easy alt-tabbing and no more screen flickering.
6. Fixes a performance issue with higher resolutions when Vsync enabled (should get up to an additional 20fps on certain systems)


HOW TO INSTALL (2 EASY STEPS):

1. Open your Psychonauts game installation directory.
   - On Steam: Right-click Psychonauts in your Steam Library -> Manage -> Browse local files.
   - (Typical path: C:\Program Files (x86)\Steam\steamapps\common\Psychonauts)

2. Copy 'dinput8.dll' and 'PsychoControllerFix.ini' directly into the main 
   Psychonauts folder next to 'Psychonauts.exe'.

That's it! Launch Psychonauts and enjoy seamless controller gameplay and borderless window.


CONFIGURATION (PsychoControllerFix.ini):

You can open 'PsychoControllerFix.ini' with Notepad to tweak:
- DeadzoneLeftStick (default 0.15)
- DeadzoneRightStick (default 0.15)
- CameraSensitivity (default 1.0)
- InvertRightStickX / InvertRightStickY (default false)
========================================================================
