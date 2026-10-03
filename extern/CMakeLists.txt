cmake_minimum_required(VERSION 3.20)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build libusb static library from source
add_subdirectory(libusb)

if(WIN32)
    target_sources(usb-1.0 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/libusb/libusb/os/windows_usbdk.c")
    include(CheckCSourceCompiles)
    check_c_source_compiles("#include <time.h>
int main(void) { struct timespec ts; return sizeof(ts) == 0; }"
        LIBUSB_HAVE_STRUCT_TIMESPEC)
    if(LIBUSB_HAVE_STRUCT_TIMESPEC)
        target_compile_definitions(usb-1.0 PRIVATE HAVE_STRUCT_TIMESPEC=1)
    endif()
endif()

# Provide variables so libftdi uses the in-tree libusb
set(LIBUSB_FOUND TRUE CACHE BOOL "" FORCE)
set(LIBUSB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libusb/libusb" CACHE PATH "" FORCE)
set(LIBUSB_LIBRARIES usb-1.0 CACHE STRING "" FORCE)

# Configure libftdi options to build libftdi static/shared library only
set(STATICLIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTDIPP OFF CACHE BOOL "" FORCE)
set(FTDI_EEPROM OFF CACHE BOOL "" FORCE)
set(PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)

# Build libftdi from source
add_subdirectory(libftdi)

if(WIN32)
    set_target_properties(ftdi1-static PROPERTIES OUTPUT_NAME "ftdi1-static")
    set_target_properties(ftdi1 PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    foreach(ftdi_target IN ITEMS ftdi1 ftdi1-static)
        if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            target_compile_options(${ftdi_target} PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/compat/windows_ftdi.h")
        else()
            target_compile_options(${ftdi_target} PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/compat/windows_ftdi.h")
        endif()
    endforeach()
endif()
