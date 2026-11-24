# STUDY - what we did and why

The project's technical decisions and the reasons behind them, by topic. Updated before every commit.

1. Build environment
2. Architecture and work split
3. Console I/O

## 1. Build environment

- **Mbed CLI 1 with the mbed 2 library (`mbed-dev`)**, as in the course's `txCmdFRTOS` example. mbed 2 has no RTOS of its own, which is why FreeRTOS can be added on top. It is also why the code uses `Serial` and `wait()` and not the mbed-os 6 APIs.
- **The mbed library (`frtos/mbed/`, ~450 MB) and the build tools (`frtos/.temp/`) are not in git.** They come from `txCmdFRTOS.zip`. The zip is over GitHub's 100 MB file limit, and the library is not our code.
- **Python 3.8 in a venv (`frtos/.venv`).**
  - Theory: Python 3.10 removed the old `collections.Mapping` alias (it lives in `collections.abc`), and the bundled tools still import the old name.
  - Practice: the system has Python 3.14, so the tools crash on import. A 3.8 venv, made with `uv`, runs them unchanged. Any Python up to 3.9 works; the lab PCs use conda.
  - The tools also need `jsonschema` and `pyelftools` 0.29. Version 0.30 removed `py3compat`, which the tools import. The lab notes (`README_LabSTR`) list the same two fixes.
  - The venv must be activated in each new terminal (`source .venv/bin/activate`). Without it, `mbed` runs on Python 3.14 and fails with `cannot import name 'Mapping'`.
- **`[Error] Compiler version mismatch: Have 14.2.1; expected ... < 7.0.0`** is printed on every build and can be ignored. The tools were written for GCC 6, the build still links, and `frtos.bin` is produced.
- **The program folder is called `frtos`**, so Mbed CLI names the binary `frtos.bin`, the name the delivery requires.

## 2. Architecture and work split

- The foundation (shared state, mutexes, queues, task stubs, console, RTC, LCD) is done first. It fixes the interfaces between modules, so the three parts (temperature; alarms and sound; bubble level and Hit Bit) can be written in parallel, each in its own files. The README lists who does what.
- **LCD: a manager task, not a mutex.**
  - The problem: the C12832 driver is one object holding shared state: a 512-byte frame buffer, the text cursor set by `locate()`, and the `auto_up` flag. Writing text is at least two calls (`locate`, then `printf`). If another task draws in between, the text lands at the wrong position. Every multi-call drawing sequence must be atomic.
  - Option A, a mutex: each task locks, draws and unlocks. It is simple, but every task waits while another holds the LCD through a full SPI transfer, so tasks of different priorities block each other. That needs priority inheritance to avoid priority inversion, and the knowledge of the screen layout is spread across every module.
  - Option B, a manager task (chosen): one task owns the LCD and the others send it short messages through a queue (clock text, temperature, bubble position). Senders never wait for SPI. There is no LCD mutex and so no priority inversion on the LCD. The whole layout lives in one file, and the task can apply several updates and push the frame buffer once.
  - The cost of B: the extra task's stack and the queue come out of the 16 KB heap (~1 KB), and updates appear one scheduling delay later, which does not matter for a display refreshed about once a second.
  - The spec accepts either ("specific resource manager tasks or ... distributed way"). B was chosen mainly because of the work split: parts 1 and 3 only send messages and never touch the LCD driver.

## 3. Console I/O

The board's USB connection also shows up on the PC as a serial port (`/dev/ttyACM0` on Linux, a COM port on Windows). A terminal program on the PC (picocom, PuTTY) opens that port: what you type is sent to the board, and whatever the board prints with `printf` appears in the window. The command interpreter (`monitor.cpp`) runs on the board.

- **Two jobs that are not automatic over a serial line:**
  - Echo: a typed key goes to the board, and nothing draws it on the PC's screen unless the terminal does ("local echo") or the board sends it back.
  - New lines: a terminal needs `\r` (back to column 0) and `\n` (down one line). C code only writes `\n`, so without a `\r` the output steps down like a staircase.
- **Who does each job (decision):**
  - Echo is done by the terminal, as `README_LabSTR` configures PuTTY in the lab. If the board echoed as well, every character would appear twice on the lab PCs.
  - New lines are fixed on the board by one mbed setting in `frtos/mbed_app.json`: `platform.stdio-convert-newlines: true`. mbed's `_write` (`mbed_retarget.cpp`) then sends `\r\n` for each `\n` written to stdout. It remembers the previous character, so a `\r\n` already in a string does not become `\r\r\n`. All code writes plain `\n`.
  - An earlier commit had moved both jobs into the code (the board echoing every key, `\r\n` in every string). It looked right on a terminal without echo, but would print every key twice in the lab, and every future `printf` would have to remember `\r\n`.
- **Terminal:** `picocom -b 115200 --echo /dev/ttyACM0`. `--echo` is local echo. No mapping option is needed, since the board sends `\r\n`.
- **`my_fgets` (`main.cpp`)** reads one line character by character with `pc.getc()`:
  - Enter (`\r` or `\n`) ends the line and prints `\n`. With local echo, the terminal only echoes the carriage return, so without this newline the command's output would overwrite the line just typed.
  - Backspace (`\b` or `0x7f`; terminals send either) removes the last character from the buffer. Only the board can do this, since the terminal can erase the character on the screen but not in the board's buffer. Otherwise `tset` + 3 backspaces + `est` would reach the parser as `tsetest`.
  - Only printable characters (32..126) are stored, so stray control characters never reach the parser.
- **`fflush(stdout)` after the `Cmd> ` prompt.** newlib (the C library) buffers stdout by line: text is sent when a `\n` arrives. The prompt has no `\n`, so without the flush it would stay in the buffer until the next line.
- **`strtok` also splits on `\r`**, so a stray `\r` never sticks to the last argument.
- **Known issues, fixed in the next steps:**
  - `pc.getc()` polls: it loops until a character arrives, so the console task never blocks. Tasks at the same priority must time-slice with it, and tasks at a lower priority (including the idle task) never run.
  - `ARGVECSIZE` is 3 in `monitor.cpp`, so a line keeps at most 3 words including the command name. `sd d M Y` and `sc h m s` need 4.
