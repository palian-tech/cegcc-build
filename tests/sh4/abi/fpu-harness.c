/* Runtime ABI test of -m4-single for sh4-mingw32ce: the FPU calling
   convention of the Microsoft SH-4 compiler with /Qsh4.

   The functions of tests/sh4/abi/ref/msvc-sh4-fpu-cases.txt (fpu-cases.c
   compiled with clsh /Qsh4 /O2) are re-assembled by msvc2gas.py and
   linked with this file, compiled by sh4-mingw32ce-gcc -m4-single.
   Calls go both ways:

   - GCC -> MSVC: this file calls take_*, keep_*, d2i, ... and checks
     their results.
   - MSVC -> GCC: the MSVC callers call_* call ext_*, extv, ext_void and
     the coredll helpers __i64tod, __i64tos and __dtoi64 defined here,
     which record what they received.

   Every call runs between canaries with the stack pointer compared, as
   in harness.c, and FPSCR.PR and FPSCR.SZ must be 0 at every call and
   return.  run-fpu-abi-test.sh runs it.  */

typedef struct { float x, y; } F2;
typedef struct { float x, y, z, w; } F4;
typedef struct { double d; } D1;
typedef struct { float f; int i; } FI;
typedef long long i64;
typedef unsigned long long u64;

/* Compiled by the Microsoft SH-4 compiler with /Qsh4.  */
extern float call_f (void);
extern float take_f (float a);
extern float call_ff (void);
extern float take_ff (float a, float b);
extern float call_if (void);
extern float take_if (int a, float b);
extern int call_fi (void);
extern int take_fi (float a, int b);
extern double call_id (void);
extern double take_id (int a, double b);
extern double call_fd (void);
extern double take_fd (float a, double b);
extern double call_ddd (void);
extern double take_ddd (double a, double b, double c);
extern float call_f8 (void);
extern float take_f8 (float a, float b, float c, float d,
		      float e, float f, float g, float h);
extern float call_f10 (void);
extern float take_f10 (float a, float b, float c, float d, float e,
		       float f, float g, float h, float i, float j);
extern double call_d5 (void);
extern double take_d5 (double a, double b, double c, double d, double e);
extern double call_mix (void);
extern double take_mix (int a, float b, int c, double d, float e, int f);
extern float call_iiiif (void);
extern float take_iiiif (int a, int b, int c, int d, float e);
extern double call_ld (void);
extern double take_ld (i64 a, double b);
extern float call_lf (void);
extern float take_lf (i64 a, float b);
extern int call_v (void);
extern int call_v2 (void);
extern double take_v (const char *fmt, ...);
extern float call_F2 (void);
extern F2 take_F2 (F2 a);
extern float call_F4 (void);
extern F4 take_F4 (F4 a);
extern double call_D1 (void);
extern D1 take_D1 (D1 a);
extern int call_FI (void);
extern FI take_FI (FI a);
extern double keep_d (double a, double b);
extern float keep_f (float a, float b);
extern float mixed_prec (float a, double b);
extern float call_ptr (void);
extern int d2i (double d);
extern unsigned d2u (double d);
extern i64 d2l (double d);
extern double i2d (int i);
extern double u2d (unsigned u);
extern double l2d (i64 l);
extern float d2f (double d);
extern double f2d (float f);
extern int dlt (double a, double b);
extern int fge (float a, float b);
extern int dne (double a, double b);

extern int sys_write (int fd, const char *buf, int len);

static int failures;
static int checks;

static void
put (const char *s)
{
  int n = 0;
  while (s[n])
    n++;
  sys_write (1, s, n);
}

static void
put_hex (u64 v)
{
  char buf[19];
  buf[0] = '0';
  buf[1] = 'x';
  for (int i = 0; i < 16; i++)
    buf[2 + i] = "0123456789abcdef"[(v >> (60 - 4 * i)) & 15];
  buf[18] = 0;
  put (buf);
}

