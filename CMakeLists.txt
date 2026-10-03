cmake_minimum_required(VERSION 3.20)
project(FTDI2SPI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Select FTD2XX library per target architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    set(FTD2XX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/ftd2xx-winarm")
    set(FTD2XX_LIB "${FTD2XX_DIR}/arm64/FTD2XX.lib")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(FTD2XX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/ftd2xx-win64")
    set(FTD2XX_LIB "${FTD2XX_DIR}/amd64/ftd2xx.lib")
else()
    # x86 (Win32 / 32-bit)
    set(FTD2XX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/ftd2xx-win32")
    set(FTD2XX_LIB "${FTD2XX_DIR}/i386/ftd2xx.lib")
endif()
set(FTD2XX_HEADER "${FTD2XX_DIR}/ftd2xx.h")

set(SOURCES
    src/Flasher.cpp
    src/ISD2100.cpp
    src/Main.cpp
    src/XNAND.cpp
    src/XSPI.cpp
    src/XeMMC.cpp
    src/sfc.cpp
    src/stdafx.cpp
    src/wrapper_spi.cpp
)

set(HEADERS
    include/Flasher.h
    include/ISD2100.h
    include/XNAND.h
    include/XSPI.h
    include/XeMMC.h
    include/sfc.h
    include/stdafx.h
    include/wrapper_spi.h
    ${FTD2XX_HEADER}
)

add_library(FTDI2SPI SHARED ${SOURCES} ${HEADERS})

target_compile_definitions(FTDI2SPI PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _MBCS
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(FTDI2SPI PRIVATE -Wno-c++11-narrowing -Wno-narrowing)
endif()

target_include_directories(FTDI2SPI PRIVATE
    include
    extern
    ${FTD2XX_DIR}
)

target_link_libraries(FTDI2SPI PRIVATE
    ${FTD2XX_LIB}
)

