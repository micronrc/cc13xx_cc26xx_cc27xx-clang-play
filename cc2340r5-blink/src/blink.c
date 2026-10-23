#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

/* RTOS header files */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>

#include <ti/drivers/Board.h>
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"

/* Stack size in bytes */
#define THREADSTACKSIZE 1024

uint8_t led = RED_LED;

static void postSem(TimerHandle_t xTimer)
{
  SemaphoreHandle_t xSemaphore = pvTimerGetTimerID(xTimer);
  xSemaphoreGive(xSemaphore);
}

SemaphoreHandle_t setupTimer(TimerHandle_t *timer, unsigned int ms)
{
  SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();
  if (xSemaphore == NULL) {
    return (NULL);
  }

  static StaticTimer_t xTimerBuffer;
  /* Create the timer that wakes up the thread that will pend on the sem. */
  *timer = xTimerCreateStatic("ledTimer",
      pdMS_TO_TICKS(ms),  // xTimerPeriod
      pdTRUE,             // uxAutoReload
      (void *)xSemaphore, // pvTimerID
      postSem,            // pxCallbackFunction
      &xTimerBuffer
  );

  if (*timer == NULL) {
    vSemaphoreDelete(xSemaphore);
    return (NULL);
  }

  int retc = xTimerStart(*timer, 0);    // start immediateley
  if (retc != pdPASS) {
    vSemaphoreDelete(xSemaphore);
    return (NULL);
  }

  return (xSemaphore);
}


void mainThread(void *arg0)
{
  /* Configure the LED pin */
  GPIO_setConfig(led, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

  /* Turn on user LED */
  GPIO_write(led, 1);

  TimerHandle_t timerHandle;
  SemaphoreHandle_t ledSemaphore = setupTimer(&timerHandle, 250);
  if (ledSemaphore == NULL) {
    while (1) {}
  }

  for (;;) {
    int retc = xSemaphoreTake(ledSemaphore, 1000); //portMAX_DELAY);
    if (retc == pdFALSE) {
      while(1) {}
    }

    GPIO_toggle(led);
  }

  GPIO_write(led,1);
}

int main(void)
{
  Board_init();
  GPIO_init();

  TaskHandle_t gpioHandle;
  int retc = xTaskCreate(mainThread,    // code
      "gpio",                           // task name
      THREADSTACKSIZE,                  // stack depth
      NULL,                             // parameters
      1,                                // priority
      &gpioHandle);                     // created handle
  if (retc != pdPASS) {
    /* task create() failed */
    while (1) {}
  }

  vTaskStartScheduler();
  
  return 0;
}

