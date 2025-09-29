cmake_minimum_required(VERSION 3.16)
# Root CMakeLists: set cross-compilation context BEFORE any languages are enabled
# so that the subdirectory's project() picks up arm-none-eabi correctly.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Do not enable C/CXX here; subproject does that with its own project() call.
project(XCORE407I_SUPER LANGUAGES NONE)

add_subdirectory(targets/TARGET_XCORE407I)

# Pin map verification helper target (optional if Python3 present)
find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
	add_custom_target(verify_pin_maps
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/verify_pin_maps.py
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Verifying pin map CSV files are synchronized with board_xcore407i.h"
	)
else()
	message(STATUS "Python3 not found; skipping verify_pin_maps helper target")
endif()
