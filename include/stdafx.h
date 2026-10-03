// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#define FTDI2SPI_EXPORT __declspec(dllexport)
#else
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>

#if defined(__GNUC__) || defined(__clang__)
#define FTDI2SPI_EXPORT __attribute__((visibility("default")))
#else
#define FTDI2SPI_EXPORT
#endif

// FTDI's native ftd2xx.h/WinTypes.h supplies the driver types (BYTE,
// DWORD, etc.). Do not redefine them here: their ABI is platform specific.
// No TCHAR APIs are currently used by this project.

inline void Sleep(std::uint32_t milliseconds)
{
    if (milliseconds == 0)
        std::this_thread::yield();
    else
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

constexpr std::size_t _TRUNCATE = static_cast<std::size_t>(-1);
constexpr int STRUNCATE = 80;

// Buffer-size overload used for FTDI device descriptions. As with the CRT,
// source and destination must not overlap. Invalid inputs return errno values
// instead of invoking Microsoft's invalid-parameter handler.
inline int strncpy_s(char* destination, std::size_t capacity,
                     const char* source, std::size_t count)
{
    if (count == 0 && destination == nullptr && capacity == 0)
        return 0;
    if (destination == nullptr || capacity == 0)
        return errno = EINVAL;
    if (count == 0)
    {
        destination[0] = '\0';
        return 0;
    }
    if (source == nullptr)
    {
        destination[0] = '\0';
        return errno = EINVAL;
    }

    std::size_t length = 0;
    while (length < capacity - 1 && length < count && source[length] != '\0')
        ++length;

    const bool truncated = length < count && source[length] != '\0';
    if (truncated && count != _TRUNCATE)
    {
        destination[0] = '\0';
        return errno = ERANGE;
    }
    std::memcpy(destination, source, length);
    destination[length] = '\0';
    return truncated ? STRUNCATE : 0;
}
#endif
