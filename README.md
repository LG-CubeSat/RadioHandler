# RadioHandler

Compile on macOS using an Alpine Linux container. The image includes GCC,
CMake, Ninja, Linux headers, GDB, and the pigpio v79 client library.

On a fresh Mac, install and start the Docker runtime once:

```sh
brew install docker colima
colima start --runtime docker --vm-type vz --cpu 2 --memory 2 --disk 20
```

From this repository:

```sh
git submodule update --init --recursive
./scripts/alpine build   # compile to build/alpine/libRadioHandler.a
./scripts/alpine         # open an Alpine bash shell in /work
```

The helper builds the compiler image automatically, reusing Docker's cache.
The repo is mounted at `/work`, so edits and build output persist on your Mac.
Containers are removed when the command or shell exits. To rebuild just the
image, run `./scripts/alpine image`. Other commands also work, for example
`./scripts/alpine cc --version`.

RadioHandler needs `radio_deframer`, `radio_queue`, and CCSDS sources from the
consuming project. If `../FlightSoftwareV1.1` exists, the helper mounts it
read-only at `/support` and the build command supplies those sources to CMake.
To use that checkout from another location:

```sh
RADIOHANDLER_SUPPORT_ROOT=/path/to/FlightSoftwareV1.1 ./scripts/alpine build
```

For a different support layout, open the shell and configure CMake with
`RADIOHANDLER_SUPPORT_SOURCES` and `RADIOHANDLER_SUPPORT_INCLUDE_DIRS` pointing
to your sources inside the container. Extra arguments to `./scripts/alpine build`
are passed to CMake and override its defaults.

This produces a native Linux static library (ARM64 on Apple Silicon). Alpine
uses musl libc; deployment to Raspberry Pi OS or another glibc system needs a
matching target toolchain. Running the radio requires the Pi's GPIO/UART hardware.

Stop the Linux VM with `colima stop`; start it again with `colima start`.
