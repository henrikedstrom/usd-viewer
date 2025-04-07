# Simple USD Viewer

A trivial viewer for USD files. By default, it opens the Kitchen Set, but you can drag and drop USD/USZ files onto the viewer to load them. (Note: Only tested with a limited set of USD files.)

## Requirements

- **CMake:** Tested with version 3.31.6.
- **Python:** Tested with version 3.13.1 (do _not_ install the optional Python debug libraries, as they may cause issues when building OpenUSD).
- **Visual Studio:** Only Visual Studio is supported at this time.

## Building from the Command Line

Open a Visual Studio x64 or ARM64 native command prompt, navigate to the root folder of this project (where the `CMakeLists.txt` file is located), and run one or both of the following. Note that this will download and build OpenUSD and the build step usually takes a few minutes.

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

Open `USDViewer.sln` in the corresponding build folder, select configuration (`Debug`/`Release`) and run.
