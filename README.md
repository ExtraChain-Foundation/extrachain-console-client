# ExtraChain Console Client

## Software stack

* Qt 6.8
* CMake 3.29+
* vcpkg
* Compilers:
  * Windows: MSVC 2022 or MSVC/Clang
  * Ubuntu: Clang or GCC
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
    
For macOS 15:

    brew install pkg-config autoconf automake libtool

3. And install vcpkg:

Windows:

    cd vcpkg
    .\bootstrap-vcpkg.bat

or Unix:

    cd vcpkg
    ./bootstrap-vcpkg.sh

4. Install packages.

For Windows x64:

    .\vcpkg install libsodium sqlite3 boost-system boost-thread boost-variant boost-interprocess boost-multiprecision boost-asio boost-mp11 boost-describe boost-json msgpack fmt magic-enum hash-library cpp-base64 blake3 --triplet x64-windows

and install integrate:

    .\vcpkg integrate install

or Unix:

    ./vcpkg install libsodium sqlite3 boost-system boost-thread boost-variant boost-interprocess boost-multiprecision boost-asio boost-mp11 boost-describe boost-json msgpack fmt magic-enum hash-library cpp-base64 blake3

If Linux ARM, before:

	export VCPKG_FORCE_SYSTEM_BINARIES=arm

6. Build project.

## Thoth provisioning

Use `--create-thoth-dictionary` to create the current `ThothDevicesV2` dictionary. The operation is idempotent. The console does not create or read the legacy `Thoth` vector.

## IDE Settings
### CMake
Use something like:

    -DCMAKE_PREFIX_PATH=%YOUR QT PATH%/lib/cmake -DCMAKE_TOOLCHAIN_FILE=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake

### Qt Creator
Open Preferences → Kits → %Your kit% → CMake Configuration → Change..., add CMAKE_TOOLCHAIN_FILE and save:

    -DCMAKE_TOOLCHAIN_FILE:FILEPATH=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake

### Visual Studio Code
Use CMake extension and create file **.vscode/settings.json**:

    {
        "cmake.configureArgs": [
            "-DCMAKE_PREFIX_PATH=%YOUR QT PATH%/lib/cmake",
            "-DCMAKE_TOOLCHAIN_FILE=%YOUR VCPKG PATH%/scripts/buildsystems/vcpkg.cmake"
        ]
    }



#API
## /balance

Get the balance of a specified actor.
Method: POST

Parameters
    •    actor_id (string) – Actor identifier.
    •    token (string) – Session token for authorization.

Example request
{
  "actor_id": "00a50acbd15a8d6be81e8f93becaa6138dfe55dd",
  "token": "26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c"
}

Example response
{
  "actor_id": "00a50acbd15a8d6be81e8f93becaa6138dfe55dd",
  "balance": "100"
}

## /transaction_by_hash_and_section_id

Search for a transaction in a section by its hash.
Method: POST

Parameters
    •    hash (string) – Transaction hash to search for.
    •    section_id (integer) – Section identifier where the search will be performed.
    •    token (string) – Session token for authorization.

Example request
{
  "hash": "f7b123abc456def...",
  "section_id": 42,
  "token": "26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c"
}

Example response
{
  "hash": "f7b123abc456def...",
  "sender": "00a50acbd15a8d6be81e8f93becaa6138dfe55dd",
  "receiver": "88a50acbd15a8d6be81e8f93becaa6138dfe55d8",
  "amount": "1000",
  "date": "09/09/2025",
  "time": "15:45:12",
  "type": "regular"
}

## count_sections

Get the total number of sections.
Method: GET

Parameters
    •    token (string) – Session token for authorization.

Example request
/count_sections?token=26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c

Example response
{
  "count_sections": "42"
}

## /count_transactions_in_section

Get the number of transactions in a section.
Method: GET

Parameters
    •    number_section (integer) – Section identifier.
    •    token (string) – Session token for authorization.