static void
check (const char *what, u64 got, u64 want)
{
  checks++;
  if (got == want)
    return;
  failures++;
  put ("FAIL ");
  put (what);
  put (": got ");
  put_hex (got);
  put (", want ");
  put_hex (want);
  put ("\n");
}

/* Doubles and floats are compared bit for bit; every value in this test
   is exact.  */
static u64
dbits (double d)
{
  union { double d; u64 u; } u;
  u.d = d;
  return u.u;
}

static u64
fbits (float f)
{
  union { float f; unsigned u; } u;
  u.f = f;
  return u.u;
}

#define check_d(what, got, want) check (what, dbits (got), dbits (want))
#define check_f(what, got, want) check (what, fbits (got), fbits (want))

static inline unsigned
get_sp (void)
{
  unsigned sp;
  __asm__ volatile ("mov r15,%0" : "=r" (sp));
  return sp;
}

static inline unsigned
get_fpscr (void)
{
  unsigned v;
  __asm__ volatile ("sts fpscr,%0" : "=r" (v));
  return v;
}

/* FPSCR.PR and FPSCR.SZ.  */
#define FPSCR_PR_SZ 0x180000

static void
check_fpscr (const char *what)
{
  check (what, get_fpscr () & FPSCR_PR_SZ, 0);
}

/* What the GCC callees received from the MSVC callers, as doubles (all
   values are exact).  */
static double rec[12];
static int nrec;
static const char *rec_str;

static void
start (const char *name)
{
  nrec = 0;
  check_fpscr (name);
}

static void
record (double v)
{
  if (nrec < 12)
    rec[nrec] = v;
  nrec++;
}

float
ext_f (float a)
{
  start ("ext_f: FPSCR at entry");
  record (a);
  return a * 4.0f;
}

float
ext_ff (float a, float b)
{
  start ("ext_ff: FPSCR at entry");
  record (a);
  record (b);
  return a - b;
}

float
ext_if (int a, float b)
{
  start ("ext_if: FPSCR at entry");
  record (a);
  record (b);
  return a + b;
}

int
ext_fi (float a, int b)
{
  start ("ext_fi: FPSCR at entry");
  record (a);
  record (b);
  return (int) (a * 2.0f) + b * 100;
}

double
ext_id (int a, double b)
{
  start ("ext_id: FPSCR at entry");
  record (a);
  record (b);
  return a * b;
}

double
ext_fd (float a, double b)
{
  start ("ext_fd: FPSCR at entry");
  record (a);
  record (b);
  return a - b;
}

double
ext_ddd (double a, double b, double c)
{
  start ("ext_ddd: FPSCR at entry");
  record (a);
  record (b);
  record (c);
  return a * b - c;
}

float
ext_f8 (float a, float b, float c, float d,
	float e, float f, float g, float h)
{
  start ("ext_f8: FPSCR at entry");
  record (a);
  record (b);
  record (c);
  record (d);
  record (e);
  record (f);
  record (g);
  record (h);
  return a + h;
}

float
ext_f10 (float a, float b, float c, float d, float e,
	 float f, float g, float h, float i, float j)
{
  start ("ext_f10: FPSCR at entry");
  record (a);
  record (b);
  record (c);
  record (d);
  record (e);
  record (f);
  record (g);
  record (h);
  record (i);
  record (j);
  return i - j;
}

double
ext_d5 (double a, double b, double c, double d, double e)
{
  start ("ext_d5: FPSCR at entry");
  record (a);
  record (b);
  record (c);
  record (d);
  record (e);
  return e - a;
}

double
ext_mix (int a, float b, int c, double d, float e, int f)
{
  start ("ext_mix: FPSCR at entry");
  record (a);
  record (b);
  record (c);
  record (d);
  record (e);
  record (f);
  return f * 1000 + d;
}

