# AITSnap

[中文说明](README.zh-CN.md)

AITSnap is an open-source desktop application project for assembling screenshots
and associating them with test execution records.

## Status

Repository initialization only. The source contains a minimal Qt Widgets window;
the product capabilities below are planned and are not implemented. There is no
release, installer, or validated platform support yet.

## Product direction

- Capture multiple regions across multiple monitors.
- Arrange captures freely on an OBS-style composition canvas.
- Automatically archive captures and related records into Word documents.
- Associate captures with commands and sequence execution.
- Provide a Windows system tray entry point.
- Develop reusable components with boundaries between UI and future capabilities.

This repository contains only the software skeleton. It contains no test data,
command files, or organization-specific materials.

## Technology and target systems

- C++20, Qt Widgets, and CMake 3.16 or newer.
- Qt 5.15 is the initial dependency baseline, using `Qt5::Widgets`.
- Windows 7 SP1 and Windows 10/11 are target systems, not verified support claims.
- Windows 7 SP1 compatibility and a suitable compiler, Qt 5.15 build, runtime,
  and deployment combination remain to be validated. C++20 compiler support alone
  does not establish Windows 7 runtime compatibility.
- Linux remains a possible future port; no Linux validation is provided.

## Build

Use a C++20 compiler and a matching Qt 5.15 development kit. Configure the Qt kit
path locally; do not commit machine-specific paths. For example, from the
repository root with Ninja available:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="<qt-kit-prefix>"
cmake --build build
```

The placeholder is the Qt kit installation prefix, not its `bin` directory.
The compiler and Qt kit must be ABI-compatible. With a multi-configuration
generator, select the desired configuration using `cmake --build build --config Debug`.
On Windows, Qt runtime DLLs and the platform plugin must be available to run the
executable. Packaging and runtime deployment are outside this initialization.

See [initial validation](docs/initial-validation.md) for the actual local result
and remaining checks. A successful configure or compiler probe is not a completed
application build or platform qualification.

## Source layout

```text
CMakeLists.txt        Build definition
src/main.cpp          Application entry point
src/main_window.*     Minimal window placeholder
docs/                 Initialization validation record
```

## License

AITSnap source is licensed under [Apache-2.0](LICENSE), using the
[official Apache license text](https://www.apache.org/licenses/LICENSE-2.0.txt).
Qt is an external dependency and retains its own license; it is not relicensed
by this repository. No Qt binaries or other third-party dependencies are bundled.
