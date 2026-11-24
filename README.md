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


## Work split

The foundation comes first. It sets the architecture and the interfaces between modules, so the three parts after it can be written in parallel without touching each other's files.

### Foundation (Simão Silva)

- One `.cpp`/`.h` pair per module (`temp`, `alarm`, `buzzer`, `bubble`, `hitbit`, `lcd`, `clock`) plus `shared.h`.
- A shared state struct (parameters, alarm settings, task states) set to the initial values above and protected by a mutex, with get/set helpers.
- The I2C mutex, since the LM75B and the MMA7660 share the bus.
- An LCD manager task that owns the LCD. Other tasks send it updates through a queue.
- The buzzer request queue, so any task can ask for an alarm sound.
- Every task created as a stub, with priorities and stack sizes chosen to fit the 16 KB heap.
- RTC setup, time helpers, and `hh:mm:ss` on the LCD.
- All 19 console commands registered. These are finished in the foundation, with range checks: `rdt`, `sd`, `rc`, `sc`, `rp`, `mmp`, `mta`, `rai`, `sac`, `sat`, `adac`, `adat`, `rts`, `adbl`, `adhb`, `adcs`.
- The `A: C T` line on the LCD.

The parts start once the foundation builds, shows the clock on the LCD and answers commands on the console.

### Part 1: Temperature monitoring (Simão Silveira)

- Temperature task: periodic reads every `pmon` seconds (none when `pmon = 0`) and on-demand reads requested by the console, with the value sent back to it.
- Commands `rt`, `rmm` and `cmm`.
- Max/min records with RTC timestamps.
- RGB LED colour gradient between `tlow` and `thigh`.
- Temperature alarm check against `[tlow, thigh]`, which sends a request to the buzzer queue.
- `T(C)= tt.t` on the LCD.

### Part 2: Alarms and sound (Isam Abbasi)

- Alarm clock: compares the RTC with `ALAH:ALAM:ALAS` while `ALAC` is on.
- Buzzer task: takes requests from its queue and drives the PWM for `tala` seconds.
- Config sound mode (`adcs`): pot1 sets the PWM period and pot2 the duty cycle.
- What happens when alarms overlap, for example a temperature alarm while the clock alarm is sounding.

### Part 3: Bubble level and Hit Bit (Simão Silva)

- Bubble level task: reads X/Y from the MMA7660 under the I2C mutex and draws the bubble on the right side of the LCD.
- Hit Bit task: rotates the pattern on LED1-4, wakes on a joystick-centre interrupt (semaphore or task notification), handles hits and misses, and blinks all 4 LEDs 3 times on a win.
- Suspend/resume for `adbl` and `adhb`, clearing the LEDs and the bubble area of the LCD when a task is turned off.

### Report

Each person writes the section on their own part. The foundation author writes the architecture, priorities and shared-resource section.


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

Mbed CLI 1 with the GCC_ARM toolchain (`README_LabSTR` has the Windows lab setup). The code is in `frtos/`.

The mbed 2 library (`frtos/mbed/`, ~450 MB) and its build tools (`frtos/.temp/`) aren't in git. After a fresh clone, copy them from the zip:

```
unzip -q materials/txCmdFRTOS.zip -d /tmp/base
cp -a /tmp/base/txCmdFRTOS/mbed /tmp/base/txCmdFRTOS/.temp frtos/
```

The bundled tools are too old for current Python (they break on 3.10+), so build inside a Python 3.8 venv. One-time setup on Linux with `uv`:

```
cd frtos
uv venv -p 3.8 .venv
uv pip install -p .venv/bin/python pip mbed-cli jsonschema "pyelftools<0.30" -r .temp/tools/requirements.txt
```

Build:

```
cd frtos
source .venv/bin/activate
mbed compile -m LPC1768 -t GCC_ARM     # -> BUILD/LPC1768/GCC_ARM/frtos.bin
```

Copy `frtos.bin` onto the `MBED` USB drive and reset the board. Since the folder is called `frtos`, the binary already has the name required for delivery.

Serial console: 115200 baud, local echo on. The board already sends `\r\n` (`frtos/mbed_app.json`), so the terminal's "CR on every LF" option makes no difference. On Linux the board is `/dev/ttyACM0` (`picocom -b 115200 --echo /dev/ttyACM0`), so `mbedWinSerial` isn't needed.


