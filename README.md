# ExtraChain Console Client

## Software stack

* Qt 6.3+
* CMake 3.21+
* vcpkg
* Compilers:
  * Windows: MSVC 2019 or MSVC/Clang 11+
  * Ubuntu: Clang 11+ or GCC 11+
  * Android: NDK 22 (Clang 11)
  * MacOS or iOS: Apple Clang

## Installation
1. First, install vcpkg. 

Clone:

    git clone https://github.com/Microsoft/vcpkg.git


2. Install some dependencies:

For Ubuntu:

    sudo apt install autogen autoconf libtool curl zip unzip tar ninja-build pkg-config

3. And install vcpkg:

Windows:

    cd vcpkg
    .\bootstrap-vcpkg.bat

or Unix:

    cd vcpkg
    ./bootstrap-vcpkg.sh

4. Install packages.

For Windows x64:

    .\vcpkg install libsodium sqlite3 mpir boost-system boost-thread boost-variant boost-interprocess fmt magic-enum --triplet x64-windows

and install integrate:

    .\vcpkg integrate install

or Unix:

    ./vcpkg install libsodium sqlite3 gmp boost-system boost-thread boost-variant boost-interprocess fmt magic-enum

If Linux ARM, before:

	export VCPKG_FORCE_SYSTEM_BINARIES=arm

6. Build project.

## IDE Settings
### CMake
Use something like:

    -DCMAKE_PREFIX_PATH=%YOUR QT PATH%/lib/cmake -DCMAKE_TOOLCHAIN_FILE=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake

### Qt Creator
Open Tools → Options → Kits → %Your kit% → CMake Configuration → Change..., add CMAKE_TOOLCHAIN_FILE and save:

    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake

### Visual Studio Code
Use CMake extension and create file **.vscode/settings.json**:

    {
        "cmake.configureArgs": [
            "-DCMAKE_PREFIX_PATH=%YOUR QT PATH%/lib/cmake",
            "-DCMAKE_TOOLCHAIN_FILE=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake"
        ]
    }
