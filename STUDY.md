# STUDY - what we did and why

The project's technical decisions and the reasons behind them, by topic. Updated before every commit.

1. Build environment
2. Architecture and work split
3. Console I/O
4. Tasks, preemption and startup
5. Shared configuration and mutual exclusion
6. Interrupt-driven console input
7. Console commands and input validation
8. Clock and RTC
9. LCD manager task
10. Clock task: periodic work on the RTC's second
11. The I2C bus and the buzzer requests
12. Task priorities and stubs
13. Turning tasks on and off
14. Stack overflow detection and memory measurement
15. Bubble level

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
- **Line endings are LF everywhere.** The course files came with Windows line endings (CRLF), and lines pasted on Linux are LF, so files ended up mixed. Everything was converted to LF, and `.gitattributes` (`* text=auto eol=lf`) makes git store LF even when someone commits from Windows.

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
- **`my_fgets` (`console.cpp`)** reads one line, one character at a time from the receive queue (section 6):
  - Enter (`\r` or `\n`) ends the line and prints `\n`. With local echo, the terminal only echoes the carriage return, so without this newline the command's output would overwrite the line just typed.
  - Backspace (`\b` or `0x7f`; terminals send either) removes the last character from the buffer. Only the board can do this, since the terminal can erase the character on the screen but not in the board's buffer. Otherwise `tset` + 3 backspaces + `est` would reach the parser as `tsetest`.
  - Only printable characters (32..126) are stored, so stray control characters never reach the parser.
- **`fflush(stdout)` after the `Cmd> ` prompt.** newlib (the C library) buffers stdout by line: text is sent when a `\n` arrives. The prompt has no `\n`, so without the flush it would stay in the buffer until the next line.
- **`strtok` also splits on `\r`**, so a stray `\r` never sticks to the last argument.
- **Command interpreter (`monitor.cpp`).** `monitor()` loops forever: print `Cmd> `, read a line, split it into words with `strtok`. `sat 20 40` becomes `argc = 3`, `argv = {"sat", "20", "40"}`, the same convention as `main(argc, argv)` in C.
  - `commands[]` is a table of `{function pointer, name, help}`. `monitor()` compares `argv[0]` with each name and calls the matching function through its pointer; `sos` prints the same table. This is table-driven dispatch: adding a command means writing a handler and adding one row, and the parser never changes. Every handler has the same signature `(int argc, char **argv)` so it fits in the table, even when it takes no arguments.
  - `extern` in `monitor.cpp` declares handlers defined in `commands.cpp`; the linker joins them.
- **`rp` (read parameters)** is run by the console itself. The spec allows commands to run "directly by the console task" or "through the interaction with specific tasks". `rp` only reads shared data, so it calls `config_get` and prints. `rt` will be different: a new temperature reading belongs to the temperature task, so the console will have to ask that task and wait for the reply.
  - Copy first, print after. The mutex is held only for the 12-byte copy inside `config_get`; the `printf` (about 2.5 ms of UART time for that line) runs outside the critical section, so a higher-priority task never waits for the UART.
  - `%u` with a `uint8_t`: in a variable-argument function like `printf`, types smaller than `int` are promoted to `int`. The values are never negative, so `%u` prints them correctly.

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

## 6. Interrupt-driven console input

`console.h`/`console.cpp` hold the console module: the UART object, the receive queue, the receive ISR, `my_fgets` and the console task. `main.cpp` only builds the system (`console_init()`, `config_init()`, create the task, start the scheduler); every module follows the same pattern with its own `_init()`.

### Theory

- **Polling vs. interrupts.** The old `pc.getc()` waited with `while (!serial_readable(obj));`, asking the UART millions of times per second whether a character had arrived. A task stuck in that loop is always ready and never blocks, so every task at a lower priority, including the idle task, never runs, and tasks at the same priority must time-slice with a loop doing nothing useful. With an interrupt, the UART hardware signals the CPU when a character arrives; the CPU pauses whatever runs, executes the ISR (Interrupt Service Routine) and then continues. Nobody spends CPU time waiting.
- **ISR rules.** An ISR interrupts everything, even the highest-priority task, so it must be short, must never block (it is not a task and cannot sleep: no mutex, no `vTaskDelay`, no `printf`), and may only call the `...FromISR` FreeRTOS functions. These never block and report whether a task switch is needed instead of switching themselves.
- **Deferred processing.** The ISR does the minimum (take the character out of the UART) and a task does the real work (build the line, run the command). This is the most common pattern in real-time systems.
- **The queue between them.** A FreeRTOS queue is a FIFO protected by the kernel, so the ISR can add while the task removes without a race. A task can block on it: `xQueueReceive(rx_queue, &c, portMAX_DELAY)` sleeps, using no CPU, until a character arrives. That one line replaces the polling loop.
- **`woken` and `portYIELD_FROM_ISR`.** When `xQueueSendFromISR` wakes a task with a higher priority than the one interrupted, it sets `woken = pdTRUE`. `portYIELD_FROM_ISR(woken)` then requests a task switch for when the ISR ends.
  - How: the macro (`portmacro.h`) only writes bit 28 of the ICSR register at `0xE000ED04`, which makes the PendSV exception pending. PendSV is an interrupt that only software triggers, and FreeRTOS gives it the least urgent priority (31), so it runs only after every other ISR has returned.
  - The PendSV handler (`xPortPendSVHandler`, `port.c`) is the context switch. On exception entry the hardware has already pushed r0-r3, r12, LR, PC and xPSR onto the current task's stack; the handler pushes r4-r11, stores the stack pointer in the task's TCB, calls `vTaskSwitchContext()` to pick the highest-priority ready task, loads that task's stack pointer, pops r4-r11 and returns from the exception. The hardware pops the rest, including the PC, so the CPU continues in the new task where it last stopped (here, inside `xQueueReceive`, holding the new character).
  - The Cortex-M3 goes from the ending ISR straight into the pending PendSV without returning to the task in between (tail-chaining).
  - Without the macro, the woken task is ready but the ISR returns to the interrupted task. The switch then waits for the next tick (up to 1 ms; the tick ISR sets the same PendSV bit) or for the running task to block. The macro changes how soon the task runs, not whether it runs. Every task switch in FreeRTOS (tick, `taskYIELD()`, a task blocking or waking another) goes through PendSV.
