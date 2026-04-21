# Generic Modular C++ Template (Framework + App)

This repo is split into two layers:

- **Framework** (`src/Framework/`): reusable infrastructure (platform loop, runtime registries, service locator, input router, event bus, panel host)
- **Application** (`src/App/`, `src/Features/`, `src/Modes/`): your project code built on top of the framework

## Rename once, use everywhere

Edit **`cmake/AppMetadata.cmake`**.

That drives:
- CMake project/target name
- generated header: `Core/Runtime/AppConfig.h` (window title, version, etc.)

## Common extension points

- Add a new **Feature**: `src/Features/<YourFeature>/...`
- Add a new **Mode/Tool**: `src/Modes/<YourMode>/...`
- Register them in: `src/App/DefaultModules/DefaultModules.cpp`

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

On Windows (Visual Studio generator), select your configuration and run the produced exe in `build/bin/`.

## Portable Zip

Build a portable Windows zip with:

```bash
cmake -S . -B build
cmake --build build --config Release --target portable
```

The packaged zip is written to `build/portable/`.

On Windows, the main app now builds as a GUI executable, so launching it does not open a separate console window.

To add an app icon, place a multi-size `.ico` file at `assets/app/icon.ico`, then rerun configure/build. The icon is embedded into the Windows executable automatically.
