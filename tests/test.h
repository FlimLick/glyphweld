#ifndef GLYPHWELD_TEST_H
#define GLYPHWELD_TEST_H

#include <stdio.h>

#define TEST_CHECK(expression)                                                                     \
  do                                                                                               \
  {                                                                                                \
    if (!(expression))                                                                             \
    {                                                                                              \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                             \
      return 1;                                                                                    \
    }                                                                                              \
  } while (0)

int test_layout(void);
int test_render(void);
int test_glyph_run(void);
int test_counter(void);
int test_events_and_tree(void);
int test_x11_host(void);
int test_wayland_host(void);

#endif
