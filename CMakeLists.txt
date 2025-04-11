cmake_minimum_required(VERSION 3.13)
project(USDViewer VERSION 1.0.0 LANGUAGES C CXX)

# ------------------------------------------------------------------------------
# Project Configuration
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# If CMAKE_BUILD_TYPE is not set (for single-configuration generators), default to Release.
if(NOT CMAKE_CONFIGURATION_TYPES)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  endif()
endif()

# ------------------------------------------------------------------------------
# External Dependencies via FetchContent
# ------------------------------------------------------------------------------
include(FetchContent)

# glm (header-only math library)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# glfw (windowing)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# ------------------------------------------------------------------------------
# OpenUSD: ExternalProject
# ------------------------------------------------------------------------------
include(ExternalProject)

# Determine USD build variant and install directory
# If the user passed -DCMAKE_BUILD_TYPE (even on multi-config generators),
# use that value; otherwise, fall back to the USD_DEFAULT_BUILD_VARIANT option.
if(DEFINED CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "")
  message(STATUS "Using CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
  if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(USD_BUILD_VARIANT "debug")
    set(USD_INSTALL_DIR ${CMAKE_BINARY_DIR}/OpenUSD_Install_Debug)
  else()
    set(USD_BUILD_VARIANT "release")
    set(USD_INSTALL_DIR ${CMAKE_BINARY_DIR}/OpenUSD_Install_Release)
  endif()
else()
  if(CMAKE_CONFIGURATION_TYPES)
    option(USD_DEFAULT_BUILD_VARIANT "Default build variant for OpenUSD in multi-config generators (debug or release)" "release")
    set(USD_BUILD_VARIANT ${USD_DEFAULT_BUILD_VARIANT})
    message(STATUS "Using USD_DEFAULT_BUILD_VARIANT: ${USD_BUILD_VARIANT}")
    if(USD_BUILD_VARIANT STREQUAL "debug")
      set(USD_INSTALL_DIR ${CMAKE_BINARY_DIR}/OpenUSD_Install_Debug)
    else()
      set(USD_INSTALL_DIR ${CMAKE_BINARY_DIR}/OpenUSD_Install_Release)
    endif()
  else()
    # Fallback for single-config with no CMAKE_BUILD_TYPE.
    set(USD_BUILD_VARIANT "release")
    set(USD_INSTALL_DIR ${CMAKE_BINARY_DIR}/OpenUSD_Install_Release)
  endif()
endif()

message(STATUS "USD Build Variant: ${USD_BUILD_VARIANT}")
message(STATUS "USD Install Directory: ${USD_INSTALL_DIR}")

# Set Python executable based on platform.
if(APPLE)
  set(PYTHON_EXECUTABLE "python3")
else()
  set(PYTHON_EXECUTABLE "python")
endif()

ExternalProject_Add(OpenUSD
  GIT_REPOSITORY    https://github.com/PixarAnimationStudios/OpenUSD.git
  GIT_TAG           dev                     # Use the dev branch (or specify a commit/tag)
  CONFIGURE_COMMAND ""                      # USD uses a Python build script
  BUILD_COMMAND     ${PYTHON_EXECUTABLE} build_scripts/build_usd.py --build-variant ${USD_BUILD_VARIANT} --usd-imaging --onetbb --no-python -vvv ${USD_INSTALL_DIR}
  INSTALL_COMMAND   ""                      # The build script installs USD into USD_INSTALL_DIR
  UPDATE_COMMAND    ""                      # Disable automatic updates
  BUILD_IN_SOURCE   1                       # Run the build command in the source directory
  LOG_DOWNLOAD      ON
  LOG_BUILD         ON
)

# ------------------------------------------------------------------------------
# Source and Header Files
# ------------------------------------------------------------------------------
set(SOURCE_FILES
  src/application.cpp
  src/camera.cpp
  src/main.cpp
  src/orbit_controls.cpp
  external/glad/src/glad.c
)

set(HEADER_FILES
  src/application.h
  src/camera.h
  src/orbit_controls.h
  src/usd_headers.h
)

# ------------------------------------------------------------------------------
# Executable Target Setup
# ------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})
add_dependencies(${PROJECT_NAME} OpenUSD)  # Ensure OpenUSD is built before our target.

# Organize files for IDEs.
source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# ------------------------------------------------------------------------------
# USD Setup: Include and Library Directories
# ------------------------------------------------------------------------------
set(USD_ROOT ${USD_INSTALL_DIR})
message(STATUS "USD_ROOT set to: ${USD_ROOT}")

target_include_directories(${PROJECT_NAME} PRIVATE ${USD_ROOT}/include)
target_link_directories(${PROJECT_NAME} PRIVATE "${USD_ROOT}/lib")

# ------------------------------------------------------------------------------
# Library Linking Setup: USD and Dependencies
# ------------------------------------------------------------------------------

# Determine platform-specific library prefix and suffix.
if(WIN32)
  set(LIB_PREFIX "")
  set(LIB_SUFFIX ".lib")
else()  # APPLE (macOS)
  set(LIB_PREFIX "lib")
  set(LIB_SUFFIX ".dylib")
endif()

# USD libraries common to all platforms.
set(COMMON_USD_LIBS
    "usd_usd"
    "usd_sdf"
    "usd_tf"
    "usd_vt"
    "usd_gf"
    "usd_hd"
    "usd_hgi"
    "usd_usdGeom"
    "usd_usdImaging"
    "usd_hdSt"
    "usd_usdImagingGL"
    "usd_hgiInterop"
    "usd_glf"
    "usd_work"
)

# Define platform-specific TBB library names.
if(WIN32)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")  ## Windows ARM64
    set(TBB_LIBS "tbb" "tbbmalloc" "tbb12")
  else()  # Windows x64
    set(TBB_LIBS "tbb" "tbbmalloc" "tbbmalloc_proxy" "tbb12")
  endif()
else()  # macOS
  set(TBB_LIBS "tbb" "tbbmalloc" "tbbmalloc_proxy")
endif()

# Combine the lists.
set(USD_BASE_LIBS ${COMMON_USD_LIBS} ${TBB_LIBS})

# For TBB libraries, in Debug mode append the '_debug' suffix.
set(TBB_DEBUG_LIBS ${TBB_LIBS})

# Build the full paths for USD libraries.
set(USD_LIBS_COMMON "")
foreach(lib ${USD_BASE_LIBS})
  set(current_lib_name ${lib})
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # If this library is one of the TBB libraries, append _debug.
    list(FIND TBB_DEBUG_LIBS ${lib} index)
    if(NOT index EQUAL -1)
      set(current_lib_name "${lib}_debug")
    endif()
  endif()
  list(APPEND USD_LIBS_COMMON "${USD_ROOT}/lib/${LIB_PREFIX}${current_lib_name}${LIB_SUFFIX}")
endforeach()

set(USD_LIBS ${USD_LIBS_COMMON})

# For macOS, set the RPATH so the executable can find the USD libraries at runtime.
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "${USD_ROOT}/lib")
endif()

# Link USD libraries to the target.
target_link_libraries(${PROJECT_NAME} PRIVATE ${USD_LIBS})

# ------------------------------------------------------------------------------
# Misc Dependencies: GLAD, glm, glfw, etc.
# ------------------------------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external/glad/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm glfw)

# ------------------------------------------------------------------------------
# IDE Specific Settings
# ------------------------------------------------------------------------------
set_target_properties(${PROJECT_NAME} PROPERTIES 
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(WIN32)
  # Visual Studio specific:
  set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_ENVIRONMENT "PATH=${USD_ROOT}/bin;${USD_ROOT}/lib;%PATH%"
  )
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()
