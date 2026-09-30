#ifndef GLYPHWELD_BUILDER_APP_H
#define GLYPHWELD_BUILDER_APP_H

#include "glyphweld/gw_ui.h"

typedef struct GwBuilderApp
{
  GwRuntime *runtime;
  char status[96];
} GwBuilderApp;

GwResult gw_builder_build(GwRuntime *runtime, GwBuilderApp *app);
void gw_builder_action(void *context, GwId action_id);

#endif
