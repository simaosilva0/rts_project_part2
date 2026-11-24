# STUDY - what we did and why

The project's technical decisions and the reasons behind them, by topic. Updated before every commit.

1. Build environment
2. Architecture and work split
3. Console I/O
4. Tasks, preemption and startup
5. Shared configuration and mutual exclusion

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

## 4. Tasks, preemption and startup

- **Tasks.** Each function of the device runs as a task: a function with an infinite loop, its own stack and a priority. One CPU means one task runs at a time. A task is in one of four states:
  - Running: on the CPU.
  - Ready: could run, but a higher-priority task is running.
  - Blocked: waiting for a delay, a queue or a mutex. It uses no CPU.
  - Suspended: switched off until another task resumes it (used later for `adbl`/`adhb`).
- **Scheduler rule:** the highest-priority ready task always runs. With `configUSE_PREEMPTION = 1`, a task that becomes ready (its delay ended, a message arrived) and has a higher priority than the running one takes the CPU immediately, between any two machine instructions. Tasks of equal priority share the CPU on each tick (time slicing). This lets an urgent task respond quickly, and it is also why shared data needs protection (section 5).
- **`main()` is not a task.** It runs once, before the kernel exists, and builds the system: hardware setup (`pc.baud`), kernel objects (`config_init()` creates the config mutex), tasks (`xTaskCreate`), then `vTaskStartScheduler()`.
  - `xTaskCreate` allocates the task's stack and its TCB (Task Control Block: the kernel's record of priority, state and saved stack pointer) from the FreeRTOS heap and puts the task in the ready list. Nothing runs yet.
  - `vTaskStartScheduler` creates the idle task (priority 0, runs when nothing else is ready), starts the 1 ms tick interrupt and jumps to the highest-priority ready task. It never returns. Reaching the `for (;;)` after it means there was not enough heap for the idle task.
  - A task function must never return. Nothing called it, so there is nowhere to return to; the console task's `monitor()` loops forever.
