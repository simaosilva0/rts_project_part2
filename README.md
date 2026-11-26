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

### Starting your part

The foundation is done: every task exists, and each part only replaces the body of its own task. `STUDY.md` explains every interface below; the section numbers point to it.

**Interfaces everyone can use**

| Function | What it does |
|---|---|
| `config_get(&c)` | copy of all parameters and task states (`shared.h`, section 5). Only the console calls `config_set`. |
| `clock_now(&tm)` | current date and time as a `struct tm` (`clock.h`, section 8) |
| `lcd_show_temp(tenths)`, `lcd_show_bubble(dx, dy)`, `lcd_hide_bubble()` | display updates; never block (`lcd.h`, section 9) |
| `buzzer_alarm(ALARM_CLOCK)` / `buzzer_alarm(ALARM_TEMP)` | ask for an alarm sound; never blocks (`buzzer.h`, section 11) |
| `i2c_lock()` / `i2c_unlock()` | around every LM75B or MMA7660 call (`i2c_bus.h`, section 11) |

**Rules**

- Between `i2c_lock()` and `i2c_unlock()`, only the driver call: no delay, no `lcd_show_*`, no `printf`. Unlock on every path.
- Only the console task prints. Other tasks send results to it, or to the LCD.
- Temperatures travel as integer tenths of a degree (235 = 23.5 °C), never as `float` in messages or `printf`.
- Don't call `vTaskDelete`, and don't suspend another task; tasks suspend themselves (section 13).
- New ISRs: NVIC priority 6, only `...FromISR` calls, then `portYIELD_FROM_ISR` (section 6).
- A new console command: write the handler in `commands.cpp`, then change its row in `monitor.cpp` from `cmd_todo` to it.
- Stack overflows stop the board with `FATAL: stack overflow in task <name>`. Run `stk` after testing your part to check its stack margin (section 14).

**Part 1 (temperature):** `temp.cpp` (`vTempTask`, priority 2) and the `rt`, `rmm`, `cmm` handlers. To design: how `rt` asks the task for a reading and gets the reply, and how the task wakes early when `mmp` changes `pmon` (section 7). `LM75B::read()` returns a `float`; convert it to tenths. RGB LED on p23/p24/p25.

**Part 2 (alarms and sound):** `buzzer.cpp` (`vBuzzerTask`, priority 3) and the alarm-clock check in the `vClockTask` loop (`clock.cpp`). The queue carries an `alarm_src_t` or `REQ_MODE`; on `REQ_MODE`, re-read `cs` from the config (section 13). Buzzer on p26, pot1 on p19, pot2 on p20.

**Part 3 (bubble level and Hit Bit):** `bubble.cpp` and `hitbit.cpp` (priorities 3 and 4). The on/off logic already works; replace the marked lines. Create the accelerometer active, `MMA7660 acc(p28, p27, true)`, or its reads busy-wait 12 ms while holding the bus (section 11). Joystick centre on p14, LEDs LED1-LED4.

Check the pins against the mbed Application Board schematic before wiring code to them.

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

Flash `frtos.bin`, open the terminal (`picocom -b 115200 --echo /dev/ttyACM0`) and press the board's reset button, so the title and the `Cmd>` prompt aren't missed. Each line below lists what to type or do and what should happen.

### Console

| Test | Expected |
|---|---|
| reset the board | `Application Control Monitor`, `Type sos for help`, `Cmd>` |
| `sos` | 22 commands in two aligned columns, no staircase lines |
| `tset`, 3 backspaces, `est`, Enter | runs as `test` |
| `test a b c` | `argv[0]` to `argv[3]` |
| `xyz` | `Invalid command!` |
| `rt` (or any command not written yet) | `rt: not implemented yet` |

### Parameters and alarm settings

| Test | Expected |
|---|---|
| `rp` after reset | `pmon = 5 s, tala = 3 s` |
| `rai` after reset | `alarm clock: 12:00:00 (off)`, `temperature: tlow = 15 C, thigh = 30 C (off)` |
| `mmp 10`, then `rp` | `pmon = 10 s`, and `rp` shows it |
| `mta 60`, `mta 61` | `tala = 60 s`; then `t must be a number 0..60` |
| `sac 7 30 0`, then `rai` | `alarm clock = 07:30:00`, and `rai` shows it |
| `sac 7 99 0`, then `rai` | `m must be a number 0..59`, alarm unchanged |
| `sat 20 25` / `sat 30 20` | thresholds changed / `tl must be lower than th` |
| `adac 1`, `adat 1`, then `rai` | `alarm clock on`, `temperature alarm on`, `rai` shows both `(on)` |
| `mmp abc`, `mmp 12x`, `mmp` | `p must be a number 0..99` twice, then `usage: mmp p` |
| `mmp 5 6` | `usage: mmp p` |

