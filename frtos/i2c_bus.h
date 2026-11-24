#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "FreeRTOS.h"

/* The LM75B (temperature) and the MMA7660 (accelerometer) share one I2C bus
   (p28/p27). Every transfer on it must happen between i2c_lock() and i2c_unlock(). */
BaseType_t i2c_bus_init(void);      /* call once in main(), before the scheduler */
void i2c_lock(void);
void i2c_unlock(void);

#endif