- **Interrupt priorities on the Cortex-M3.** The NVIC gives each interrupt a priority 0..31 on the LPC1768 (5 bits); a lower number is more urgent, the opposite of task priorities. FreeRTOS splits the range with `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5 here):

| NVIC priority | May call FreeRTOS `FromISR` functions | Masked by FreeRTOS critical sections |
|---|---|---|
| 0..4 | no | no (always run immediately) |
| 5..31 | yes | yes |

  When the kernel changes its ready and blocked lists it masks interrupts 5..31 for a moment. An ISR at 0..4 is never masked, so if it called FreeRTOS it could change those lists while the kernel is halfway through, corrupting them and crashing the system later at a random point. This port has no `configASSERT` to catch that.

### Decisions in `console.cpp`

- **`NVIC_SetPriority(UART0_IRQn, 6)`.** mbed leaves every interrupt at priority 0, the forbidden range for an ISR that calls `xQueueSendFromISR`. Any value 5..31 works; 6 leaves room for something more urgent later.
- **`RawSerial`, not `Serial`.** `Serial::getc()` reads through the C library's `FILE` layer (buffers, internal state), which is not safe in an ISR. `RawSerial::getc()` reads the UART register directly; mbed provides it for interrupt use. `printf` still works through mbed's own stdio path to the same UART. Creating the object on the stdio pins marks the stdio UART as initialized (`serial_api.c`), so the first `printf` does not re-initialize the UART and switch the receive interrupt off.
- **The ISR drains the UART in a `while (pc.readable())` loop.** Reading the data register clears the interrupt; an ISR that returned without reading would be entered again immediately, forever. The loop also collects characters that arrived close together.
- **A 32-character queue, characters dropped when full.** An ISR must never wait, so `xQueueSendFromISR` gives up on a full queue. A line is at most 50 characters and the console empties the queue as fast as characters arrive, so in practice nothing is lost.
- **The interrupt is enabled in `console_init()`, before the scheduler.** FreeRTOS keeps interrupts 5..31 masked from the first `xTaskCreate` until the scheduler starts (`uxCriticalNesting` starts at `0xaaaaaaaa` in `port.c`), so the ISR cannot run before the queue and the console task exist.
- **`console_init()` runs before `config_init()`,** so the baud rate is already set if the error `printf` is needed.
- **Output is still polled.** `printf` waits for the UART on every character (about 87 µs each), and the console task keeps the CPU while it prints. Acceptable because only the console prints; an interrupt-driven transmit buffer would remove it if it ever matters.

## 7. Console commands and input validation

- **`ARGVECSIZE` 3 → 4** (`monitor.cpp`). It is the maximum number of words kept from a line, including the command name. The longest commands (`sd d M Y`, `sc h m s`, `sac h m s`) have 1 name + 3 arguments; with 3, the last argument was lost. Extra words beyond 4 are ignored. `argv` is declared `[ARGVECSIZE+1]` because the parser stores one more entry after the last word.
- **All 19 spec commands are in the table from the start.** Commands not written yet point to one shared stub, `cmd_todo`, which prints `<name>: not implemented yet` using `argv[0]`. `sos` already shows the full interface, and implementing a command means writing its handler and changing one word in its row. Each part's owner only edits their own rows, so `monitor.cpp` does not cause merge conflicts.
- **`const char*` for the name and help strings.** In C++ a string literal is read-only (stored in flash), so a plain `char*` pointing at one claims a write permission that does not exist. The compiler warned twice per row (42 warnings with the full table); `const char*` states the truth and removes them. Four older warnings from the course code (`int` vs `unsigned` comparisons, one indentation) are harmless and left as they are.
- **`sos` output in two aligned columns:** `printf("%-5s %s\n", ...)` pads the name to 5 characters, left-aligned, and each help string starts with its arguments padded to 7 characters.
- **Parameter and alarm commands** (`commands.cpp`): `mmp`, `mta`, `rai`, `sac`, `sat`, `adac`, `adat`. The console runs them itself; they only read or change the shared config.

### Input validation

- **Never trust input.** Anything from outside (a user, a sensor, a network) can be missing, extra, not a number or out of range, and the system must keep working whatever is typed. Everything is validated at the border, before it reaches the shared config, so other tasks can trust the config without checking it again.
- **`strtol`, not `atoi`.** `atoi("abc")` returns 0 and `atoi("12x")` returns 12, with no way to tell the input was invalid (`mmp abc` would quietly set `pmon = 0`). `strtol(s, &end, 10)` also reports where it stopped: `end == s` means no digits, `*end != '\0'` means junk after the number. Only a string read completely is accepted, then the range is checked. It returns `long`, so a huge value is caught by the range check, not wrapped around.
- **Two helpers.** `arg_count(argc, n, usage)` checks the number of arguments and prints the usage; `arg_int(s, name, min, max, &out)` checks one argument and prints a precise message (`h must be a number 0..23`). The checks are written once and every command reads as plain logic.
- **Short-circuit `||`.** In `!arg_count(...) || !arg_int(argv[1], ...) || ...`, C evaluates the next operand only if the previous one was false, so the checks run in order and stop at the first failure. If the argument count is wrong, `argv[2]` may not exist, and it is never read.
- **Validate everything, then write.** `sac 10 99 0` changes nothing: all fields are checked before any is stored, so the config never holds a half-applied command.
- **`sat` requires `tl < th`.** The spec only gives the range 0..50, but `tlow >= thigh` makes no sense, and the RGB gradient (Part 1) divides by `thigh - tlow`; equal values would divide by zero. Rejecting it at the border keeps that case out of the temperature task.
- **Read-modify-write: `config_get` → change → `config_set`.** Safe only because the console is the single writer (section 5). The mutex is held for the two copies, never while parsing or printing.
- **Confirmation after every change** (`pmon = 10 s`, `alarm clock = 07:30:00`). It is printed after `config_set`, so it only appears once the value is stored, and outside the mutex. Errors always say what was wrong. `%d` matches the `int` variables used here; `rp`/`rai` print the `uint8_t` fields with `%u`. `%02d`/`%02u` pad to two digits, so times print as `hh:mm:ss`.
- **When other tasks see a change.** `config_set` only updates the struct; each task sees the new value the next time it calls `config_get`. This matters for the temperature task (Part 1): if it sleeps with `vTaskDelay(pmon)` and `pmon = 99`, `mmp 5` takes effect only after up to 99 s. It should instead wait on something the console can signal (a queue or a task notification) with `pmon` as the timeout.

## 8. Clock and RTC

`clock.h`/`clock.cpp` wrap the LPC1768's RTC: `clock_init()`, `clock_now(&tm)`, `clock_set_date(d, M, Y)` and `clock_set_time(h, m, s)`. The console commands `rdt`, `rc`, `sd` and `sc` use them.

### Theory

- **The RTC** is a separate counter in the LPC1768, driven by its own 32.768 kHz crystal. 32768 = 2^15, so fifteen divisions by 2 give exactly 1 Hz. It counts seconds in hardware, independent of the CPU, FreeRTOS and task load, and keeps running through a press of the reset button (not through a power-off: the mbed board has no battery).
- **Why not count seconds in software.** A task that wakes every 1000 ms with `vTaskDelay` also spends time running and can be delayed by higher-priority tasks, so each "second" is slightly longer and the error accumulates (drift). The RTC does not drift with CPU load.
- **Two representations of time:**
  - `time_t`: seconds since 01/01/1970 00:00:00 (the Unix epoch). One number, easy to store, compare and add to.
  - `struct tm` ("tm" = time): broken-down calendar fields `tm_sec`, `tm_min`, `tm_hour`, `tm_mday`, `tm_mon`, `tm_year`. `tm_mon` counts 0..11 and `tm_year` counts years since 1900, hence `M - 1` and `Y - 1900` in the code.
  - `localtime_r(&t, &tm)` converts `time_t` → `struct tm`; `mktime(&tm)` converts back.
- **Why the spec limits years to 1970..2037.** On many systems `time_t` is a signed 32-bit number, which overflows on 19/01/2038 (the Year 2038 problem). This toolchain's `time_t` is 64-bit, but the spec range stays inside the safe zone anyway.
- **A hardware race: the torn read.** mbed's `time()` reads the RTC registers (SEC, MIN, HOUR, DOM, MONTH, YEAR) one at a time, and the RTC keeps counting meanwhile. If the time changes between two of those reads, the fields come from different moments: reading at 12:59:59 → 13:00:00 can give 13:59:59.
- **Reentrant functions.** `localtime()` returns a pointer to one static buffer inside the C library, so two tasks calling it overwrite each other's result. `localtime_r()` (`_r` = reentrant) writes into a buffer the caller passes, on its own stack. In a multitasking program the `_r` versions are the safe ones; the course's `ctime.c` demo uses them too.

### Decisions in `clock.cpp`

- **Read twice, accept only matching reads** (`rtc_read_stable`). A torn read needs the time to change during a read; two consecutive reads take microseconds, and the clock cannot change twice in that time, so two equal reads are both correct.
- **A mutex for the RTC registers as well** (`rtc_mutex`). The double read handles the clock ticking, not another task writing. mbed's `set_time()` pauses the RTC, writes the registers one by one and restarts it; a task reading in between sees new seconds with old hours, and because the clock is paused, both of its reads match. mbed's own lock around `time()`/`set_time()` is empty when no mbed RTOS is present, so the mutex is ours to add.
- **One mutex per resource.** The RTC and the config are different resources with different mutexes, so a task reading the clock never waits for one copying the config. No code holds both at once, so they cannot deadlock.
- **Only the hardware access is inside the mutex** (`rtc_get`, `rtc_set`). The conversions (`localtime_r`, `mktime`) run outside it, on the caller's local copy, so the critical section stays short.
- **`sd` and `sc` are read-modify-write.** The RTC holds date and time as one value, so `sd` reads the full current `struct tm` (`clock_now`), replaces only the date fields, converts and writes back; `sc` keeps the date and replaces the time. Without the read, the untouched fields would be uninitialized stack garbage. This is safe because the console is the only task that sets the clock (single writer). `set_time()` also resets the RTC's sub-second counter, so each set loses under 1 s.
- **Checking that a date exists.** `arg_int` checks `d` 1..31 and `M` 1..12 separately, so `sd 31 2 2026` passes it. `mktime` normalizes impossible dates (31/02/2026 becomes 03/03/2026), so if `tm_mday` or `tm_mon` changed after `mktime`, the date did not exist. `t == (time_t)-1` catches dates `mktime` cannot convert at all. One check covers 30/02, 31/04 and 29/02 in non-leap years.
- **`strftime` for output.** Its `%` codes map to `struct tm` fields with padding and offsets built in: `%d` day, `%m` month (adds 1), `%Y` year (adds 1900), `%H:%M:%S`. The format decides which fields are printed, so `rdt` and `rc` share `clock_now` and differ only in the format. Unlike `printf`, `strftime` never writes past the buffer: if the result does not fit, it returns 0 and the buffer is undefined, so buffers are sized exactly (19 + `\0` = 20 for `dd/mm/yyyy hh:mm:ss`, 8 + `\0` = 9 for `hh:mm:ss`).
- **On first power-up** `time()` finds the RTC disabled and starts it at 01/01/1970 00:00:00, a valid date inside the spec range. `clock_init()` calls it once so this happens before any task runs.
- **Flash cost:** the C library's date functions add about 22 KB of code (78 KB total of the LPC1768's 512 KB).

## 9. LCD manager task

`lcd.h`/`lcd.cpp` implement the decision from section 2: one task owns the LCD, and the others send it messages. The driver library (`C12832/`, from the course's `C12832.zip`, without its `.hg` Mercurial folder) is compiled as part of the program.

### The display hardware and driver

- The C12832 is 128x32 pixels, one bit each: 4096 bits = 512 bytes. The driver keeps a copy of the whole screen in RAM, the frame buffer (`buffer[512]`). `locate`, `_putc`, `rect` and `fillcircle` only change bits in that buffer; `copy_to_lcd()` sends all 512 bytes over SPI (a serial bus: data on MOSI, clock on SCK) at 20 MHz, about 0.3 ms plus overhead.
- **`set_auto_up(0)`.** By default the driver sends the whole buffer after every drawing call (every character, every rectangle); drawing three lines would take about 25 transfers. With auto-update off, the task draws into the buffer and sends it once.
- **Pins** `C12832 lcd(p5, p7, p6, p8, p11)` (mosi, sck, reset, a0, ncs), as in the course's `tstLM75B` example.
- **The constructor runs before `main()`.** `lcd` is a global C++ object, so it is initialized before `main()`. The constructor resets the display with `wait_ms(5)`, a busy-wait, which is harmless because no task exists yet.
- **Font and layout.** `Small_7` is 9 px tall, so three text lines fit at y = 0, 11 and 22. The right 32x32 square (x = 96..127) is reserved for the bubble level. The constants live in `lcd.h`, so the layout is defined in one place.
- **The font is proportional** (each character has its own width), so new text can be narrower than the old one; each line is cleared (`fillrect` with colour 0) before it is redrawn, or old pixels would remain.

### Ownership instead of locking

- The `lcd` object is `static` in `lcd.cpp`, so no other file can reach the driver; the compiler enforces that only the LCD task draws. With a single user there is no race on the LCD and no mutex: mutual exclusion comes from ownership.
- **No `lcd.printf`.** The driver inherits `printf` from mbed's `Stream`, which writes through the C library's stdio (`FILE`, `vfprintf`). That code keeps shared internal state and, without extra configuration, is not reentrant; two tasks inside it at once (the console printing and the LCD drawing) could corrupt each other. Text is drawn with `locate` + `_putc` straight into the frame buffer. Numbers are formatted with `snprintf` into a local buffer, with integers only: that only touches the caller's stack. The dangerous parts are `FILE` streams and float formatting, and `snprintf`, unlike `sprintf`, never writes past the buffer.

### Message passing

- **Shared memory with a lock vs. message passing.** The config is shared memory behind a mutex. The LCD uses message passing: a FreeRTOS queue copies the message in on send and out on receive, so the sender can build it in a local variable and return, and the two tasks never touch the same memory.
- **The message is a tagged union** (`lcd_msg_t`): `type` (the tag) says which update it is; a `union` holds the data, the time (3 bytes), the alarm flags (2), the temperature (2) or the bubble offset (2). Union members share the same memory, so a message is only as large as the largest member plus the tag: 8 bytes after alignment. The receiver reads `type` first and only the matching member; reading another one would give garbage.
- **The interface is a set of functions:** `lcd_show_clock(h, m, s)`, `lcd_show_alarms(c, t)`, `lcd_show_temp(tenths)`, `lcd_show_bubble(dx, dy)`. The message type, the queue and `send()` are `static` in `lcd.cpp`. Other modules never see queues, structs or pixel coordinates, and the message format can change without touching them.
- **Sending never blocks** (`xQueueSend(..., 0)`). If the 8-message queue is full, the update is dropped and the sender carries on. The display is the least important job in the system: reacting to the joystick or sounding an alarm on time matters (hard deadlines), while a display update that is late or skipped is harmless (a soft deadline), and the next one replaces it.
- **Batching.** The task sleeps in `xQueueReceive(..., portMAX_DELAY)`. After the first message wakes it, it keeps receiving with timeout 0 until the queue is empty, drawing each message into the frame buffer, then calls `copy_to_lcd()` once. Updates that arrive together cost one SPI transfer.
- **Temperature in tenths of °C (fixed-point):** `lcd_show_temp(235)` means 23.5 °C. The Cortex-M3 has no floating-point hardware, so float operations run as slow software routines, and float formatting uses C-library code with shared buffers (not reentrant). With an integer, `abs(t) / 10` and `abs(t) % 10` give the two parts, and the sign is printed separately so -0.5 shows as `-0.5`. The LM75B reads in 0.125 °C steps, so tenths keep all the precision the display shows.
- **Drawing details.** `C` and `T` sit at fixed positions (x = 14 and 24), so `T` does not move when the alarm clock is turned off. The bubble offset is clamped to ±12 px, so it can never draw outside its square, even with bad data: the code that owns a resource protects it. The initial screen shows `--:--:--` and `--.-` until the first values arrive.
- **The `A: C T` line is event-driven.** It changes only when `adac` or `adat` runs. Polling (some task re-sending the flags every second) would mean 86,400 messages a day for a value that almost never changes, plus up to 1 s of lag. The console is the only writer of the config, so it is the one place that knows when the flags change, and it calls `lcd_show_alarms(c.alac, c.alat)` right after `config_set`.
  - Both flags travel in every message, taken from the console's updated copy, so the LCD redraws the whole line from one consistent state and keeps no copy of the truth of its own.
  - At startup the LCD task reads the config and sends itself an alarms message, so the initial line goes through the same drawing code as every later change. This happens in the task, not in `lcd_init()`: `config_get` takes a mutex, and calls that can block must not run before the scheduler starts.
- **Task settings (provisional):** priority 1, stack 2x minimal (no `printf`). Step 5 sets all priorities together, checks every `xTaskCreate`, and measures stacks.

## 10. Clock task: periodic work on the RTC's second

`vClockTask` (`clock.cpp`) runs once per second: it reads the RTC and sends `hh:mm:ss` to the LCD. Part 2 adds the alarm-clock check to the same loop.

### Theory

- **Periodic tasks.** A periodic task does its job every T: the clock every second, the temperature every `pmon`, the bubble level several times a second. Three ways to time one:

| Method | How | Problem |
|---|---|---|
| `vTaskDelay(T)` | sleep T ticks after the work | the period is T plus the work plus any preemption, so it drifts |
| `vTaskDelayUntil(&last, T)` | wake at fixed points last + T, + 2T, ... | no drift, but it follows the CPU's clock |
| hardware interrupt | the device that defines the event wakes the task | the task runs when the event happens |

- **Two clocks drift apart.** The RTC (32.768 kHz crystal) and the FreeRTOS tick (the CPU's 12 MHz crystal) are independent. A task timed by the tick would slowly slide relative to the RTC's seconds; every so often it would wake just before the RTC changed and show the same second twice, then later skip one. Waking on the RTC's own interrupt ties the task to the real second.
- **Binary semaphore for signalling.** The ISR gives (an event happened), the task takes (sleep until it happens): the "semaphore to signal" row of the table in section 5. No data travels, only the moment, so a semaphore is enough instead of a queue. A mutex would be wrong: it has an owner that must give it back, and it cannot be used from an ISR. A binary semaphore has no owner.
- **A binary semaphore does not count.** Two gives before one take merge into one. That is acceptable here because the task never counts ticks: it reads the real time from the RTC each time, so a missed signal could only skip one display update, never show a wrong time.

### Decisions

- **The RTC counter-increment interrupt.** `CIIR` (Counter Increment Interrupt Register) bit 0 enables an interrupt on every seconds increment. `ILR` (Interrupt Location Register) bit 0 is its flag; writing 1 clears it. The ISR must clear it, or it is entered again immediately, forever (as with reading the UART character). `NVIC_SetVector`, `NVIC_SetPriority(RTC_IRQn, 6)` (the same 5..31 rule as the UART) and `NVIC_EnableIRQ` install and enable it.
- **The ISR is the deferred-processing pattern again:** clear the flag, `xSemaphoreGiveFromISR(tick_sem, &woken)`, `portYIELD_FROM_ISR(woken)`.
- **The task enables the interrupt as its first action.** `tick_sem` is created in `clock_init()`, but the interrupt only matters once its listener runs; enabling it inside the task means the ISR can never run before the scheduler, without relying on FreeRTOS masking interrupts before the scheduler starts (which the UART does).
- **A semaphore, not a task notification.** Task notifications are lighter, but the ISR would need the task's handle, and `xTaskGetCurrentTaskHandle` is disabled in this config (`INCLUDE_xTaskGetCurrentTaskHandle = 0`). The semaphore needs no handle.
- **Show first, then wait.** The loop reads and displays the time before taking the semaphore, so the display is right at startup instead of showing `--:--:--` for up to a second. After `sc`, it catches up at the next tick.
- **Priority 3** (above the console and LCD at 1). The work is short and once a second, and the alarm-clock check (Part 2) must happen on the second, not whenever the CPU is free. General rule: the more urgent and shorter a task's work, the higher its priority. Step 5 sets the full scheme.
- **Task creation order does not matter.** When the scheduler starts it runs the highest-priority ready task, whatever order the tasks were created in.

## 11. The I2C bus and the buzzer requests

The last two shared interfaces, used by the parts: `i2c_bus.h`/`.cpp` (a mutex for the I2C bus) and `buzzer.h`/`.cpp` (the alarm request queue).

### The I2C bus

- **I2C** uses two wires shared by every device: SDA (data) and SCL (clock). The mbed is the master and starts every transfer; the LM75B and the MMA7660 are slaves, each with its own address, both on p28/p27. A read is a transaction: START, address + write, the register number, a repeated START, address + read, the data bytes, STOP. `LM75B::read()` does exactly this: `i2c.write(addr, cmd, 1)` selects the temperature register, then `i2c.read(addr, cmd, 2)` reads it.
- **Why a mutex.** The temperature task (Part 1) and the bubble level task (Part 3) can each be preempted in the middle of a transaction. If the other one runs a transaction in between, the first reads the wrong register or the bus is left mid-transfer: a race on a hardware resource. Each driver library creates its own mbed `I2C` object on the same pins, so nothing below us coordinates them, and mbed's I2C lock is empty without the mbed RTOS.
- **A mutex here, a manager task for the LCD.** A manager task suits work callers can hand off and forget (the LCD: several ms per redraw, no result needed). An I2C reading takes about 0.3-0.5 ms at 100 kHz and the caller needs the value back at once; a manager task would need a request queue, a reply queue and two task switches per reading. A mutex is simpler and faster, and priority inheritance bounds the wait whatever priorities the two tasks get.
- **`i2c_lock()`/`i2c_unlock()`.** The driver objects belong to Parts 1 and 3, so they make the calls and must wrap them. The functions hide the FreeRTOS mutex, so the other modules need no `semphr.h`, and only `i2c_bus.cpp` would change if the mechanism did. Rules for the users:
  1. Only the driver call goes between lock and unlock: no `vTaskDelay`, no `lcd_show_...`, no `printf` while holding the bus.
  2. Unlock on every path; a forgotten unlock blocks the other task forever.
  3. The MMA7660 must be created active (`MMA7660 acc(p28, p27, true)`). In standby, its read functions call `wait(0.012 + ...)`, a 12 ms busy-wait that would hold the bus and the CPU.
- **Driver notes for the parts.** `LM75B::read()` returns a `float` in °C; Part 1 converts it to tenths for `lcd_show_temp`. Both drivers talk to their device in their constructors, which run before `main()` (global objects), when no task exists yet.

### The buzzer request queue

- The spec gives the alarm sound to a specific task. Alarms come from two producers: the clock task (alarm clock, Part 2) and the temperature task (thresholds, Part 1). Both call `buzzer_alarm(ALARM_CLOCK)` or `buzzer_alarm(ALARM_TEMP)`, which puts a request in a 4-slot queue of `uint8_t` (an `alarm_src_t`, or `REQ_MODE` from section 13); the buzzer task takes requests and sounds for `tala` seconds.
- **A queue, not a semaphore:** each request carries which alarm it is, so Part 2 can decide what to do when alarms overlap. A semaphore only says that something happened.
- **Never blocks (timeout 0).** The clock task must not stall on the second because the buzzer is busy. A full queue means 4 alarms are already waiting, so dropping one more loses nothing.
- **Interfaces before tasks.** `buzzer_alarm` works even when no task reads the queue: requests queue up and, once the queue is full, are dropped. The parts can write their calls against interfaces that already compile.

## 12. Task priorities and stubs

Every task of the system now exists. The parts' tasks are stubs in their own files (`temp.cpp`, `buzzer.cpp`, `bubble.cpp`, `hitbit.cpp`); each part only replaces the body.

### Choosing priorities

- **Rate Monotonic (RM):** for periodic tasks, the shorter the period, the higher the priority. A task due every 100 ms has less slack than one due every 5 s. Liu & Layland (1973) showed RM is the optimal fixed-priority order when each deadline equals the period.
- **Deadline Monotonic (DM):** the general version for any task, periodic or event-driven: the shorter the required response time, the higher the priority.
- **Hard vs. soft:** tasks whose lateness is harmless (display, human console) go to the bottom.

### The scheme (`main.cpp`, one table of `PRIO_*` constants)

| Prio | Task | Period / trigger | Reason |
|---|---|---|---|
| 4 | hitbit | LED rotation every ~100-300 ms; joystick press | shortest period; a press judged late (LED4 already moved) makes the game unfair |
| 3 | bubble | ~100 ms | short period (RM) |
| 3 | buzzer | alarm request; pots every ~100 ms in config sound mode | the alarm must start promptly; short polling period |
| 3 | clock | 1 s, on the RTC second | short work; the alarm-clock check must happen on the second |
| 2 | temp | `pmon` (≥ 1 s, default 5 s) or on demand | longest period of the periodic tasks (RM) |
| 1 | lcd | messages | soft; below its producers so it batches |
| 1 | console | human typing | soft; a person does not notice a few ms |
| 0 | idle | nothing else ready | created by FreeRTOS |

- **The LCD sits below its producers on purpose.** Producers run first, so when the LCD finally runs it finds several messages and draws them with one SPI transfer. Above them, it would preempt after each message and redraw every time.
- **Equal priorities are fine here.** FreeRTOS time-slices equal-priority tasks only while they are ready at the same moment; these tasks run for microseconds to a millisecond and block, so they rarely overlap.
- **Only 5 levels** (`configMAX_PRIORITIES = 5`, 0 taken by idle), so tasks are grouped by urgency.
- **One table** keeps the urgency order of the whole system visible in seven lines.
- **The console must stay below bubble and Hit Bit**: section 13 depends on it.

### Checked task creation and the heap budget

- `create_task()` wraps `xTaskCreate` and stops with `cannot create task <name>: out of heap` if the heap cannot hold the stack and TCB. Before, a failed creation was silent and only showed up as odd behaviour later. With `heap_1` nothing is freed, so a failure at startup means the design does not fit, and the message names the task.
- **Stacks:** `STACK_PRINTF` (4x minimal = 520 words) for the console, which calls `printf`; `STACK_SMALL` (2x = 260 words) for the rest.
- **Estimated heap use**, to be measured on the board:

| Item | Bytes (approx.) |
|---|---|
| console stack | 2080 |
| 6 small stacks | 6240 |
| idle stack (130 words) | 520 |
| 8 TCBs (~96 B each) | ~770 |
| 3 queues (UART, LCD, buzzer) | ~340 |
| 3 mutexes + 1 binary semaphore | ~300 |
| **Total** | **~10.2 KB of 16 KB** |

### The stubs

- `temp` sleeps forever (a task must never return).
- `buzzer` already drains its queue, the receive pattern Part 2 builds on.
- `bubble` and `hitbit` are periodic skeletons with the on/off logic of section 13; `bubble` shows a centred bubble, the first real use of `lcd_show_bubble`.

## 13. Turning tasks on and off

`rts` shows BL, HB and CS; `adbl`, `adhb` and `adcs` change them.

### Theory

- **Three ways to stop a task:**

| | How | Comes back | Here |
|---|---|---|---|
| Delete | `vTaskDelete` | no, must be created again | no: with `heap_1` the memory is never freed, so on/off cycles would leak until creation fails |
| Block | waiting on a delay, queue or semaphore | when the event happens | what a task does between jobs |
| Suspend | `vTaskSuspend` | only on `vTaskResume` | yes: "off until told otherwise" |

  A suspended task keeps its stack and TCB and uses no CPU; the scheduler never picks it.
- **Suspending another task from outside is dangerous.** The caller does not know where the task is in its code: it could freeze it while it holds the I2C mutex (the bus would stay locked and the temperature task would wait forever), or with Hit Bit's LEDs left on. Whatever was in progress stays half-done.
- **Cooperative suspension.** The console only records the wish (`bl = 0` in the config). The task checks it at a safe point, the top of its loop, where it holds no mutex; it cleans up (hides the bubble, turns the LEDs off) and suspends itself with `vTaskSuspend(NULL)`. To turn it on, the console calls `vTaskResume(handle)`; the task continues after `vTaskSuspend(NULL)`, re-reads the config and runs. The cost is up to one period of latency (100 ms for the bubble).
- **The lost wake-up race.** In "check a flag, then sleep", a wake-up that arrives between the check and the sleep is lost: the task reads `bl = 0`, the console sets `bl = 1` and calls `vTaskResume` (no effect, the task is not suspended yet), then the task suspends and nobody wakes it.
  - It cannot happen here because of priorities: the console (1) runs only when no higher-priority task is ready, and between reading `bl` and `vTaskSuspend` the bubble (3) and Hit Bit (4) tasks never block, so the console cannot run in that window.
  - The argument is fragile: giving the console a priority above these tasks would bring the race back. The robust alternative is a semaphore the task waits on, since a semaphore remembers a give that arrives early.

### Decisions

- **The handles** (`bubble_task`, `hitbit_task`) are defined in their modules, declared `extern` in their headers, and filled by `create_task(..., &handle)` in `main()`. Only the console uses them, for `vTaskResume`.
- **`vTaskResume` on a task that is not suspended does nothing,** so `adbl 1` when already on is harmless.
- **Initial states use the same path.** Hit Bit starts off (`HB = 0`): its first loop iteration reads `hb = 0` and suspends itself. No startup special case, as with the initial `A:` line.
- **`lcd_hide_bubble()`** (new message `MSG_BUBBLE_OFF`) clears the bubble square when the bubble level turns off.
- **Config sound is a mode of the buzzer task, not a task.** `adcs` stores `cs` and calls `buzzer_mode_changed()`, which puts `REQ_MODE` (0xFF) in the buzzer queue; the queue items became `uint8_t` so they can hold either an alarm source or this value. `REQ_MODE` carries no value: it means "wake up and re-read `cs`". The state lives in the config and the message is only a wake-up, so even if it were dropped (queue full), the task sees the right `cs` the next time it wakes. Same principle as the clock tick: the signal says when, the shared state says what.

## 14. Stack overflow detection and memory measurement

`hooks.cpp` holds the FreeRTOS error hooks; the debug command `stk` (not in the spec) prints each task's unused stack and the free heap.

### Theory

- **Why a stack overflow is so dangerous in an RTOS.** Each task has its own fixed-size stack from the FreeRTOS heap, holding its local variables, return addresses and saved registers. A task that needs more (deep calls, a large local array, a heavy `printf`) writes past the end. The Cortex-M3 here has no MMU, so nothing stops it: it overwrites whatever is next in memory, often another task's stack or TCB. The overflowing task may keep working while a different task crashes later, somewhere unrelated.
- **Detection (`configCHECK_FOR_STACK_OVERFLOW`)** runs when a task is switched out:
  - Method 1: is the stack pointer outside the task's stack? Catches only a task overflowing at that moment.
  - Method 2 (used here): method 1, plus a pattern check. At creation every stack is filled with `0xA5`; at each switch the kernel checks that the last 16 bytes still hold it. Overwritten means the task went past its end at some point, even if its stack pointer has since come back.
  - On failure the kernel calls `vApplicationStackOverflowHook` with the task's name.
  - Limits: it detects, it does not prevent (memory may already be corrupted, so the only safe reaction is to stop); it only checks at context switches, so an overflow that never touches the last 16 bytes can be missed; and it adds a little time to every switch.
- **High-water mark.** Because stacks start filled with `0xA5`, FreeRTOS can count how many words at the far end were never touched: the least free stack the task has ever had. `uxTaskGetSystemState` returns it for every task (`usStackHighWaterMark`). Near 0: the task came close to overflowing. Large: the stack can be smaller.

### Decisions

- **Method 2** in `FreeRTOSConfig.h` (the only config file FreeRTOS includes; `FreeRTOSConfig_b.h`, `_v7.h` and `_v9.h` are spare versions from the course that nothing includes).
- **`configUSE_MALLOC_FAILED_HOOK = 1`:** `pvPortMalloc` calls `vApplicationMallocFailedHook` when the heap is full. `create_task` and the `_init()` checks already catch failures at startup; the hook also covers any later allocation.
- **Both hooks print and stop.** `fatal()` disables interrupts, prints `FATAL: ...` and loops forever, so the corruption cannot spread and the message stays on screen.
- **`raw_puts` instead of `printf`.** When a hook runs, the system is already broken, and the stack hook runs inside the kernel's context switch. `printf` needs plenty of stack and the C library's internal state, either of which may be damaged. `raw_puts` waits for `LSR` bit 5 (transmitter empty) and writes each byte to `THR`: no interrupts, no library, almost no stack. It writes its own `\r\n` because it bypasses mbed's newline conversion.
- **`extern "C"` around the hooks.** C++ mangles function names, encoding the parameter types in the symbol (`vApplicationMallocFailedHook()` becomes something like `_Z28vApplicationMallocFailedHookv`) so that functions can be overloaded. FreeRTOS is C and calls the plain names; without `extern "C"` the link fails with "undefined reference". The built `frtos.elf` shows both hooks under their plain names.
- **`stk` uses a static `TaskStatus_t` array, not `vTaskList()`.** `vTaskList` allocates a temporary array and frees it; with `heap_1` freeing does nothing, so every call would leak about 350 bytes until the heap ran out. The static array (10 x 36 bytes) is allocated once and stays off the console's stack. Library functions have to be checked against the heap scheme before use.
- **Sizing procedure (on the board):** run every feature (all commands, alarms, on/off toggles, and later the parts' code), run `stk`, keep a margin of about 25% or at least ~50 words per task for rare paths, then shrink stacks with large margins and grow any close to 0. `free heap` replaces the ~10.2 KB estimate of section 12. The numbers change as the parts are written, so the final sizing is done with all the real code in.

## 15. Bubble level

`bubble.cpp` reads the MMA7660 accelerometer and moves the bubble on the right side of the LCD. The driver library is in `frtos/MMA7660/` (from `materials/MMA7660.zip`, without its `.hg` folder).

### Theory

- **What the sensor measures.** The MMA7660 gives the acceleration on X, Y and Z as 6-bit signed numbers (-32..31), about 21.33 counts per g. At rest the only acceleration is gravity. Flat on the table, X and Y read about 0 and Z about +21 (1 g). Tilted by an angle θ, gravity's component along X is `g·sin θ`, so X ≈ 21·sin θ counts. Near level sin θ ≈ θ, which gives roughly 2.7° per count.
- **A bubble moves toward the raised side,** opposite to where gravity pulls, so the offset on the LCD is the measured component with its sign flipped.
- **`vTaskDelay` vs `vTaskDelayUntil`.** `vTaskDelay(100)` waits 100 ms from the end of the iteration, so the real period is 100 ms plus the run time and slowly drifts. `vTaskDelayUntil` wakes at fixed multiples of the period counted from the tick, with no drift. The exact phase matters when sampling for a filter or a control loop. For a display a small drift is invisible, so the simpler call is enough.

### Decisions

- **One transaction for X and Y.** `acc.readData(xyz)` reads registers 0x00-0x02 in one I2C transfer, so X and Y come from the same sample. Calling `x()` and then `y()` could pair an old X with a new Y if the chip updated between the two reads. One transfer also holds the bus for less time, and it returns integer counts, while `x()` returns a `float` in g that we would only convert back.
- **Only the driver call inside the lock** (rule 1 of section 11). `lcd_show_bubble` comes after `i2c_unlock()`, so the temperature task never waits for the bus because of an LCD message. The driver re-reads if a register's alert bit (bit 6) says it was being updated during the read; in practice that is at most one extra read.
- **Created active** (`MMA7660 acc(p28, p27, true)`). The chip samples on its own at 64 Hz, faster than our 10 Hz reads, and a read is only the I2C transfer. In standby every read would wake it and busy-wait about 28 ms (`wait(0.012 + 1/samplerate)`) holding the mutex.
- **A `static` global object.** Only the bubble task uses the accelerometer, so the object is private to `bubble.cpp`. Its constructor writes the MODE register over I2C before `main()`, when no task exists yet, so it needs no lock.
- **Scale: 1 count = 1 pixel.** The bubble can move ±12 px, which is reached at about 0.56 g (~34°). A level has to show small tilts, and a few degrees already move it. The scale costs no multiplication.
- **No clamping in the bubble task.** The LCD task clamps the offset to ±12 px (section 9), and that rule lives in one place. `clamp()` uses plain `if` statements with early returns.
- **The axis signs are `#define`s** (`BUBBLE_SIGN_X`, `BUBBLE_SIGN_Y`). How the chip's axes line up with the LCD depends on how it sits on the Application Board, so the signs are checked on the board and flipped there if the bubble moves the wrong way.
- **Period 100 ms.** About 10 updates per second already looks smooth. Each update is an LCD message and a redraw, so a faster rate would mostly add work for the LCD task.