## Testing

Flash `frtos.bin`, open the terminal (`picocom -b 115200 --echo /dev/ttyACM0`) and press the board's reset button, so the title and the `Cmd>` prompt aren't missed. Each line below lists what to type or do and what should happen. The "Board" column says whether it has been checked on real hardware; update it after every board session.

Last board session: none yet.

### Console

| Test | Expected | Board |
|---|---|---|
| reset the board | `Application Control Monitor`, `Type sos for help`, `Cmd>` | not yet |
| `sos` | 21 commands in two aligned columns, no staircase lines | not yet |
| `tset`, 3 backspaces, `est`, Enter | runs as `test` | not yet |
| `test a b c` | `argv[0]` to `argv[3]` | not yet |
| `xyz` | `Invalid command!` | not yet |
| `rdt` (or any command not written yet) | `rdt: not implemented yet` | not yet |

### Parameters and alarm settings

| Test | Expected | Board |
|---|---|---|
| `rp` after reset | `pmon = 5 s, tala = 3 s` | not yet |
| `rai` after reset | `alarm clock: 12:00:00 (off)`, `temperature: tlow = 15 C, thigh = 30 C (off)` | not yet |
| `mmp 10`, then `rp` | `pmon = 10 s`, and `rp` shows it | not yet |
| `mta 60`, `mta 61` | `tala = 60 s`; then `t must be a number 0..60` | not yet |
| `sac 7 30 0`, then `rai` | `alarm clock = 07:30:00`, and `rai` shows it | not yet |
| `sac 7 99 0`, then `rai` | `m must be a number 0..59`, alarm unchanged | not yet |
| `sat 20 25` / `sat 30 20` | thresholds changed / `tl must be lower than th` | not yet |
| `adac 1`, `adat 1`, then `rai` | `alarm clock on`, `temperature alarm on`, `rai` shows both `(on)` | not yet |
| `mmp abc`, `mmp 12x`, `mmp` | `p must be a number 0..99` twice, then `usage: mmp p` | not yet |
| `mmp 5 6` | `usage: mmp p` | not yet |

### Still to write

To be filled in as each part is implemented, from the spec:

- **Clock:** `sd`/`sc` set the RTC and keep it running; `rdt`/`rc` read it in `dd/MM/YYYY hh:mm:ss`; out-of-range dates are rejected; the alarm clock sounds at `ALAH:ALAM:ALAS` when `ALAC = 1`.
- **Temperature:** periodic reads every `pmon` s, none when `pmon = 0`; `rt` works even when `pmon = 0`; `rmm`/`cmm` with timestamps; RGB LED blue below `tlow`, red above `thigh`, graded in between; an alarm outside `[tlow, thigh]` when `ALAT = 1`.
- **Buzzer:** sounds for `tala` s on each alarm; config sound mode (`adcs 1`) changes the PWM period with pot1 and the duty cycle with pot2.
- **LCD:** `hh:mm:ss` updating every second, `A: C T` showing which alarms are on, `T(C)= tt.t`.
- **Bubble level:** the bubble follows the board's tilt on X and Y; `adbl 0`/`1` stops and restarts it.
- **Hit Bit:** pattern rotating on LED1-4, the joystick centre clears a lit bit or adds one, a win blinks all 4 LEDs 3 times; `adhb 0`/`1`.
- **Task states:** `rts` shows BL, HB and CS, initially 1, 0 and 0.


## Repository layout

```
frtos/              the application, built from txCmdFRTOS
  main.cpp            builds the system: init modules, create tasks, start the scheduler
  console.cpp/.h      UART receive interrupt, input queue, console task
  monitor.cpp         command interpreter and command table (course code)
  commands.cpp        console command handlers
  shared.cpp/.h       shared parameters and task states, behind a mutex
  mbed_app.json       mbed settings (newline conversion)
  freertos-cm3/       FreeRTOS V9.0.0 and FreeRTOSConfig.h
materials/          spec, FreeRTOS manuals, lab notes and course examples
STUDY.md            every design decision and the theory behind it
```
