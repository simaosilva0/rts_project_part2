# RTS Lab Project - Part 2

Second part of the Real-Time Systems lab project. Part 1 (the PIC16F18875 monitoring and alarm system on the Curiosity HPC board) lives in [rts_project_part1](https://github.com/simaosilva0/rts_project_part1).

Group: 2
Members: Simão Silva (110486), Simão Silveira (110049), Isam Abbasi (122334)

> The spec is `Project_part2.pdf`. Where this README and the spec disagree, the spec wins.


## What part 2 is about

Part 2 moves from the cyclic executive of part 1 to a real-time kernel. FreeRTOS runs on the mbed LPC1768 (ARM Cortex-M3) mounted on the mbed Application Board, and that board talks to the PIC system from part 1 over a serial link.

Rough idea:

- FreeRTOS tasks replace the single main loop. Each job (sensors, display, command handling, communication) gets its own task with a priority, and tasks talk through queues, semaphores and mutexes instead of global flags.
- A command interface over the serial port (see `txCmdFRTOS.zip`) to read and change the system's parameters, records and clock.
- Peripherals on the Application Board:
  - LM75B: I2C temperature sensor
  - C12832: 128x32 graphic LCD (SPI)
  - MMA7660: 3-axis accelerometer (I2C)
  - On-chip RTC of the LPC1768 for date and time (`tstRTC.zip`, `ctime.c`)
- The PIC side from part 1 gets extended with the USART that part 1 left for later (9600 baud).


## Hardware

| Board | MCU | Used for |
|---|---|---|
| Curiosity HPC | PIC16F18875 | Part 1 system, now with UART |
| mbed LPC1768 + Application Board | NXP LPC1768 (Cortex-M3) | FreeRTOS, sensors, LCD, RTC |


## Tools

- MPLAB X IDE + XC8 + MCC for the PIC side (same as part 1)
- Mbed CLI 1 (mbed-os 6.16) for the LPC1768: `mbed new`, `mbed compile -m LPC1768 -t GCC_ARM`
- FreeRTOS (see the two FreeRTOS PDFs in `docs/`)
- A serial terminal (`screen`, `minicom` or `picocom`) at 9600 baud. The board shows up as `/dev/ttyACM0` on Linux, so `mbedWinSerial` isn't needed.


## Repository layout

Planned layout, filled in as work progresses:

```
docs/        datasheets, spec and FreeRTOS manuals
examples/    unzipped course examples (txCmdFRTOS, tstLM75B, tstRTC, ...)
lpc1768/     FreeRTOS application for the mbed board
pic/         part 1 PIC project extended with UART
```
