#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "init.h"

#include <winsock2.h>

#include "error.h"

int init(void)
{
  WSADATA data;
  int r = WSAStartup(MAKEWORD(2, 2), &data);
  return -r;
}

void deinit(void)
{
  int saved = WSAGetLastError();

  int r = WSACleanup();
  ASSERT_UNUSED(r == 0);

  WSASetLastError(saved);
}
