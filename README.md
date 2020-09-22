# Sega VR Emulation
This Sega Genesis emulator features Sega VR support. The emulator itself is based on some ancient DGen/SDL 1.33 source code, and my Sega VR support specifically lives in source/segavr with tendrils in a few places. There's support for emulating the HMD's motion tracking via analog controller input, and a few different stereoscopic view modes. Lots of adjustable options are exposed via config file.

Normally, I only use this codebase for local testing and experimentation, but the Sega VR support warranted putting the code up. There happens to be a bunch of unrelated additions/changes over the stock DGen/SDL 1.33 source in here, and I've been building it in Visual Studio 2015 exclusively, so it seems likely that the non-Windows platforms and build targets were broken at some point.

The only Sega VR title that this emulator's support has been tested on is Nuclear Rush, because as of this writing, it's the only Sega VR title known to exist in the wild. My buildable repository for that game is located here: https://github.com/DickBlackshack/SegaVR-NuclearRush

## License
See source/COPYING for the spiderweb of licensing/copyright information.
