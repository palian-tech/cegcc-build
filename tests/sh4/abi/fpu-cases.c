/* Floating-point calling convention of the Microsoft SH-4 compiler with
   /Qsh4 (Gate 5, docs/gcc16/SPEC.md).  What fl, callfl and calldbl
   already show is in docs/sh4/ABI.md; this file asks the rest.

   Compile:  clsh /Qsh4 /O2 /c fpu-cases.c   and disassemble with the FPU
   instructions decoded, like docs/sh4/msvc-sh4-abi-open-questions.txt.

   Each question has a caller (call_*), which passes distinct constants so
   that each value can be found in a register or stack slot, and a callee
   (take_*), which shows where it reads its arguments.  The constants are
   exact in float: 1.5f, 2.5f, ... and 1.25, 2.25, ...  */

#include <stdarg.h>

typedef struct { float x, y; } F2;
typedef struct { float x, y, z, w; } F4;
typedef struct { double d; } D1;
typedef struct { float f; int i; } FI;

extern float  ext_f (float a);
extern float  ext_ff (float a, float b);
extern float  ext_if (int a, float b);
extern int    ext_fi (float a, int b);
extern double ext_id (int a, double b);
extern double ext_fd (float a, double b);
extern double ext_ddd (double a, double b, double c);
extern float  ext_f8 (float a, float b, float c, float d,
		      float e, float f, float g, float h);
extern float  ext_f10 (float a, float b, float c, float d, float e,
		       float f, float g, float h, float i, float j);
extern double ext_d5 (double a, double b, double c, double d, double e);
extern double ext_mix (int a, float b, int c, double d, float e, int f);
extern float  ext_iiiif (int a, int b, int c, int d, float e);
extern double ext_ld (__int64 a, double b);
extern float  ext_lf (__int64 a, float b);
extern int    extv (const char *fmt, ...);
extern F2     ext_F2 (F2 a);
extern F4     ext_F4 (F4 a);
extern D1     ext_D1 (D1 a);
extern FI     ext_FI (FI a);
extern void   ext_void (void);
extern float  (*ext_pf) (float a, double b);

/* 1. First float argument and float result.  */
float call_f (void) { return ext_f (1.5f) + 1.0f; }
float take_f (float a) { return a * 2.0f; }

/* 2. Two floats.  */
float call_ff (void) { return ext_ff (1.5f, 2.5f); }
float take_ff (float a, float b) { return a - b; }

/* 3. int, then float: fr4 or fr5?  */
float call_if (void) { return ext_if (7, 2.5f); }
float take_if (int a, float b) { return (float) a + b; }

/* 4. float, then int: r4 or r5?  */
int call_fi (void) { return ext_fi (1.5f, 7); }
int take_fi (float a, int b) { return (int) a + b; }

/* 5. int, then double: dr4 or dr6, and which int slots?  */
double call_id (void) { return ext_id (7, 2.25); }
double take_id (int a, double b) { return (double) a * b; }

/* 6. float, then double.  */
double call_fd (void) { return ext_fd (1.5f, 2.25); }
double take_fd (float a, double b) { return (double) a - b; }

/* 7. Three doubles: the third beyond the four int slots.  */
double call_ddd (void) { return ext_ddd (1.25, 2.25, 3.25); }
double take_ddd (double a, double b, double c) { return a * b - c; }

/* 8. Eight and ten floats: FPU registers fr4..fr11, then the stack?  */
float call_f8 (void)
{
  return ext_f8 (1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f);
}
float take_f8 (float a, float b, float c, float d,
	       float e, float f, float g, float h)
{
  return a - b + c - d + e - f + g - h;
}
float call_f10 (void)
{
  return ext_f10 (1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f, 9.5f,
		  10.5f);
}
float take_f10 (float a, float b, float c, float d, float e,
		float f, float g, float h, float i, float j)
{
  return a - b + c - d + e - f + g - h + i - j;
}

/* 9. Five doubles.  */
double call_d5 (void) { return ext_d5 (1.25, 2.25, 3.25, 4.25, 5.25); }
double take_d5 (double a, double b, double c, double d, double e)
{
  return a - b + c - d + e;
}