float
ext_iiiif (int a, int b, int c, int d, float e)
{
  start ("ext_iiiif: FPSCR at entry");
  record (a);
  record (b);
  record (c);
  record (d);
  record (e);
  return e * 2.0f;
}

double
ext_ld (i64 a, double b)
{
  start ("ext_ld: FPSCR at entry");
  record ((int) (a >> 32));
  record ((int) a);
  record (b);
  return b * 4.0;
}

float
ext_lf (i64 a, float b)
{
  start ("ext_lf: FPSCR at entry");
  record ((int) (a >> 32));
  record ((int) a);
  record (b);
  return b * 4.0f;
}

/* call_v:  extv ("x", 7, 1.5f, 8, 2.25, 9)
   call_v2: extv ("x", 1.25, 2.25, 3.25)  */
static int v_doubles;

int
extv (const char *fmt, ...)
{
  __builtin_va_list ap;
  start ("extv: FPSCR at entry");
  rec_str = fmt;
  __builtin_va_start (ap, fmt);
  if (v_doubles)
    for (int i = 0; i < 3; i++)
      record (__builtin_va_arg (ap, double));
  else
    {
      record (__builtin_va_arg (ap, int));
      record (__builtin_va_arg (ap, double));
      record (__builtin_va_arg (ap, int));
      record (__builtin_va_arg (ap, double));
      record (__builtin_va_arg (ap, int));
    }
  __builtin_va_end (ap);
  return 42;
}

F2
ext_F2 (F2 a)
{
  F2 r;
  start ("ext_F2: FPSCR at entry");
  record (a.x);
  record (a.y);
  r.x = a.y;
  r.y = a.x * 4.0f;
  return r;
}

F4
ext_F4 (F4 a)
{
  F4 r;
  start ("ext_F4: FPSCR at entry");
  record (a.x);
  record (a.y);
  record (a.z);
  record (a.w);
  r.x = a.w;
  r.y = a.z;
  r.z = a.y;
  r.w = a.x * 4.0f;
  return r;
}

D1
ext_D1 (D1 a)
{
  D1 r;
  start ("ext_D1: FPSCR at entry");
  record (a.d);
  r.d = a.d * 4.0;
  return r;
}

FI
ext_FI (FI a)
{
  FI r;
  start ("ext_FI: FPSCR at entry");
  record (a.f);
  record (a.i);
  r.f = a.i;
  r.i = (int) (a.f * 2.0f) + a.i * 100;
  return r;
}

static float
pf_impl (float a, double b)
{
  start ("ext_pf: FPSCR at entry");
  record (a);
  record (b);
  return a * 4.0f;
}

float (*ext_pf) (float a, double b) = pf_impl;

/* Clobbers every call-clobbered FPU register and uses fr12..fr15, which
   GCC must save for the MSVC callers keep_d and keep_f.  Runs double
   arithmetic, so it sets FPSCR.PR inside.  */
static volatile double vd = 3.0;
static int nvoid;

void
ext_void (void)
{
  start ("ext_void: FPSCR at entry");
  nvoid++;
  vd = vd * 1.0;
  __asm__ volatile ("fldi0 fr0\n\tfldi1 fr1\n\tfldi0 fr2\n\tfldi1 fr3\n\t"
		    "fldi0 fr4\n\tfldi1 fr5\n\tfldi0 fr6\n\tfldi1 fr7\n\t"
		    "fldi0 fr8\n\tfldi1 fr9\n\tfldi0 fr10\n\tfldi1 fr11\n\t"
		    "fldi0 fr12\n\tfldi1 fr13\n\tfldi0 fr14\n\tfldi1 fr15"
		    : : : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6",
		      "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
		      "fr14", "fr15");
}

/* coredll's __int64 helpers, as called by the /Qsh4 code: the __int64
   operand by address (take_ld, take_lf, l2d), the __int64 result through
   the hidden pointer (d2l).  */
static int ni64tod, ni64tos, ndtoi64;

