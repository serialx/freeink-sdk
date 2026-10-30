#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
inline bool failPanelPsram = false;
inline void* heap_caps_malloc(size_t size, int caps) {
  return failPanelPsram && (caps & MALLOC_CAP_SPIRAM) ? nullptr : malloc(size);
}
