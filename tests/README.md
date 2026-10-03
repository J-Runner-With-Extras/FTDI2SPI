# Portability checks

Run the header's string-copy and sleep checks without an FTDI device or SDK:

```sh
c++ -std=c++14 -Wall -Wextra -Werror -pthread -Iinclude tests/stdafx_test.cpp -o /tmp/ftdi2spi-stdafx-test
/tmp/ftdi2spi-stdafx-test
```

The same source compiles on Windows using the native Windows APIs and CRT.
The invalid-input checks are non-Windows only because the Microsoft CRT can
invoke an invalid-parameter handler for those inputs.

`stdafx.h` uses the Windows SDK on Windows and C++14 shims elsewhere. Driver
types come from `ftd2xx.h`, which requires the native SDK's `WinTypes.h` on
non-Windows platforms. No TCHAR functions are currently used, so there is no
non-Windows TCHAR emulation.

The top-level CMake configuration still selects the bundled Windows D2XX
libraries. Building the entire library on Linux or macOS additionally requires
selecting the target platform's D2XX headers and library in the build setup.
