#ifndef SHARED_H
#define SHARED_H

#include <stdint.h>
#include "FreeRTOS.h"

/* System parameters and task states
   Rule: only the console task writes them; any task may read them. */
typedef struct {
    uint8_t pmon;   /* monitoring period, s (0..99, 0 = no periodic reads) */
    uint8_t tala;   /* alarm sound duration, s (0..60) */
    uint8_t tlow;   /* temperature threshold low, C (0..50) */
    uint8_t thigh;  /* temperature threshold high, C (0..50) */
    uint8_t alat;   /* temperature alarm enabled (0/1) */
    uint8_t alac;   /* alarm clock enabled (0/1) */
    uint8_t alah;   /* alarm clock hours (0..23) */
    uint8_t alam;   /* alarm clock minutes (0..59) */
    uint8_t alas;   /* alarm clock seconds (0..59) */
    uint8_t bl;     /* Bubble Level task active (0/1) */
    uint8_t hb;     /* Hit Bit task active (0/1) */
    uint8_t cs;     /* Config Sound active (0/1) */
} config_t;

BaseType_t config_init(void);           /* call once in main(), before the scheduler */
void config_get(config_t *out);         /* copy the whole config out */
void config_set(const config_t *in);    /* replace the whole config */

#endif