### Clock

| Test | Expected |
|---|---|
| `rdt` right after power-on | `01/01/1970 00:00:..`, seconds counting |
| `sd 24 11 2026`, `sc 16 30 0`, then `rdt` | `date = 24/11/2026`, `clock = 16:30:00`, then `24/11/2026 16:30:0x` |
| `rc` a few times | the seconds advance |
| `sd 31 2 2026` | `31/02/2026 does not exist` |
| `sd 29 2 2028` / `sd 29 2 2027` | accepted (leap year) / `29/02/2027 does not exist` |
| `sd 1 1 2038`, `sc 24 0 0` | `Y must be a number 1970..2037`, `h must be a number 0..23` |
| press reset, then `rdt` | the time is kept: the RTC survives a reset |

### LCD

| Test | Expected |
|---|---|
| reset the board | line 1 counting `hh:mm:ss`, line 2 `A:`, line 3 `T(C)= --.-` |
| watch the clock for a minute next to `rc` | the LCD and `rc` agree, no second shown twice or skipped |
| `sc 12 0 0` | the LCD shows `12:00:00` within a second |
| `adac 1`, `adat 1`, `adac 0` | line 2: `A: C`, then `A: C T`, then `A:   T` (the `T` stays in place) |

### Task states

| Test | Expected |
|---|---|
| `rts` after reset | `bubble level: on, Hit Bit: off, config sound: off` |
| `adbl 0`, then `adbl 1` | the bubble square disappears within 0.1 s, then comes back |
| `adbl 1` when already on | `bubble level on`, nothing else changes |
| `adhb 1`, `adcs 1`, then `rts` | both shown as `on` |
| `adbl 2` | `value must be a number 0..1` |

### Bubble level

| Test | Expected |
|---|---|
| board flat on the table | the bubble sits in the centre of its square, at most 1-2 px off |
| tilt the board slowly to each side | the bubble moves toward the raised side (if an axis goes the wrong way, flip its `BUBBLE_SIGN_*` in `bubble.cpp`) |
| tilt the board steeply | the bubble stops at the edge and never draws over the square's border |
| tilt the board while typing commands on the console | the bubble keeps moving, no freeze |

### Memory

| Test | Expected |
|---|---|
| `stk` | 8 tasks (`console`, `lcd`, `clock`, `temp`, `buzzer`, `bubble`, `hitbit`, `IDLE`) with their priority and stack left, then `free heap: N of 16384 bytes` |
| `stk` after using every command and toggle | no task with stack left near 0 |

### Still to write

Tests for the parts not implemented yet, from the spec:

- **Alarm clock:** sounds at `ALAH:ALAM:ALAS` when `ALAC = 1`.
- **Temperature:** periodic reads every `pmon` s, none when `pmon = 0`; `rt` works even when `pmon = 0`; `rmm`/`cmm` with timestamps; RGB LED blue below `tlow`, red above `thigh`, graded in between; an alarm outside `[tlow, thigh]` when `ALAT = 1`.
- **Buzzer:** sounds for `tala` s on each alarm; config sound mode (`adcs 1`) changes the PWM period with pot1 and the duty cycle with pot2.
- **LCD temperature line:** `T(C)= tt.t` shows the last reading.
- **Hit Bit:** pattern rotating on LED1-4, the joystick centre clears a lit bit or adds one, a win blinks all 4 LEDs 3 times; `adhb 0`/`1`.


## Repository layout

```
frtos/              the application, built from txCmdFRTOS
  main.cpp            builds the system: init modules, create tasks, start the scheduler
  console.cpp/.h      UART receive interrupt, input queue, console task
  monitor.cpp         command interpreter and command table (course code)
  commands.cpp        console command handlers
  shared.cpp/.h       shared parameters and task states, behind a mutex
  clock.cpp/.h        RTC access: read and set date/time
  lcd.cpp/.h          LCD manager task and its message interface
  i2c_bus.cpp/.h      mutex for the shared I2C bus (LM75B, MMA7660)
  buzzer.cpp/.h       alarm request queue and buzzer task (Part 2)
  hooks.cpp           FreeRTOS stack-overflow and out-of-heap hooks
  temp.cpp/.h         temperature task (Part 1)
  bubble.cpp/.h       bubble level task (Part 3)
  hitbit.cpp/.h       Hit Bit task (Part 3)
  C12832/             LCD driver library (course code)
  MMA7660/            accelerometer driver library (course code)
  mbed_app.json       mbed settings (newline conversion)
  freertos-cm3/       FreeRTOS V9.0.0 and FreeRTOSConfig.h
materials/          spec, FreeRTOS manuals, lab notes and course examples
STUDY.md            every design decision and the theory behind it
```
