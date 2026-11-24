#include "shared.h"
#include "semphr.h"

static config_t cfg;                 /* the only copy; reached only through the functions below */
static SemaphoreHandle_t cfg_mutex;

BaseType_t config_init(void)
{
    cfg.pmon  = 5;
    cfg.tala  = 3;
    cfg.tlow  = 15;
    cfg.thigh = 30;
    cfg.alat  = 0;
    cfg.alac  = 0;
    cfg.alah  = 12;
    cfg.alam  = 0;
    cfg.alas  = 0;
    cfg.bl    = 1;
    cfg.hb    = 0;
    cfg.cs    = 0;

    cfg_mutex = xSemaphoreCreateMutex();
    return (cfg_mutex != NULL) ? pdPASS : pdFAIL;
}

void config_get(config_t *out)
{
    xSemaphoreTake(cfg_mutex, portMAX_DELAY);
    *out = cfg;
    xSemaphoreGive(cfg_mutex);
}

void config_set(const config_t *in)
{
    xSemaphoreTake(cfg_mutex, portMAX_DELAY);
    cfg = *in;
    xSemaphoreGive(cfg_mutex);
}
