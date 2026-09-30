#include "test.h"

int main(void)
{
  int failed =
      test_layout() + test_render() + test_glyph_run() + test_counter() + test_events_and_tree();
  if (failed != 0)
    return 1;
  puts("all Glyphweld tests passed");
  return 0;
}