/* 10. Mixed int, float, double.  */
double call_mix (void) { return ext_mix (7, 1.5f, 8, 2.25, 2.5f, 9); }
double take_mix (int a, float b, int c, double d, float e, int f)
{
  return a + b + c + d + e + f;
}

/* 11. A float after four ints (stack).  */
float call_iiiif (void) { return ext_iiiif (1, 2, 3, 4, 2.5f); }
float take_iiiif (int a, int b, int c, int d, float e)
{
  return (float) (a + b + c + d) + e;
}

/* 12. __int64 with double and float.  */
double call_ld (void) { return ext_ld ((__int64) 7 << 33, 2.25); }
double take_ld (__int64 a, double b) { return (double) a + b; }
float call_lf (void) { return ext_lf ((__int64) 7 << 33, 2.5f); }
float take_lf (__int64 a, float b) { return (float) a + b; }

/* 13. Varargs: float (promoted to double) and double, with ints around
   them, from the caller and from a callee using va_arg.  */
int call_v (void) { return extv ("x", 7, 1.5f, 8, 2.25, 9); }
int call_v2 (void) { return extv ("x", 1.25, 2.25, 3.25); }
double take_v (const char *fmt, ...)
{
  va_list ap;
  int a;
  double b, c;

  va_start (ap, fmt);
  a = va_arg (ap, int);
  b = va_arg (ap, double);
  c = va_arg (ap, double);
  va_end (ap);
  return a + b + c;
}

/* 14. Structs of floats and doubles by value and as results.  */
float call_F2 (void) { F2 s; s.x = 1.5f; s.y = 2.5f; return ext_F2 (s).y; }
F2 take_F2 (F2 a) { F2 r; r.x = a.y; r.y = a.x + 1.0f; return r; }
float call_F4 (void)
{
  F4 s; s.x = 1.5f; s.y = 2.5f; s.z = 3.5f; s.w = 4.5f;
  return ext_F4 (s).w;
}
F4 take_F4 (F4 a) { F4 r; r.x = a.w; r.y = a.z; r.z = a.y; r.w = a.x; return r; }
double call_D1 (void) { D1 s; s.d = 2.25; return ext_D1 (s).d; }
D1 take_D1 (D1 a) { D1 r; r.d = a.d * 2.0; return r; }
int call_FI (void) { FI s; s.f = 1.5f; s.i = 7; return ext_FI (s).i; }
FI take_FI (FI a) { FI r; r.f = (float) a.i; r.i = (int) a.f; return r; }

/* 15. Callee-saved FPU registers: values live across a call.  */
double keep_d (double a, double b)
{
  double x = a * b, y = a + b, z = a - b, w = a / b;
  ext_void ();
  return x + y * z - w;
}
float keep_f (float a, float b)
{
  float v0 = a + b, v1 = a - b, v2 = a * b, v3 = a / b;
  float v4 = v0 * v1, v5 = v1 * v2, v6 = v2 * v3, v7 = v3 * v0;
  float v8 = v4 + v5, v9 = v6 + v7, v10 = v4 - v6, v11 = v5 - v7;
  ext_void ();
  return v0 + v1 * v2 - v3 + v4 * v5 - v6 + v7 * v8 - v9 + v10 * v11;
}

/* 16. FPSCR across a call when the caller computes in double and in
   float around it.  */
float mixed_prec (float a, double b)
{
  double d = (double) a * b;
  float f = a * a;
  ext_void ();
  return (float) d + f;
}

/* 17. Through a pointer.  */
float call_ptr (void) { return ext_pf (1.5f, 2.25); }

/* 18. Conversions (inline or helper calls?).  */
int d2i (double d) { return (int) d; }
unsigned d2u (double d) { return (unsigned) d; }
__int64 d2l (double d) { return (__int64) d; }
double i2d (int i) { return (double) i; }
double u2d (unsigned u) { return (double) u; }
double l2d (__int64 l) { return (double) l; }
float d2f (double d) { return (float) d; }
double f2d (float f) { return (double) f; }

/* 19. Comparisons.  */
int dlt (double a, double b) { return a < b; }
int fge (float a, float b) { return a >= b; }
int dne (double a, double b) { return a != b; }
