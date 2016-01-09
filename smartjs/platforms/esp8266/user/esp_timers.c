#include <stdlib.h>

#include <v7.h>
#include <sj_timers.h>
#include <sj_v7_ext.h>

#include "esp_missing_includes.h"
#include "v7_esp.h"

#ifndef RTOS_SDK

#include <osapi.h>
#include <os_type.h>

#else

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#endif /* RTOS_SDK */

#ifdef RTOS_SDK
struct timer_info {
  xTimerHandle t;
  v7_val_t *cb;
};

/* Espressif broke callback API for no reason with this change in timers.c:
 *   //pxTimer->pxCallbackFunction( ( xTimerHandle ) pxTimer );
 *     pxTimer->pxCallbackFunction( ( void * ) (pxTimer->pvTimerID) );
 * So, instead of the timer handle we get ID as the argument. This makes it
 * resemble os_timer API more, but it forces us to use additional struct. */

static void sj_timer_callback(xTimerHandle t) {
  struct timer_info *ti = (struct timer_info *) t;
  xTimerDelete(ti->t, 0);
  sj_invoke_cb0(v7, *ti->cb);
  v7_disown(v7, ti->cb);
  free(ti->cb);
  free(ti);
}

void sj_set_timeout(int msecs, v7_val_t *cb) {
  struct timer_info *ti = calloc(1, sizeof(*ti));
  ti->t =
      xTimerCreate((const signed char *) "js_timeout", msecs / portTICK_RATE_MS,
                   pdFALSE /* repeat */, ti, sj_timer_callback);
  ti->cb = cb;
  xTimerStart(ti->t, 0);
}

#else /* OSS SDK */

struct timer_info {
  os_timer_t t;
  v7_val_t *js_cb;
  timer_callback c_cb;
  void *c_cb_param;
};

void esp_timer_callback(void *arg) {
  struct timer_info *ti = (struct timer_info *) arg;

  if (ti->js_cb != NULL) {
    sj_invoke_cb0(v7, *ti->js_cb);
    v7_disown(v7, ti->js_cb);
    free(ti->js_cb);
  }

  if (ti->c_cb != NULL) {
    ti->c_cb(ti->c_cb_param);
  }

  free(ti);
}

static void esp_set_timeout(int msecs, struct timer_info *ti) {
  os_timer_disarm(&ti->t);
  os_timer_setfn(&ti->t, esp_timer_callback, ti);
  os_timer_arm(&ti->t, msecs, 0 /* repeat */);
}

void sj_set_timeout(int msecs, v7_val_t *cb) {
  struct timer_info *ti = calloc(1, sizeof(*ti));
  ti->js_cb = cb;
  esp_set_timeout(msecs, ti);
}

void sj_set_c_timeout(int msecs, timer_callback cb, void *param) {
  struct timer_info *ti = calloc(1, sizeof(*ti));
  ti->c_cb = cb;
  ti->c_cb_param = param;
  esp_set_timeout(msecs, ti);
}

#endif /* !RTOS_SDK */