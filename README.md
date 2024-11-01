# ExtraChain Console Client

## Software stack

* Qt 6.7.3
* CMake 3.29+
* vcpkg
* Compilers:
  * Windows: MSVC 2022 or MSVC/Clang
  * Ubuntu: Clang 16+ or GCC 13+
  * Android: NDK 26 (Clang)
  * MacOS or iOS: Apple Clang

## Installation
1. First, install vcpkg. 

Clone:

    git clone https://github.com/Microsoft/vcpkg.git


2. Install some dependencies:

For Ubuntu 24.04:

    sudo apt install make git curl zip unzip tar pkg-config autoconf libtool
    sudo snap install cmake --classic
    bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"

3. And install vcpkg:

Windows:

    cd vcpkg
    .\bootstrap-vcpkg.bat

or Unix:

    cd vcpkg
    ./bootstrap-vcpkg.sh

4. Install packages.

For Windows x64:

    .\vcpkg install libsodium sqlite3 boost-system boost-thread boost-variant boost-interprocess boost-multiprecision boost-asio boost-filesystem boost-mp11 boost-describe boost-json msgpack fmt magic-enum hash-library cpp-base64 --triplet x64-windows

and install integrate:

    .\vcpkg integrate install

or Unix:

    ./vcpkg install libsodium sqlite3 boost-system boost-thread boost-variant boost-interprocess boost-multiprecision boost-asio boost-filesystem boost-mp11 boost-describe boost-json msgpack fmt magic-enum hash-library cpp-base64

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
