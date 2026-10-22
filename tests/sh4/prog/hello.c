/* M4 test program: WinMain, MessageBoxW, wsprintfW, a struct return, a
   64-bit argument and signed/unsigned division.  Imports only coredll.
   The message box reads PASS when every computed value is as expected.  */
#include <windows.h>

struct divmod { int q, r; };		/* 8 bytes: hidden pointer in r4 */
struct udivmod { unsigned q, r; };

/* Not constants, so the compiler cannot fold the arithmetic away.  */
volatile int sv = -1234567;
volatile int sd = 89;
volatile unsigned uv = 4000000000u;
volatile unsigned ud = 7;
volatile long long lv = 0x123456789abcdefLL;

static __attribute__ ((noinline)) struct divmod
sdiv (int a, int b)
{
  struct divmod r = { a / b, a % b };
  return r;
}

static __attribute__ ((noinline)) struct udivmod
udiv (unsigned a, unsigned b)
{
  struct udivmod r = { a / b, a % b };
  return r;
}

/* The 64-bit argument starts in r7 and is split with the stack.  */
static __attribute__ ((noinline)) long long
mix64 (int a, int b, int c, long long v, int d)
{
  return v + a + b + c + d;
}

int WINAPI
WinMain (HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
  WCHAR buf[256];
  struct divmod s = sdiv (sv, sd);
  struct udivmod u = udiv (uv, ud);
  long long m = mix64 (1, 2, 3, lv, 4);
  int ok = s.q == -13871 && s.r == -48
	   && u.q == 571428571u && u.r == 3u
	   && m == 0x123456789abcdefLL + 10;

  wsprintfW (buf, L"%s\nsdiv %d %d\nudiv %u %u\nmix64 %08lx%08lx",
	     ok ? L"PASS" : L"FAIL", s.q, s.r, u.q, u.r,
	     (unsigned long) (m >> 32), (unsigned long) m);
  MessageBoxW (NULL, buf, L"cegcc hello", MB_OK);
  return ok ? 0 : 1;
}
