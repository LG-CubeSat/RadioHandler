# RadioHandler

The radio driver builds natively on Linux and macOS. Both platforms share the
transmit queue, CCSDS framing, receive deframer, resend history, and statistics.

## Native macOS build

Install the Xcode command line tools and CMake, then build:

```sh
git submodule update --init --recursive
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Debug
cmake --build build/macos --parallel
ctest --test-dir build/macos --output-on-failure
```

This produces `build/macos/libRadioHandler.a` and its dependencies. When using
this repository with CMake's `add_subdirectory`, link your application against
`RadioHandler::RadioHandler` to include those dependencies automatically.
The tests use a virtual serial port and do not require radio hardware.

Connect the E22 through a USB serial adapter and set its device path before
starting the application that calls `radio_send` / `radio_receive`:

```sh
export E22_UART_DEVICE=/dev/cu.usbserial-YOUR_DEVICE
./your-radio-application
```

The macOS driver opens the serial port at 9600 baud, 8N1, with no flow control.
It requires a radio already configured to match the peer, with M0 and M1 held
low for normal transparent mode. It does not program the E22's registers or
control AUX. To match the Linux driver's settings, configure the module with
address/network ID/key zero, REG0 `0x62`, REG1 `0x00`, channel `23`, and REG3
`0x03` (see `src/radio.c`). The Linux driver uses temporary settings at startup;
they do not persist across a module power cycle.

Without AUX feedback, macOS waits 1000 ms after each transmitted packet. Set
`E22_TX_DELAY_MS` to a value from 0 to 60000 to adjust that delay for your radio's
air rate; slower configurations may need a longer delay. Zero is useful for
virtual serial ports. Successful sends confirm delivery to the serial port;
they do not confirm over-the-air reception. `E22_SWAPPED_UART=1` requires the
Linux/pigpio driver and is rejected on macOS.

## Linux build

```sh
git submodule update --init --recursive
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux --parallel
```

The Linux hardware driver requires pigpio's `pigpiod_if2` headers and library.
It controls the Pi GPIO pins, configures the E22 at startup, and defaults to
`/dev/serial0`. `E22_UART_DEVICE` can override that serial device. Swapped UART
operation still uses `E22_SWAPPED_UART=1` and the pigpio daemon.
