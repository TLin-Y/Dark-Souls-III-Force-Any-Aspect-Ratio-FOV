# Dark-Souls-III-Force-Any-Aspect-Ratio-FOV
Dark-Souls-III-Force-Any-Aspect-Ratio-FOV

# !!!!!!!!!! Do not use online play with this mod !!!!!!!!!!

# Special Thanks @ellekz: 
- fix the fullscreen mode https://www.nexusmods.com/darksouls3/mods/1545

-----------------------------------------------------

This mod allows you to play Dark Souls III at any custom resolution (like 4:3 for handhelds) without black bars, stretched UI, or broken camera angles. 

## STEP 1: 
Put the mod files (the .dll and .ini) into your Dark Souls III "Game" folder (where DarkSoulsIII.exe is located).
- Open the mod's .ini file (e.g., DS3_ARFix.ini) with Notepad you can customize any resolution or FOV.


## STEP 2: For Linux/Mac/Android Only
If you are playing on Windows, you are done! Just launch the game. 

### FOR ARMADA / GAMENATIVE / WINLATOR (ANDROID EMULATORS):
1. Open your container settings.
2. Find the "Environment Variables" section.
3. Add a new variable:
   - Name: WINEDLLOVERRIDES
   - Value: dinput8,d3d11=n,b

### FOR STEAM DECK / STEAMOS (LINUX):
1. In Desktop Mode, go to Dark Souls III in your Steam Library.
2. Click the "Gear" icon (Settings) -> Properties -> General.
3. Scroll down to "Launch Options" and paste this exact text:
   WINEDLLOVERRIDES="dinput8,d3d11=n,b" %command%
