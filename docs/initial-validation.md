# Initialization validation

Date: 2026-10-31 (Asia/Shanghai). Scope: repository initialization on the current
Windows development host. No additional toolchain was installed.

## Results

| Check | Result |
| --- | --- |
| Git / GitHub CLI | Available: Git 2.55.0.windows.3, GitHub CLI 2.100.0 |
| Build tools | CMake 3.29.2, Ninja 1.12.0, GCC 13.2.0 (MinGW-w64 UCRT) |
| C++20 compiler probe | Passed compilation and execution with `-std=c++20 -Wall -Wextra -Werror`; exercised `std::array`, `std::span`, and the C++20 language-version macro |
| Application CMake configure | Blocked at `find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)`: `Qt5Config.cmake` / `qt5-config.cmake` was not found |
| Qt application compilation / linking | Not run because configuration was blocked |
| Qt application launch / visual check | Not run |
| Windows 7 SP1 / Qt 5.15 compatibility | Pending |
| Windows 10/11 application compatibility | Pending |
| Linux | Future port only; not tested |

The application configuration was attempted with:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

The probe, binary, CMake cache, and raw logs remain in the ignored local `build/`
directory. They are not source files or release artifacts. The compiler probe
does not compile any Qt application source and does not qualify any target OS.

## Publication scope

Only the build definition, minimal application source, license, repository
configuration, and public documentation belong in the initial commit. No
internal materials, real test records, sensitive command files, credentials,
machine-specific paths, or bundled dependencies are included.

## Remaining validation

- Configure, compile, link, and launch using a matching C++20 compiler and Qt 5.15
  development kit.
- Verify Windows 7 SP1 compatibility, including compiler, Qt, runtime libraries,
  and deployment dependencies; verify Windows 10/11 separately.
- Check packaging and runtime deployment before producing any release.

Screenshot capture, canvas composition, Word archiving, execution association,
and system tray integration remain unimplemented product work outside this
initialization.
