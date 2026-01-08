Put the macOS Raylib library here.

If you downloaded the file from the Raylib GitHub releases, it is likely a "Fat Binary" (Universal Static Library) that contains code for both Intel (x86_64) and Apple Silicon (ARM64).

1. Copy 'lib/libraylib.a' to 'boats/lib/raylib_macos/lib/libraylib.a'
2. Copy 'include/raylib.h' (and others) to 'boats/lib/raylib_macos/include/'

You can verify if the library supports both architectures by opening a terminal in the lib folder and running:
   lipo -info libraylib.a
   
It should say: "Architectures in the fat file: libraylib.a are: x86_64 arm64"