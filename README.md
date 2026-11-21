# RTS Lab Project - Part 2: Multi-Function Device

Second part of the Real-Time Systems lab project (IST, 2026/2027). A concurrent FreeRTOS application on the mbed LPC1768 (ARM Cortex-M3) plugged into the mbed Application Board. Part 1 was the PIC16F18875 monitoring system in [rts_project_part1](https://github.com/simaosilva0/rts_project_part1); part 2 doesn't use the PIC.

Group: 2
Members: Simão Silva (110486), Simão Silveira (110049), Isam Abbasi (122334)

Deadline: 18/12/2026. Deliver a ZIP with the sources, the final binary `frtos.bin` and a 3-4 page report on the real-time and communication/synchronization decisions. Demos and oral exams for both parts are in the week of 04/01/2027.

The spec is `materials/Project_part2.pdf`. Where this README and the spec disagree, the spec wins.


## What the device does

Each function below runs as its own FreeRTOS task (or group of tasks):

- Temperature monitoring (LM75B). It reads every `pmon` seconds and on demand from the console; `pmon = 0` turns off periodic reads but on-demand reads still work. Each reading goes to the LCD and to the RGB LED, gets checked against the max/min records (stored with an RTC timestamp) and raises an alarm if it's outside `[tlow, thigh]` and temperature alarms are on.
- Bubble level using the X and Y axes of the MMA7660 accelerometer, drawn on the right side of the LCD.
- RTC for date and time: timestamps for the records and an alarm clock.
- Alarms, signalled by the buzzer for `tala` seconds. The buzzer task also has a "config sound" mode where pot1 sets the PWM period and pot2 the duty cycle.
- Console over USB serial, built on the course's command interpreter.
- Hit Bit game: a bit pattern rotates across the 4 mbed LEDs and you press the joystick centre when a bit is on LED4. If LED4 was on, that bit is cleared, otherwise a new bit is added. Clearing every bit is a win, shown by blinking all 4 LEDs 3 times.

Bubble level, Hit Bit and config sound can each be switched on and off from the console. Shared devices (LCD, I2C bus, buzzer) need either a manager task or a mutex so tasks don't step on each other.

RGB LED colour: anything below `tlow` is pure blue, anything above `thigh` is pure red, and in between it shades across all three colours.

### LCD (C12832, 128x32)

```
hh:mm:ss
A: C T              O     <- right side: bubble level
T(C)= tt.t
```

`C` and `T` appear when the alarm clock or the temperature alarm is enabled.

### Console commands

| cmd | args | description |
|---|---|---|
| `rdt` | | read date/time (dd/MM/YYYY hh:mm:ss) |
| `sd` | d M Y | set date |
| `rc` | | read clock |
| `sc` | h m s | set clock |
| `rt` | | read temperature |
| `rmm` | | read temperature max and min |
| `cmm` | | clear temperature max and min |
| `rp` | | read parameters (pmon, tala) |
| `mmp` | p | modify monitoring period (0 = off) |
| `mta` | t | modify alarm duration |
| `rai` | | read alarm info (alarm clock, on/off, tlow, thigh, on/off) |
| `sac` | h m s | set alarm clock |
| `sat` | tl th | set temperature thresholds |
| `adac` | 1/0 | alarm clock on/off |
| `adat` | 1/0 | temperature alarm on/off |
| `rts` | | read task state (bubble level, Hit Bit, config sound) |
| `adbl` | 1/0 | bubble level task on/off |
| `adhb` | 1/0 | Hit Bit task on/off |
| `adcs` | 1/0 | config sound on/off |

Ranges: day 1-31, month 1-12, year 1970-2037, h 0-23, m and s 0-59, tl and th 0-50 °C, p 0-99 s, t 0-60 s.

### Initial values

| pmon | tala | tlow | thigh | ALAT | ALAC | ALAH:ALAM:ALAS | BL | HB | CS |
|---|---|---|---|---|---|---|---|---|---|
| 5 s | 3 s | 15 °C | 30 °C | 0 | 0 | 12:00:00 | 1 | 0 | 0 |


## Starting point: txCmdFRTOS

`txCmdFRTOS.zip` is the base project. It contains:

- `main.cpp`: two FreeRTOS tasks passing an `int32_t` through a queue, and the serial port set to 115200 baud. `vTask1` calls `monitor()`, which never returns, so that task is the console.
- `monitor.cpp`: the command interpreter (Carlos Almeida, IST). Commands live in the `commands[]` table as `{function, name, help}`. `sos` prints the help.
- `comando.cpp`: the command handlers (`cmd_test`, `cmd_send`, ...). Our commands go here.
- `freertos-cm3/`: FreeRTOS V9.0.0 for Cortex-M3. The manuals in `materials/` describe V10, so a few newer API calls won't be there.

The settings in `FreeRTOSConfig.h` that matter most:

| Setting | Value | Why it matters |
|---|---|---|
| `configTICK_RATE_HZ` | 1000 | 1 tick = 1 ms, so `vTaskDelay(1000)` waits 1 s |
| `configMAX_PRIORITIES` | 5 | priorities 0-4 |
| `configTOTAL_HEAP_SIZE` | 16 KB | all task stacks and queues share this |
| `configMINIMAL_STACK_SIZE` | 130 words | the example uses 2x for each task |
| `configUSE_MUTEXES` | 1 | mutexes are available |
| heap | `heap_1.c` | memory is never freed, so don't delete and recreate tasks. Suspend and resume them instead (relevant for `adbl`/`adhb`). |

The examples are built against the old mbed 2 library (`mbed.bld` points at `mbed-dev`), not mbed-os 6. That's why they use `Serial` and `wait()`.


## Other course materials

| File | What it is |
|---|---|
| `tstLM75B.zip` | Reads the LM75B and prints it on the LCD. Shows the pins: `C12832 lcd(p5, p7, p6, p8, p11)`, `LM75B sensor(p28, p27)` |
| `tstRTC.zip` | RTC example with the `RTC` library (`RTC.h/.cpp`): `set_time()`, per-second/minute callbacks and `RTC::alarm()` |
| `LM75B.zip`, `C12832.zip`, `MMA7660.zip` | Driver libraries for the temperature sensor, LCD (includes text and graphics: rectangle, circle, ...) and accelerometer |
| `ctime.c` | PC-side demo of `time`, `localtime_r`, `strftime` and `strptime`, handy for the `dd/MM/YYYY hh:mm:ss` format and for `sd`/`sc` |
| `README_LabSTR` | Lab PC setup (Windows, Miniconda, Mbed CLI 1) and serial settings |
| `FreeRTOS_Reference_Manual_V10.0.0.pdf` | API reference |
| `161204_Mastering_the_FreeRTOS...pdf` | FreeRTOS tutorial: tasks, queues, semaphores, mutexes, interrupts |

The LM75B and the MMA7660 share the I2C bus on p28/p27.

### Not in this repo: txCmdFRTOS.zip, tstLM75B.zip, tstRTC.zip

These three zips are deliberately left out of the repo (see `.gitignore`). Each is about 106 MB because it bundles the whole mbed library, which is over GitHub's 100 MB file limit. Download them from the course page and put them in `materials/`.


## Building and flashing

Mbed CLI 1 with the GCC_ARM toolchain (`README_LabSTR` has the Windows lab setup):

```
mbed target LPC1768
mbed toolchain GCC_ARM
mbed deploy                 # after adding new files
mbed compile                # -> BUILD/LPC1768/GCC_ARM/<project>.bin
```

Copy the `.bin` onto the `MBED` USB drive and reset the board. Rename it `frtos.bin` for delivery.

Serial console: 115200 baud, local echo on, CR added to every LF. On Linux the board is `/dev/ttyACM0` (for example `picocom -b 115200 --echo --omap crlf /dev/ttyACM0`), so `mbedWinSerial` isn't needed.


## Repository layout

```
materials/          spec, FreeRTOS manuals, lab notes and course examples
```

Source code will go in its own folder, built from `txCmdFRTOS`.
