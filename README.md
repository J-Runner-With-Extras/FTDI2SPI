# What is this

FTDI2SPI used in J-Runner with Extras for the xFlasher.

## How to compile

Visual Studio:

FTDI2SPI -> Properties -> C/C++ -> Preprocessor -> Preprocessor Definitions -> add "_CRT_SECURE_NO_WARNINGS"

Build -> Build Solution

CMake:

x86-64

`cmake -G "Visual Studio 18 2026" -A x64 -B build -S .`
`cmake --build build`

x86-32

`cmake -G "Visual Studio 18 2026" -A win32 -B build -S .`
`cmake --build build`