double
_i64tod (const i64 *p)
{
  start ("__i64tod: FPSCR at entry");
  ni64tod++;
  return (double) *p;
}

float
_i64tos (const i64 *p)
{
  start ("__i64tos: FPSCR at entry");
  ni64tos++;
  return (float) *p;
}

i64
_dtoi64 (double d)
{
  start ("__dtoi64: FPSCR at entry");
  ndtoi64++;
  return (i64) d;
}

/* Run CALL with a canary area above the callee and check that the stack
   pointer, the canaries and FPSCR survive.  */
#define CANARY 0x5a5aa5a5
#define GUARDED(NAME, CALL)						\
  do									\
    {									\
      volatile int canary[8];						\
      for (int i_ = 0; i_ < 8; i_++)					\
	canary[i_] = CANARY + i_;					\
      unsigned sp_ = get_sp ();						\
      CALL;								\
      check (NAME ": stack pointer", get_sp (), sp_);			\
      check (NAME ": FPSCR after return", get_fpscr () & FPSCR_PR_SZ, 0); \
      for (int i_ = 0; i_ < 8; i_++)					\
	check (NAME ": canary", canary[i_], CANARY + i_);		\
    }									\
  while (0)

/* Check the arguments that the last GCC callee received.  */
static void
check_rec (const char *name, int n, const double *want)
{
  check (name, nrec, n);
  for (int i = 0; i < n && i < 12; i++)
    {
      char what[64];
      int k = 0;
      for (const char *s = name; *s && k < 50; s++)
	what[k++] = *s;
      what[k++] = ' ';
      what[k++] = 'a';
      what[k++] = 'r';
      what[k++] = 'g';
      what[k++] = ' ';
      if (i >= 9)
	what[k++] = '0' + (i + 1) / 10;
      what[k++] = '0' + (i + 1) % 10;
      what[k] = 0;
      check_d (what, rec[i], want[i]);
    }
}

#define REC(name, ...)							\
  do									\
    {									\
      static const double w_[] = { __VA_ARGS__ };			\
      check_rec (name, sizeof w_ / sizeof w_[0], w_);			\
    }									\
  while (0)

/* Keep the compiler from folding the test inputs.  */
static volatile float vf[11] = { 0, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f,
				 7.5f, 8.5f, 9.5f, 10.5f };
