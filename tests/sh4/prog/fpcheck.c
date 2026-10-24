/* Test program: coredll's floating-point functions called directly and
   snprintf "%f" (which gets its digits from coredll's _ecvt/_fcvt).  On
   SH-4, coredll is /Qsh4 code that takes and returns double in FPU
   registers; mingwrt's thunks convert from -m4-nofpu (docs/sh4/ABI.md).
   Imports only coredll.

   Output as in tests/gcc16/cetest.cpp: one line per check,
     PASS <name>
     FAIL <name>: <reason>
   and a last line
     DONE <pass>/<run>
   The lines go to OutputDebugStringW and to fpcheck.txt next to the EXE,
   written line by line.  The lines are put together without printf,
   which is under test.  */
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HANDLE log_file = INVALID_HANDLE_VALUE;
static int pass, run;

static void
out (const char *line)
{
  WCHAR w[300];
  DWORD written;
  int n = MultiByteToWideChar (CP_ACP, 0, line, -1, w, 298);

  if (n <= 0)
    n = 1;
  w[n - 1] = L'\n';
  w[n] = 0;
  OutputDebugStringW (w);
  if (log_file != INVALID_HANDLE_VALUE)
    {
      WriteFile (log_file, line, strlen (line), &written, NULL);
      WriteFile (log_file, "\r\n", 2, &written, NULL);
      FlushFileBuffers (log_file);
    }
}

/* FAIL lines name the failed condition, and GOT if not NULL.  */
static void
report (const char *name, const char *cond, const char *got)
{
  char line[256];

  run++;
  if (!cond)
    {
      pass++;
      strcpy (line, "PASS ");
      strcat (line, name);
    }
  else
    {
      strcpy (line, "FAIL ");
      strcat (line, name);
      strcat (line, ": ");
      strncat (line, cond, 120);
      if (got)
	{
	  strcat (line, ", got \"");
	  strncat (line, got, 64);
	  strcat (line, "\"");
	}
    }
  out (line);
}

static void
append_int (char *s, int v)
{
  char d[12];
  int i = 0;

  do
    d[i++] = '0' + v % 10;
  while ((v /= 10) > 0);
  s += strlen (s);
  while (i > 0)
    *s++ = d[--i];
  *s = 0;
}

#define CHECK(name, c) report (name, (c) ? NULL : #c, NULL)

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
  WCHAR path[MAX_PATH + 16];
  char done[32], buf[64] = "";
  int e = 0, dec = 0, sign = 1;
  double ip = 0;
  const char *dig;
  DWORD n;

  /* fpcheck.txt next to the EXE.  */
  n = GetModuleFileNameW (NULL, path, MAX_PATH);
  while (n > 0 && path[n - 1] != L'\\')
    n--;
  wcscpy (path + n, L"fpcheck.txt");
  log_file = CreateFileW (path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  out ("fpcheck GCC " __VERSION__
#if defined(__arm__)
       ", arm"
#else
       ", sh4"
#endif
       );

  CHECK ("sqrt", p_sqrt (2.25) == 1.5);
  CHECK ("floor", p_floor (-2.5) == -3.0);
  CHECK ("pow", p_pow (2.0, 10.0) == 1024.0);
  CHECK ("fmod", p_fmod (7.5, 2.0) == 1.5);
  CHECK ("ldexp", p_ldexp (0.75, 4) == 12.0);
  CHECK ("frexp", p_frexp (12.0, &e) == 0.75 && e == 4);
  CHECK ("modf", p_modf (3.25, &ip) == 0.25 && ip == 3.0);
  dig = p_ecvt (3.14159, 3, &dec, &sign);
  report ("_ecvt",
	  strcmp (dig, "314") == 0 && dec == 1 && sign == 0
	  ? NULL : "_ecvt (3.14159, 3) == \"314\", dec 1, sign 0", dig);
  snprintf (buf, sizeof buf, "%5.2f|%.3e", (double) pi, (double) pi);
  /* Windows CE's CRT, like Microsoft's on the desktop, prints a
     three-digit exponent.  */
  report ("snprintf %f %e", strcmp (buf, " 3.14|3.142e+00") == 0
	  || strcmp (buf, " 3.14|3.142e+000") == 0
	  ? NULL : "\" 3.14|3.142e+00\" or \" 3.14|3.142e+000\"", buf);

  /* "DONE pass/run" without printf.  */
  strcpy (done, "DONE ");
  append_int (done, pass);
  strcat (done, "/");
  append_int (done, run);
  out (done);
  if (log_file != INVALID_HANDLE_VALUE)
    CloseHandle (log_file);
  return run - pass;
}
