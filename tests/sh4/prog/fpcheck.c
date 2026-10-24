/* Test program: coredll's floating-point functions called directly and
   snprintf "%f" (which gets its digits from coredll's _ecvt/_fcvt).  On
   SH-4, coredll is /Qsh4 code that takes and returns double in FPU
   registers; mingwrt's thunks convert from -m4-nofpu (docs/sh4/ABI.md).
   Imports only coredll.  The message box reads PASS when every value is
   as expected, and names the first failing check otherwise.  */
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { failed = #c; goto done; } } while (0)

int WINAPI
WinMain (HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
  /* Volatile pointers, so the compiler neither folds nor expands the
     calls.  */
  double (*volatile p_sqrt) (double) = sqrt;
  double (*volatile p_floor) (double) = floor;
  double (*volatile p_pow) (double, double) = pow;
  double (*volatile p_fmod) (double, double) = fmod;
  double (*volatile p_ldexp) (double, int) = ldexp;
  double (*volatile p_frexp) (double, int *) = frexp;
  double (*volatile p_modf) (double, double *) = modf;
  char *(*volatile p_ecvt) (double, int, int *, int *) = _ecvt;
  volatile double pi = 3.14159;
  const char *failed = NULL;
  char out[64] = "";
  WCHAR buf[256];
  int e = 0, dec = 0, sign = 1;
  double ip = 0;
  const char *dig;

  CHECK (p_sqrt (2.25) == 1.5);
  CHECK (p_floor (-2.5) == -3.0);
  CHECK (p_pow (2.0, 10.0) == 1024.0);
  CHECK (p_fmod (7.5, 2.0) == 1.5);
  CHECK (p_ldexp (0.75, 4) == 12.0);
  CHECK (p_frexp (12.0, &e) == 0.75 && e == 4);
  CHECK (p_modf (3.25, &ip) == 0.25 && ip == 3.0);
  dig = p_ecvt (3.14159, 3, &dec, &sign);
  CHECK (strcmp (dig, "314") == 0 && dec == 1 && sign == 0);
  snprintf (out, sizeof out, "%5.2f|%.3e", (double) pi, (double) pi);
  CHECK (strcmp (out, " 3.14|3.142e+00") == 0);

done:
  wsprintfW (buf, L"%s\n%hs\nsnprintf: %hs", failed ? L"FAIL" : L"PASS",
	     failed ? failed : "coredll double functions", out);
  MessageBoxW (NULL, buf, L"cegcc fpcheck", MB_OK);
  return failed ? 1 : 0;
}