static volatile double vdd[6] = { 0, 1.25, 2.25, 3.25, 4.25, 5.25 };
static volatile int vi[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

static void
test_msvc_calls_gcc (void)
{
  float f;
  double d;
  int r;

  GUARDED ("call_f", f = call_f ());
  check_f ("call_f", f, 1.5f * 4 + 1);
  REC ("call_f: ext_f", 1.5);

  GUARDED ("call_ff", f = call_ff ());
  check_f ("call_ff", f, 1.5f - 2.5f);
  REC ("call_ff: ext_ff", 1.5, 2.5);

  GUARDED ("call_if", f = call_if ());
  check_f ("call_if", f, 9.5f);
  REC ("call_if: ext_if", 7, 2.5);

  GUARDED ("call_fi", r = call_fi ());
  check ("call_fi", r, 3 + 700);
  REC ("call_fi: ext_fi", 1.5, 7);

  GUARDED ("call_id", d = call_id ());
  check_d ("call_id", d, 7 * 2.25);
  REC ("call_id: ext_id", 7, 2.25);

  GUARDED ("call_fd", d = call_fd ());
  check_d ("call_fd", d, 1.5 - 2.25);
  REC ("call_fd: ext_fd", 1.5, 2.25);

  GUARDED ("call_ddd", d = call_ddd ());
  check_d ("call_ddd", d, 1.25 * 2.25 - 3.25);
  REC ("call_ddd: ext_ddd", 1.25, 2.25, 3.25);

  GUARDED ("call_f8", f = call_f8 ());
  check_f ("call_f8", f, 1.5f + 8.5f);
  REC ("call_f8: ext_f8", 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5);

  GUARDED ("call_f10", f = call_f10 ());
  check_f ("call_f10", f, 9.5f - 10.5f);
  REC ("call_f10: ext_f10", 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5,
       10.5);

  GUARDED ("call_d5", d = call_d5 ());
  check_d ("call_d5", d, 5.25 - 1.25);
  REC ("call_d5: ext_d5", 1.25, 2.25, 3.25, 4.25, 5.25);

  GUARDED ("call_mix", d = call_mix ());
  check_d ("call_mix", d, 9000 + 2.25);
  REC ("call_mix: ext_mix", 7, 1.5, 8, 2.25, 2.5, 9);

  GUARDED ("call_iiiif", f = call_iiiif ());
  check_f ("call_iiiif", f, 5.0f);
  REC ("call_iiiif: ext_iiiif", 1, 2, 3, 4, 2.5);

  GUARDED ("call_ld", d = call_ld ());
  check_d ("call_ld", d, 9.0);
  REC ("call_ld: ext_ld", 14, 0, 2.25);

  GUARDED ("call_lf", f = call_lf ());
  check_f ("call_lf", f, 10.0f);
  REC ("call_lf: ext_lf", 14, 0, 2.5);

  v_doubles = 0;
  rec_str = 0;
  GUARDED ("call_v", r = call_v ());
  check ("call_v", r, 42);
  check ("call_v: format", rec_str ? (rec_str[0] << 8 | rec_str[1]) : -1,
	 'x' << 8);
  REC ("call_v: extv", 7, 1.5, 8, 2.25, 9);

  v_doubles = 1;
  GUARDED ("call_v2", r = call_v2 ());
  check ("call_v2", r, 42);
  REC ("call_v2: extv", 1.25, 2.25, 3.25);

  GUARDED ("call_F2", f = call_F2 ());
  check_f ("call_F2", f, 6.0f);
  REC ("call_F2: ext_F2", 1.5, 2.5);

  GUARDED ("call_F4", f = call_F4 ());
  check_f ("call_F4", f, 6.0f);
  REC ("call_F4: ext_F4", 1.5, 2.5, 3.5, 4.5);

  GUARDED ("call_D1", d = call_D1 ());
  check_d ("call_D1", d, 9.0);
  REC ("call_D1: ext_D1", 2.25);

  GUARDED ("call_FI", r = call_FI ());
  check ("call_FI", r, 3 + 700);
  REC ("call_FI: ext_FI", 1.5, 7);

  GUARDED ("call_ptr", f = call_ptr ());
  check_f ("call_ptr", f, 6.0f);
  REC ("call_ptr: ext_pf", 1.5, 2.25);
}

static void
test_gcc_calls_msvc (void)
{
  float f;
  double d;
  int r;

  GUARDED ("take_f", f = take_f (vf[1]));
  check_f ("take_f", f, 3.0f);

  GUARDED ("take_ff", f = take_ff (vf[5], vf[1]));
  check_f ("take_ff", f, 4.0f);

  GUARDED ("take_if", f = take_if (vi[7], vf[2]));
  check_f ("take_if", f, 9.5f);

  GUARDED ("take_fi", r = take_fi (vf[3], vi[9]));
  check ("take_fi", r, 12);

  GUARDED ("take_id", d = take_id (vi[3], vdd[1]));
  check_d ("take_id", d, 3.75);

  GUARDED ("take_fd", d = take_fd (vf[1], vdd[2]));
  check_d ("take_fd", d, -0.75);

  GUARDED ("take_ddd", d = take_ddd (vdd[1], vdd[2], vdd[3]));
  check_d ("take_ddd", d, 1.25 * 2.25 - 3.25);

  /* Signs alternate: a register pair swapped or shifted by one changes
     the result.  */
  GUARDED ("take_f8", f = take_f8 (vf[1], vf[2] * 2, vf[3] * 4, vf[4] * 8,
				   vf[5] * 16, vf[6] * 32, vf[7] * 64,
				   vf[8] * 128));
  check_f ("take_f8", f, 1.5f - 5 + 14 - 36 + 88 - 208 + 480 - 1088);

  GUARDED ("take_f10", f = take_f10 (vf[1], vf[2] * 2, vf[3] * 4, vf[4] * 8,
				     vf[5] * 16, vf[6] * 32, vf[7] * 64,
				     vf[8] * 128, vf[9] * 256,
				     vf[10] * 512));
  check_f ("take_f10", f,
	   1.5f - 5 + 14 - 36 + 88 - 208 + 480 - 1088 + 2432 - 5376);

  GUARDED ("take_d5", d = take_d5 (vdd[1], vdd[2] * 2, vdd[3] * 4,
				   vdd[4] * 8, vdd[5] * 16));
  check_d ("take_d5", d, 1.25 - 4.5 + 13 - 34 + 84);

  GUARDED ("take_mix", d = take_mix (vi[1], vf[1] * 2, vi[4] * 4, vdd[1] * 8,
				     vf[1] * 32, vi[2] * 64));
  check_d ("take_mix", d, 1 + 3 + 16 + 10 + 48 + 128);

  GUARDED ("take_iiiif", f = take_iiiif (vi[1], vi[2], vi[3], vi[4], vf[1]));
  check_f ("take_iiiif", f, 11.5f);

  ni64tod = 0;
  GUARDED ("take_ld", d = take_ld ((i64) vi[7] << 33, vdd[1]));
  check_d ("take_ld", d, 7.0 * 8589934592.0 + 1.25);
  check ("take_ld: __i64tod calls", ni64tod, 1);

  ni64tos = 0;
  GUARDED ("take_lf", f = take_lf ((i64) vi[7] << 33, vf[1]));
  check_f ("take_lf", f, 7.0f * 8589934592.0f + 1.5f);
  check ("take_lf: __i64tos calls", ni64tos, 1);

  GUARDED ("take_v", d = take_v ("x", vi[3], vdd[1], vdd[2]));
  check_d ("take_v", d, 3 + 1.25 + 2.25);

  {
    F2 a = { vf[1], vf[2] }, b;
    GUARDED ("take_F2", b = take_F2 (a));
    check_f ("take_F2.x", b.x, 2.5f);
    check_f ("take_F2.y", b.y, 2.5f);
  }
  {
    F4 a = { vf[1], vf[2], vf[3], vf[4] }, b;
    GUARDED ("take_F4", b = take_F4 (a));
    check_f ("take_F4.x", b.x, 4.5f);
    check_f ("take_F4.y", b.y, 3.5f);
    check_f ("take_F4.z", b.z, 2.5f);
    check_f ("take_F4.w", b.w, 1.5f);
  }
  {
    D1 a = { vdd[3] }, b;
    GUARDED ("take_D1", b = take_D1 (a));
    check_d ("take_D1", b.d, 6.5);
  }
  {
    FI a = { vf[3], vi[9] }, b;
    GUARDED ("take_FI", b = take_FI (a));
    check_f ("take_FI.f", b.f, 9.0f);
    check ("take_FI.i", b.i, 3);
  }

  /* keep_d and keep_f keep values in fr12..fr15 across ext_void, which
     clobbers all FPU registers and must preserve fr12..fr15.  Dividing by
     0.5 keeps every value exact.  */
  {
    double a = vdd[3], b = vdd[1] - 0.75;
    nvoid = 0;
    GUARDED ("keep_d", d = keep_d (a, b));
    check_d ("keep_d", d, a * b + (a + b) * (a - b) - a / b);
    check ("keep_d: ext_void calls", nvoid, 1);
  }
  {
    float a = vf[3], b = vf[1] - 1.0f;
    float v0 = a + b, v1 = a - b, v2 = a * b, v3 = a / b;
    float v4 = v0 * v1, v5 = v1 * v2, v6 = v2 * v3, v7 = v3 * v0;
    float v8 = v4 + v5, v9 = v6 + v7, v10 = v4 - v6, v11 = v5 - v7;
    GUARDED ("keep_f", f = keep_f (a, b));
    check_f ("keep_f", f, v0 + v1 * v2 - v3 + v4 * v5 - v6 + v7 * v8 - v9
	     + v10 * v11);
  }
  GUARDED ("mixed_prec", f = mixed_prec (vf[1], vdd[2]));
  check_f ("mixed_prec", f, (float) (1.5 * 2.25) + 1.5f * 1.5f);

  /* Conversions and comparisons.  */
  GUARDED ("d2i", r = d2i (-vdd[2]));
  check ("d2i", r, -2);
  {
    unsigned u;
    GUARDED ("d2u", u = d2u (vdd[1] * 3221225472.0));
    check ("d2u", u, 4026531840u);
  }
  {
    i64 l;
    ndtoi64 = 0;
    GUARDED ("d2l", l = d2l (-vdd[5] * 4294967296.0));
    check ("d2l", l, -(i64) 22548578304LL);
    check ("d2l: __dtoi64 calls", ndtoi64, 1);
  }
  GUARDED ("i2d", d = i2d (-vi[5]));
  check_d ("i2d", d, -5.0);
  GUARDED ("u2d", d = u2d (4026531840u + vi[1]));
  check_d ("u2d", d, 4026531841.0);
  ni64tod = 0;
  GUARDED ("l2d", d = l2d (-((i64) vi[3] << 40)));
  check_d ("l2d", d, -3298534883328.0);
  check ("l2d: __i64tod calls", ni64tod, 1);
  GUARDED ("d2f", f = d2f (vdd[4]));
  check_f ("d2f", f, 4.25f);
  GUARDED ("f2d", d = f2d (vf[9]));
  check_d ("f2d", d, 9.5);
  GUARDED ("dlt", r = dlt (vdd[1], vdd[2]));
  check ("dlt", r, 1);
  GUARDED ("dlt 2", r = dlt (vdd[2], vdd[1]));
  check ("dlt 2", r, 0);
  GUARDED ("fge", r = fge (vf[2], vf[2]));
  check ("fge", r, 1);
  GUARDED ("fge 2", r = fge (vf[1], vf[2]));
  check ("fge 2", r, 0);
  GUARDED ("dne", r = dne (vdd[3], vdd[3]));
  check ("dne", r, 0);
  GUARDED ("dne 2", r = dne (vdd[3], vdd[4]));
  check ("dne 2", r, 1);
}

/* rt.s calls test_main.  qemu-sh4 starts the process with FPSCR.PR = 1
   (the Linux default); Windows CE starts threads with PR = 0, which
   -m4-single code assumes like /Qsh4 code, so clear PR and SZ before
   any C code runs.  */
int fpu_main (void);
__asm__ ("\t.text\n"
	 "\t.align 2\n"
	 "\t.global _test_main\n"
	 "_test_main:\n"
	 "\tsts fpscr,r1\n"
	 "\tmov.l 1f,r2\n"
	 "\tand r2,r1\n"
	 "\tlds r1,fpscr\n"
	 "\tmov.l 2f,r1\n"
	 "\tjmp @r1\n"
	 "\tnop\n"
	 "\t.align 2\n"
	 "1:\t.long 0xffe7ffff\n"
	 "2:\t.long _fpu_main\n");

int
fpu_main (void)
{
  check_fpscr ("FPSCR at start");
  test_msvc_calls_gcc ();
  test_gcc_calls_msvc ();
  if (failures)
    {
      put ("FPU ABI test: FAILED\n");
      return 1;
    }
  put ("FPU ABI test: all checks passed\n");
  return 0;
}
