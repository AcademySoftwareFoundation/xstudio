#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "clock.h"

#include <windows.h>

int64_t now(void)
{
  return (int64_t) GetTickCount64();
}
