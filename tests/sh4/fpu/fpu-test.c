/* FPU thunk test for sh4-mingw32ce (run-fpu-test.sh).

   mingwrt's libcoredll.a replaces the jump stubs of coredll's
   floating-point functions by thunks (coredll_fpu.S) that call them in
   the Microsoft /Qsh4 convention.  This test takes the thunks out of the
   installed libcoredll.a and points their import pointers at the
   Microsoft compiler's own /Qsh4 code (calldbl, fl, from
   docs/sh4/msvc-sh4-abi-*.txt) or at fpu_callee (fpu-rt.s), then runs
   under qemu-sh4.  Built with -m4-nofpu like any sh4-mingw32ce code.  */

typedef unsigned long long u64;
typedef unsigned int u32;

/* The thunks, by kind (see coredll_fpu.S), under the name they replace.  */
extern double thunk_D_I (int) __asm__ ("_atof");
extern double thunk_D_DD (double, double) __asm__ ("_fmod");
extern char *thunk_I_DIII (double, int, int *, int *) __asm__ ("__ecvt");
extern double thunk_D_D (double) __asm__ ("_sqrt");
extern float thunk_F_F (float) __asm__ ("_sqrtf");
extern float thunk_F_ABS (float) __asm__ ("_fabsf");

extern int sys_write (int fd, const char *buf, int len);
extern u32 get_fpscr (void);
extern void set_fpscr (u32);

/* Filled by fpu_callee: fr4, fr5, fr6, fr7, r4, r5, r6, r7, stack, FPSCR.  */
u32 rec[10];

#define PR (1u << 19)
#define SZ (1u << 20)

static int failures, checks;

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

static double
dbl (u64 bits)
{
  union { u64 u; double d; } x = { bits };
  return x.d;
}

static u64
bits (double d)
{
  union { double d; u64 u; } x = { d };
  return x.u;
}

static float
flt (u32 b)
{
  union { u32 u; float f; } x = { b };
  return x.f;
}

static u32
fbits (float f)
{
  union { float f; u32 u; } x = { f };
  return x.u;
}

/* Calls with FPSCR.PR set (the thunk must clear it for the callee and
   restore it).  */
static u32 fpscr_in;

static void
pr_on (void)
{
  fpscr_in = get_fpscr () | PR;
  set_fpscr (fpscr_in);
}

static void
fpscr_after (const char *what)
{
  check (what, get_fpscr (), fpscr_in);
  set_fpscr (fpscr_in & ~PR);
}

int
test_main (void)
{
  /* D_I into MSVC calldbl (3): extdbl gets 3.0 in dr4 and 3 in r6 (the
     double takes the r4:r5 slots); calldbl returns 4.0 in dr0.  */
  for (int i = 0; i < 10; i++)
    rec[i] = 0;
  pr_on ();
  check ("D_I calldbl", bits (thunk_D_I (3)), 0x4010000000000000ull);
  fpscr_after ("D_I FPSCR restored");
  check ("calldbl: extdbl fr4 (high word)", rec[0], 0x40080000);
  check ("calldbl: extdbl fr5 (low word)", rec[1], 0);
  check ("calldbl: extdbl int in r6", rec[6], 3);

  /* D_DD into MSVC fl (x, y): fl reads its float y from fr6, which holds
     the high word of the second double: 8.0 = 0x40200000_00000000, and
     0x40200000 is 2.5f.  fl (3.0, 2.5f) = 8.5.  The high word of the
     second double comes from the caller's stack.  */
  pr_on ();
  check ("D_DD fl", bits (thunk_D_DD (dbl (0x4008000000000000ull),
				      dbl (0x4020000000000000ull))),
	 0x4021000000000000ull);
  fpscr_after ("D_DD FPSCR restored");

  /* I_DIII: double in dr4, ints in r6, r7, fourth argument on the stack
     above a fresh 16-byte home area; FPSCR.PR = SZ = 0.  */
  int a, b;
  pr_on ();
  char *p = thunk_I_DIII (dbl (0x123456789abcdef0ull), 17, &a, &b);
  fpscr_after ("I_DIII FPSCR restored");
  check ("I_DIII result", (u32) p, 1234);
  check ("I_DIII fr4", rec[0], 0x12345678);
  check ("I_DIII fr5", rec[1], 0x9abcdef0);
  check ("I_DIII r6", rec[6], 17);
  check ("I_DIII r7", rec[7], (u32) &a);
  check ("I_DIII stack", rec[8], (u32) &b);
  check ("I_DIII callee FPSCR.PR/SZ", rec[9] & (PR | SZ), 0);

  /* D_D: the callee returns its argument.  */
  pr_on ();
  check ("D_D", bits (thunk_D_D (dbl (0xc0fedcba87654321ull))),
	 0xc0fedcba87654321ull);
  fpscr_after ("D_D FPSCR restored");
  check ("D_D fr4", rec[0], 0xc0fedcba);
  check ("D_D fr5", rec[1], 0x87654321);
  check ("D_D callee FPSCR.PR/SZ", rec[9] & (PR | SZ), 0);

  /* F_F: float -> double in dr4, double result -> float.  */
  pr_on ();
  check ("F_F", fbits (thunk_F_F (flt (0x3fc00000))), 0x3fc00000);
  fpscr_after ("F_F FPSCR restored");
  check ("F_F fr4 (1.5 as double)", rec[0], 0x3ff80000);
  check ("F_F fr5", rec[1], 0);
  check ("F_F callee FPSCR.PR/SZ", rec[9] & (PR | SZ), 0);

  check ("F_ABS", fbits (thunk_F_ABS (flt (0xc0000000))), 0x40000000);

  put (failures ? "FAIL" : "PASS");
  put (": FPU thunks, ");
  put_hex (checks);
  put (" checks\n");
  return failures;
}
