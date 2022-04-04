# ExtraChain Console Client

## Software stack

* Qt 6.2+
* CMake 3.21+
* vcpkg
* Compilers:
  * Windows: MSVC 2019 or MSVC/Clang 9+
  * Ubuntu or Linux: Clang 9+
  * Android: NDK 21 (Clang 9)
  * MacOS or iOS: Apple Clang

## Installation
1. First, install vcpkg. 

Clone:

    git clone https://github.com/Microsoft/vcpkg.git


2. Install some dependencies:

For Ubuntu:

    sudo apt install autoconf curl zip unzip tar libgmp-dev ninja-build clang

3. And install vcpkg:

Windows:

    .\vcpkg\bootstrap-vcpkg.bat
    cd vcpkg

or Unix:

    ./vcpkg/bootstrap-vcpkg.sh
    cd vcpkg

4. Install packages.

For Windows x64:

    .\vcpkg install libsodium:x64-windows sqlite3:x64-windows mpir:x64-windows boost:x64-windows

and install integrate:

    .\vcpkg integrate install

or Unix:

    ./vcpkg install libsodium sqlite3 boost

If arm, before:

	export VCPKG_FORCE_SYSTEM_BINARIES=arm

6. Build project.

## IDE Settings
### CMake
Use something like:

    -DCMAKE_PREFIX_PATH=%YOUR QT PATH%/lib/cmake -DCMAKE_TOOLCHAIN_FILE=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake

### Qt Creator
Open Tools → Options → Kits → %Your kit% → CMake Configuration → Change..., add CMAKE_TOOLCHAIN_FILE and save:

    CMAKE_TOOLCHAIN_FILE:STRING=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake

### Visual Studio Code
Use CMake extension and create file **.vscode/settings.json**:

    {
        "cmake.configureArgs": [
            "-DCMAKE_PREFIX_PATH=%YOUR QT PATH%/lib/cmake",
            "-DCMAKE_TOOLCHAIN_FILE=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake"
        ]
    }
