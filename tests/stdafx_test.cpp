#include "stdafx.h"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>

int main()
{
    char buffer[4] = {};
    assert(strncpy_s(buffer, sizeof(buffer), "abc", _TRUNCATE) == 0);
    assert(std::strcmp(buffer, "abc") == 0);
    assert(strncpy_s(buffer, sizeof(buffer), "abcde", _TRUNCATE) == STRUNCATE);
    assert(std::strcmp(buffer, "abc") == 0);
    assert(strncpy_s(buffer, sizeof(buffer), "abcde", 2) == 0);
    assert(std::strcmp(buffer, "ab") == 0);
    assert(strncpy_s(buffer, sizeof(buffer), "", _TRUNCATE) == 0);
    assert(buffer[0] == '\0');
    char single[1] = {'x'};
    assert(strncpy_s(single, sizeof(single), "a", _TRUNCATE) == STRUNCATE);
    assert(single[0] == '\0');

#ifndef _WIN32
    // The shim returns errors; the Microsoft CRT may invoke its invalid-parameter handler.
    assert(strncpy_s(buffer, sizeof(buffer), "abcd", 4) == ERANGE);
    assert(buffer[0] == '\0');
    assert(strncpy_s(buffer, sizeof(buffer), nullptr, 1) == EINVAL);
    assert(buffer[0] == '\0');
    assert(strncpy_s(nullptr, 0, nullptr, 0) == 0);
    assert(strncpy_s(nullptr, 4, "a", 1) == EINVAL);
    assert(strncpy_s(buffer, 0, "a", 1) == EINVAL);
    assert(strncpy_s(buffer, sizeof(buffer), nullptr, 0) == 0);
#endif

    const auto start = std::chrono::steady_clock::now();
    Sleep(20);
    assert(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(20));
    Sleep(0);
}
