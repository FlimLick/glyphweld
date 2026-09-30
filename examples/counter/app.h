#ifndef GLYPHWELD_COUNTER_APP_H
#define GLYPHWELD_COUNTER_APP_H

#include "glyphweld/gw_ui.h"

typedef struct GwCounterApp
{
  GwRuntime *runtime;
  int32_t value;
  char label[32];
} GwCounterApp;

GwResult gw_counter_build(GwRuntime *runtime, GwCounterApp *app);
void gw_counter_action(void *context, GwId action_id);

#endif