- **The task name** (`"console"`) is only for debugging. The kernel schedules by handle (a pointer to the TCB), never by name. The name is copied into the TCB (at most `configMAX_TASK_NAME_LEN` = 12 bytes including `\0`, reserved in every TCB anyway) and shows up in the stack-overflow hook, `vTaskList()` and debuggers. Names should be short and distinct.
- **Console task: priority 1, stack `4*configMINIMAL_STACK_SIZE`** (520 words = 2080 bytes).
  - newlib's `printf` needs about 1 KB of stack when formatting, and the console prints constantly.
  - A stack overflow in FreeRTOS silently overwrites the memory next to the stack (another task's stack or a TCB) and causes random crashes later. Stack sizes will be measured with `uxTaskGetStackHighWaterMark` and overflow checking turned on once all tasks exist, then trimmed to fit the 16 KB heap.
- **The course demo was removed**: `vTask1`/`vTask2`, the `xQueue` they shared, `cmd_send` (which used that queue) and `cmd_sair` (empty). `led1`/`led2` went too, since LED1-4 belong to Hit Bit and only one module should drive a pin.
- **Serial setup.** `pc.baud(115200)` and `pc.format(8, SerialBase::None, 1)`. The format line sets the default, so it changes nothing; it stays to document the whole setup in one place.
  - Each character travels as a UART frame: a start bit (line goes low), 8 data bits (LSB first), no parity bit, 1 stop bit (line back high). That is 8N1, 10 bits per character.
  - At 115200 bit/s a character takes about 87 µs, so a 40-character line keeps the UART busy for about 3.5 ms. Both ends must agree on baud and format, or the receiver reads garbage.
- `comando.cpp` was renamed to `commands.cpp`. Mbed CLI compiles every `.cpp` in the folder, so no build file names it.

## 5. Shared configuration and mutual exclusion

`shared.h`/`shared.cpp` hold the spec's 12 parameters and task states (`pmon`, `tala`, `tlow`, `thigh`, `alat`, `alac`, `alah:alam:alas`, `bl`, `hb`, `cs`) in one `config_t` struct, with a mutex.

### Theory

- **Shared resource:** anything more than one task uses (a variable, a struct, the LCD, the I2C bus, the serial port).
- **Race condition:** a result that depends on the exact moment of a task switch. Example: the console handles `sat 20 40` and writes `tlow = 20`; the temperature task (higher priority) preempts it, reads `tlow = 20` and the old `thigh = 30`, and checks against a pair no user ever set. Even `x++` can race: it is load, add, store, three instructions, and a switch between them loses an update. Races are rare and timing-dependent, so they are hard to reproduce.
  - On the Cortex-M3, one aligned 8/16/32-bit read or write is atomic, so a single field never tears. The problem is consistency across several fields, or a read-modify-write.
- **Critical section:** the code that touches a shared resource. **Mutual exclusion:** at most one task inside the critical section of a resource at a time.
- **Mutex:** a kernel object that works like the one key to a room.
  - `xSemaphoreTake`: if the key is free, take it and go on; if another task holds it, become blocked (asleep, no CPU) until it is given back.
  - `xSemaphoreGive`: hand the key back. A task waiting for it becomes ready and gets it.
  - A mutex has an owner (only the task that took it gives it back) and cannot be used from an interrupt handler, since an ISR cannot block.
- **Priority inversion:** H (high) and L (low) share a mutex, M (medium) shares nothing. L takes the mutex; H preempts L, tries to take it and blocks; then M becomes ready and preempts L. While M runs, L cannot release the mutex, so H waits on M for an unbounded time. This happened on Mars Pathfinder (1997), causing repeated resets until priority inheritance was enabled remotely.
- **Priority inheritance:** when H blocks on a mutex held by L, FreeRTOS raises L to H's priority until L gives the mutex back. M can no longer preempt L, so H waits at most for L's critical section. That bound is why critical sections must be short.
- **Choosing a tool:**

| Tool | How it works | Use it for |
|---|---|---|
| Mutex | key with an owner, priority inheritance | protecting shared data or devices between tasks |
| Binary semaphore | token with no owner, no inheritance, can be given from an ISR | signalling an event (e.g. the joystick ISR waking Hit Bit) |
| Critical section (`taskENTER_CRITICAL`) | disables interrupts, so nothing preempts | a few instructions only; delays every interrupt |
| Gatekeeper task | one task owns the resource, others send it messages | slow multi-step devices (the LCD manager) |

  A binary semaphore used as a lock has no inheritance, so it allows unbounded priority inversion. Mutex to protect, semaphore to signal.
- **Rules for mutexes:** keep critical sections short; never block (delay, queue wait, slow I/O) while holding one; avoid holding two at once, or always take them in the same order, to avoid deadlock (A holds 1 and waits for 2, B holds 2 and waits for 1); always give back what was taken, on every path.

### Decisions in `shared.cpp`

- **One struct, one mutex.** The whole struct is the shared resource, so a reader always gets a consistent snapshot of all 12 values.
- **`static config_t cfg`.** A file-level `static` is invisible to other `.cpp` files, so no task can reach the data without the mutex. The compiler enforces it, which matters with three people writing modules.
- **`config_get` copies the whole struct out** under the mutex. The critical section is a 12-byte copy, a few microseconds; the caller then uses its private copy for as long as it likes. Returning a pointer would let the caller read shared data after giving back the mutex.
- **A mutex, not a binary semaphore,** because tasks of different priorities (console low, temperature higher) share it: the H/L case above.
- **A critical section would also work** for a 12-byte copy, and costs less. The mutex was chosen because it never blocks interrupts and works for any length of critical section.
- **`portMAX_DELAY`**: wait for the mutex without a timeout. Safe because nobody holds it for more than a copy and nothing blocks while holding it; with a timeout, there would be nothing sensible to do on failure.
- **Single writer: only the console calls `config_set`.** Changing one field is get → change → set. Two writers doing this at once would each copy, change different fields, and the second `set` would overwrite the first one's change (a lost update). The mutex does not prevent that, because `get` and `set` are locked separately; one writer rules it out. The rule is written in `shared.h`.
- **`uint8_t` fields.** Every range fits in 0..255, so the struct is 12 bytes and the type documents the range. It is not faster than `int` on a 32-bit CPU.
- **`config_init()` in `main()`, before the scheduler, with its result checked.** The mutex must exist before any task calls `config_get`. No task runs yet, so filling `cfg` without the lock is safe. `xSemaphoreCreateMutex()` allocates from the FreeRTOS heap and returns `NULL` when it is full; with `heap_1` nothing is ever freed, so a failure means the design does not fit, and `main()` stops with a message.
