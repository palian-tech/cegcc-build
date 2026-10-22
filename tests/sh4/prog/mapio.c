/* M4 test program: imports MmMapIoSpace/MmUnmapIoSpace from ceddk.dll.
   It maps one page of physical memory, does not touch it, unmaps it and
   shows the virtual address it got.  */
#include <windows.h>
#include <ceddk.h>

int WINAPI
WinMain (HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
  WCHAR buf[128];
  PHYSICAL_ADDRESS pa;
  PVOID va;

  pa.QuadPart = 0;
  va = MmMapIoSpace (pa, 0x1000, FALSE);
  if (va)
    MmUnmapIoSpace (va, 0x1000);
  wsprintfW (buf, L"MmMapIoSpace(0, 0x1000) = %08lx", (unsigned long) va);
  MessageBoxW (NULL, buf, L"cegcc mapio", MB_OK);
  return va ? 0 : 1;
}
