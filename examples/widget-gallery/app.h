#ifndef GLYPHWELD_WIDGET_GALLERY_APP_H
#define GLYPHWELD_WIDGET_GALLERY_APP_H

#include "glyphweld/gw_ui.h"

typedef struct GwWidgetGalleryApp
{
  GwRuntime *runtime;
  char status[64];
} GwWidgetGalleryApp;

GwResult gw_widget_gallery_build(GwRuntime *runtime, GwWidgetGalleryApp *app);
void gw_widget_gallery_action(void *context, GwId action_id);

#endif
