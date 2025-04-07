cmake_minimum_required(VERSION 3.13)
project(USDViewer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use FetchContent to download header-only and external dependencies
include(FetchContent)

# Fetch glm (header-only)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# Fetch glfw
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw
  GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# Include ExternalProject to fetch and build OpenUSD
include(ExternalProject)

# If CMAKE_BUILD_TYPE is not set, default to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Set the USD build variant and installation directory based on the build type
if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(USD_BUILD_VARIANT "debug")
  set(USD_INSTALL_DIR ${CMAKE_BINARY_DIR}/OpenUSD_Install_Debug)
else()
  set(USD_BUILD_VARIANT "release")
  set(USD_INSTALL_DIR ${CMAKE_BINARY_DIR}/OpenUSD_Install_Release)
endif()

message(STATUS "USD Build Variant: ${USD_BUILD_VARIANT}")
message(STATUS "USD Install Directory: ${USD_INSTALL_DIR}")

# ExternalProject_Add to clone and build OpenUSD
ExternalProject_Add(OpenUSD
  GIT_REPOSITORY    https://github.com/PixarAnimationStudios/OpenUSD.git
  GIT_TAG           dev                     # Use the dev branch (or specify a commit/tag)
  CONFIGURE_COMMAND ""                      # USD uses a Python build script
  BUILD_COMMAND     python build_scripts/build_usd.py --build-variant ${USD_BUILD_VARIANT} --usd-imaging --onetbb --no-python -vvv ${USD_INSTALL_DIR}
  INSTALL_COMMAND   ""                      # The build script installs USD into USD_INSTALL_DIR
  UPDATE_COMMAND    ""                      # Disable automatic updates
  BUILD_IN_SOURCE   1                       # Run the build command in the source directory
  LOG_DOWNLOAD      ON
  LOG_BUILD         ON
)

# Add source and header files
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

# Add the executable target
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})

# Ensure that your project builds only after OpenUSD is built
add_dependencies(${PROJECT_NAME} OpenUSD)

# Organize files in IDEs
source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})

# Project include directory
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Set USD_ROOT to the installation directory of OpenUSD
set(USD_ROOT ${USD_INSTALL_DIR})
message(STATUS "USD_ROOT set to: ${USD_ROOT}")

# Add USD include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${USD_ROOT}/include)

# Add the USD library directory to the linker search path
target_link_directories(${PROJECT_NAME} PRIVATE "${USD_ROOT}/lib")

# Add USD libraries
set(USD_LIBS_COMMON
  "${USD_ROOT}/lib/usd_usd.lib"
  "${USD_ROOT}/lib/usd_sdf.lib"
  "${USD_ROOT}/lib/usd_tf.lib"
  "${USD_ROOT}/lib/usd_vt.lib"
  "${USD_ROOT}/lib/usd_gf.lib"
  "${USD_ROOT}/lib/usd_usdGeom.lib"
  "${USD_ROOT}/lib/usd_usdImaging.lib"
  "${USD_ROOT}/lib/usd_usdImagingGL.lib"
  "${USD_ROOT}/lib/usd_glf.lib"
  "${USD_ROOT}/lib/usd_work.lib"
)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(USD_LIBS ${USD_LIBS_COMMON} "${USD_ROOT}/lib/tbb12_debug.lib")
else()
  set(USD_LIBS ${USD_LIBS_COMMON} "${USD_ROOT}/lib/tbb12.lib")
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${USD_LIBS})

# Misc dependencies: include and link for GLAD, glm, and glfw
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/external/glad/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm glfw)

# Set the working directory for IDE debuggers
set_target_properties(${PROJECT_NAME} PROPERTIES 
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Set the runtime PATH for Visual Studio to include the USD bin and lib directories
set_target_properties(${PROJECT_NAME} PROPERTIES
  VS_DEBUGGER_ENVIRONMENT "PATH=${USD_ROOT}/bin;${USD_ROOT}/lib;%PATH%"
)

# Set USDViewer as the startup project for Visual Studio
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

