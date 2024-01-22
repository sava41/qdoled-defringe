# QD Oled Defringe
This app applies a screenwide filter that reduces the green/magenta fringe seen on samsung display qd oled panels. It works by using DLL injection on the desktop windows manager; similar to how ReShade works for video games. Code is based on [dwm_lut](https://github.com/lauralex/dwm_lut).
![before and after example photot](example.jpg?raw=true)

## [Download latest release](https://github.com/lauralex/dwm_lut/releases/latest/download/Release.zip)

## App Features
 - Run in taskbar
 - Filter strength adjustment (I like to run it at 0.5)
 - Per monitor control: If you have a multi-monitor setup with only one qd oled display, set filter strength for other monitors to 0.0

## Known Issues / Limitations
 - Only tested on windows 11 with aw3423dwf monitor.
 - Only works when monitor is operating in native resolution.
 - Filter causes screen to become slightly blury. I believe this can be reduced with a more advanced filter but I dont want to spend time on this feature.
 - May cause visual artifacts. For example on youtube, there may appear a thin green line under the video box.
 - Does not work in fullscreen.
 - Does not save settings. Will need to set settings after every launch.
 - DirectFlip and MPO get force disabled. These features are designed to improve performance for some windowed applications like video players.
 - May trigger antivirus.

 ## Building
**Requirements:**
  - CMake 3.19 or later
  - MSVC 2022
  - .NET 4.8

**Build Project:**
```bash
> mkdir build 
> cd build 
> cmake ..
> cmake --build . --config Release
```

 