Example request
/count_transactions_in_section?number_section=42&token=26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c

Example response
{
  "count_transactions": 125,
  "section_number": "42"
}

## /have_rewards

Check if an actor has transactions of type Reward within a given period.
Method: POST

Parameters
    •    actor_id (string) – Actor identifier.
    •    token (string) – Session token for authorization.
    •    period (string, optional) – Time period for verification (e.g., "1d", "7d", "1h"). Default is "1d".

Example request
{
  "actor_id": "00a50acbd15a8d6be81e8f93becaa6138dfe55dd",
  "token": "26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c",
}

Example response
{
  "actor_id": "00a50acbd15a8d6be81e8f93becaa6138dfe55dd",
  "has_rewards": true,
  "reward_count": 5,
  "period_ms": 604800000,
  "period_str": "7d"
}

## /subscription_state

Get the subscription status of an actor to the RaccoonSubscription service.
Method: POST

Parameters
    •    actor_id (string) – Actor identifier.
    •    token (string) – Session token for authorization.

Example request
{
  "actor_id": "00a50acbd15a8d6be81e8f93becaa6138dfe55dd",
  "token": "26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c"
}

Example response
{
  "actor_id": "00a50acbd15a8d6be81e8f93becaa6138dfe55dd",
  "active": true,
  "subscribed": true
}

## /get_actor

Get actor public key.
Method: GET

Parameters
    •    id (string) – Actor ID identifier.
    •    token (string) – Session token for authorization.

Example request
/get_actor?id=c3659c28c0974df7c53cf752dfe6366e424f1b3d&token=26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c
Example response
{
    "public_key": "JHimQL6cb-V4fVld4gJuGN0Kk0S5yGnRwlXuB_nVruQ"
}

## /verify_actor

Verify that actor is valid.
Method: GET

Parameters
    •    id (string) – Actor ID identifier.
    •    signature (string) – Actor signature.
    •    token (string) – Session token for authorization.

Example request
/verify_actor?id=f6656942732dc8f9e4d3f385ff99b895aac70851&signature=LvfHAPhlNPPwX0CYqxJF57feqOSLptawjgQZkC-XYOIncQAIC5zRMK4PKOCoLF__pvW1UmNGiLVBvn8oJFmZCw&token=26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c
Example response
{
    "result": true
}

## /get_devices

Get info about devices.
Method: GET

Parameters
    •    token (string) – Session token for authorization.

Example request
/get_devices?token=26981bbf8819c458b971861591fdc5e3ecc0876e0e3742d8634d79680fc8e89c
Example response
{
    "result": [
        {
            "ram": 32768,
            "gpu_model": "NVIDIA GeForce RTX 4090",
            "ssd": true,
            "gpu_count": 1,
            "vram": 24576,
            "cpu_model": "AMD Ryzen 9 7950X",
            "cpu_cores": 16
        },
        {
            "ram": 262144,
            "gpu_model": "NVIDIA A100",
            "ssd": true,
            "gpu_count": 4,
            "vram": 40960,
            "cpu_model": "Intel Xeon Platinum 8380",
            "cpu_cores": 40
        },
        {
            "ram": 131072,
            "gpu_model": "AMD Radeon RX 7900 XTX",
            "ssd": true,
            "gpu_count": 2,
            "vram": 24576,
            "cpu_model": "AMD Ryzen Threadripper PRO 5995WX",
            "cpu_cores": 64
        },
        {
            "ram": 65536,
            "gpu_model": "NVIDIA RTX 6000 Ada",
            "ssd": true,
            "gpu_count": 1,
            "vram": 49152,
            "cpu_model": "Intel Core i9-14900K",
            "cpu_cores": 24
        },
        {
            "ram": 32768,
            "gpu_model": "NVIDIA GeForce RTX 4070 Ti",
            "ssd": true,
            "gpu_count": 1,
            "vram": 12288,
            "cpu_model": "Intel Core i7-13700K",
            "cpu_cores": 16
        }
    ]
}
