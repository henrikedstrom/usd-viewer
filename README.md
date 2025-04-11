# Simple USD Viewer

A trivial viewer for USD files. By default, it opens the Kitchen Set, but you can drag and drop USD/USZ files onto the viewer to load them. (Note: Only tested with a limited set of USD files.)

## Platforms Supported

- **Windows:** x64 and ARM64
- **macOS**

## Requirements

- **CMake:** Tested with version 3.31.6.
- **Python:** Tested with version 3.13.1 (do _not_ install the optional Python debug libraries, as they may cause issues when building OpenUSD).
- **Windows:** Visual Studio (2019/2022). Tested with VS2022.
- **macOS:** Xcode 15 (or later) including command-line tools. Tested with AppleClang 17.0.0.

## Building from the Command Line

The build process is similar across platforms. Open a command prompt (on Windows, use a Visual Studio x64 or ARM64 native command prompt) in the project’s root folder (where `CMakeLists.txt` is located) and run:

### Debug Build
```
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug .
cmake --build build_debug --config Debug
```

### Release Build
```
cmake -B build_release -DCMAKE_BUILD_TYPE=Release .
cmake --build build_release --config Release
```

## Running

### Windows x64 / ARM
Open `USDViewer.sln` in the corresponding build folder, select configuration (`Debug`/`Release`) and run.

### macOS
Run the executable directly from the build folder:
```
./USDViewer
```
The executable’s RPATH is set so it should be able to locate the OpenUSD dylibs automatically.
