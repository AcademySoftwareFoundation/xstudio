#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "handle.h"

#include <windows.h>

#include "error.h"

const HANDLE HANDLE_INVALID = INVALID_HANDLE_VALUE; // NOLINT

// `handle_cloexec` is POSIX-only.

HANDLE handle_destroy(HANDLE handle)
{
  if (handle == NULL || handle == HANDLE_INVALID) {
    return HANDLE_INVALID;
  }

  int r = CloseHandle(handle);
  ASSERT_UNUSED(r != 0);

  return HANDLE_INVALID;
}
