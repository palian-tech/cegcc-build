/* Runtime ABI test for sh4-mingw32ce.

   The functions of tests/sh4/abi/ref/msvc-sh4-abi-reference.txt and
   tests/sh4/abi/ref/msvc-sh4-abi-open-questions.txt are re-assembled from the
   Microsoft compiler's output (msvc2gas.py) and linked with this file,
   compiled by sh4-mingw32ce-gcc.  Calls go both ways:

   - GCC -> MSVC: this file calls retS8, take64, ... and checks results.
   - MSVC -> GCC: the MSVC callers call2, callv, divu, ... call ext2, extv,
     __divlu, ... defined here, which record what they received.

   Every call is made with a canary area just above the callee's frame
   and the stack pointer is compared before and after, so a missing
   16-byte home area or a wrong stack adjustment shows up as a failure.  */

typedef struct { int a; int b; } S8;
typedef struct { int a; } S4;
typedef struct { int a, b, c, d, e; } S20;
typedef long long i64;
typedef unsigned long long u64;

/* Compiled by the Microsoft SH-4 compiler.  */
extern int call2 (int x);
extern int call5 (int x);
extern i64 ret64 (int x);
extern int use64 (int x);
extern S8 retS8 (int x);
extern S4 retS4 (int x);
extern S20 retS20 (int x);
extern int useS8 (int x);
extern int takeS8 (S8 s, int y);
extern int take64 (int a, i64 b, int c);
extern int callv (int x);
extern unsigned divu (unsigned a, unsigned b);

/* From msvc-sh4-abi-open-questions.txt, /Qsh4 /O2.  */
typedef struct { char a, b, c; } S3;
typedef struct { short a, b, c; } S6p;
typedef struct { char a[5]; } S5;
extern int call_r7 (int x);
extern int callv_r7 (int x);
extern int take_r7 (int a, int b, int c, i64 d, int e);
extern S3 retS3 (int x);
extern S5 retS5 (int x);
extern S6p retS6p (int x);
extern int useS3 (int x);
extern int useS6p (int x);
extern int callc (int x);
extern char retc (int x);
extern int takec (char c, short s);
extern int keepmac (int a, int b);
extern int divs (int a, int b);
/* The same file compiled without /Qsh4 (software floating point).  */
extern double calldbl_nofpu (int x);

extern int sys_write (int fd, const char *buf, int len);
extern int home_clobber (int a, int b, int c, volatile void *local);

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

static inline unsigned
get_sp (void)
{
  unsigned sp;
  __asm__ volatile ("mov r15,%0" : "=r" (sp));
  return sp;
}

/* What the GCC callees received from the MSVC callers.  */
static int rec[8];
static int nrec;
static const char *rec_str;

static void
record (int v)
{
  if (nrec < 8)
    rec[nrec] = v;
  nrec++;
}

int
ext2 (int a, int b)
{
  nrec = 0;
  record (a);
  record (b);
  return a * 10 + b;
}

int
ext5 (int a, int b, int c, int d, int e)
{
  nrec = 0;
  record (a);
  record (b);
  record (c);
  record (d);
  record (e);
  return a + b * 2 + c * 3 + d * 4 + e * 5;
}

i64
ext64 (int a)
{
  nrec = 0;
  record (a);
  return ((i64) a << 33) | 0x12345;
}

S8
extS8 (int a)
{
  S8 s;
  nrec = 0;
  record (a);
  s.a = a;
  s.b = a * 3;
  return s;
}

int
extv (const char *f, ...)
{
  __builtin_va_list ap;
  int sum = 0;
  nrec = 0;
  rec_str = f;
  __builtin_va_start (ap, f);
  for (int i = 0; i < 5; i++)
    {
      int v = __builtin_va_arg (ap, int);
      record (v);
      sum = sum * 10 + v;
    }
  __builtin_va_end (ap);
  return sum;
}

int
ext_r7 (int a, int b, int c, i64 d, int e)
{
  nrec = 0;
  record (a);
  record (b);
  record (c);
  record ((int) d);
  record ((int) (d >> 32));
  record (e);
  return 0;
}

S3
extS3 (int x)
{
  S3 s;
  nrec = 0;
  record (x);
  s.a = (char) x;
  s.b = 11;
  s.c = 22;
  return s;
}

S6p
extS6p (int x)
{
  S6p s;
  nrec = 0;
  record (x);
  s.a = (short) x;
  s.b = 11;
  s.c = 33;
  return s;
}

char
extc (char c, short s)
{
  nrec = 0;
  record (c);
  record (s);
  return (char) (c + 1);
}

/* Uses mul.l, i.e. clobbers MACL.  */
int
extmul (int a, int b)
{
  volatile int va = a, vb = b;
  return va * vb + 1;
}

double
extdbl (double d, int i)
{
  nrec = 0;
  record ((int) d);
  record (i);
  return d * 2 + i;
}

/* The coredll floating-point helpers used by code compiled without
   /Qsh4, imported through __imp_ pointers: results and operands by
   address.  */
static void
itod_impl (double *r, int x)
{
  *r = x;
}

static void
addd_impl (double *r, const double *a, const double *b)
{
  *r = *a + *b;
}

void (*_imp___itod) (double *, int) = itod_impl;
void (*_imp___addd) (double *, const double *, const double *) = addd_impl;

/* coredll's unsigned division helpers, as called by divu.  Written
   without '/' and '%', which would call these functions.  */
static unsigned
udivmod (unsigned a, unsigned b, unsigned *rem)
{
  unsigned q = 0, r = 0;
  for (int i = 31; i >= 0; i--)
    {
      r = (r << 1) | ((a >> i) & 1);
      if (r >= b)
	{
	  r -= b;
	  q |= 1u << i;
	}
    }
  *rem = r;
  return q;
}

static int ndiv, nmod;

unsigned
_divlu (unsigned a, unsigned b)
{
  unsigned r;
  ndiv++;
  return udivmod (a, b, &r);
}

unsigned
_modlu (unsigned a, unsigned b)
{
  unsigned r;
  nmod++;
  udivmod (a, b, &r);
  return r;
}

static int ndivs, nmods;

int
_divls (int a, int b)
{
  unsigned r, q;
  ndivs++;
  q = udivmod (a < 0 ? -(unsigned) a : a, b < 0 ? -(unsigned) b : b, &r);
  return (a < 0) != (b < 0) ? -(int) q : (int) q;
}

int
_modls (int a, int b)
{
  unsigned r;
  nmods++;
  udivmod (a < 0 ? -(unsigned) a : a, b < 0 ? -(unsigned) b : b, &r);
  return a < 0 ? -(int) r : (int) r;
}

/* The 64-bit multiply helper called by ret64: result, a and b are passed
   by address (see ret64 in the reference).  */
void
_muli64 (i64 *r, const i64 *a, const i64 *b)
{
  *r = *a * *b;
}

/* Run CALL with a canary area above the callee and check that the stack
   pointer and the canaries survive.  */
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
      for (int i_ = 0; i_ < 8; i_++)					\
	check (NAME ": canary", canary[i_], CANARY + i_);		\
    }									\
  while (0)

/* Keep the compiler from folding the test inputs.  */
static volatile int seven = 7;

static void
test_gcc_calls_msvc (void)
{
  int x = seven;

  /* 64-bit and >= 8-byte struct results come back through the hidden
     pointer in r4; a 4-byte struct in r0.  */
  {
    i64 r;
    GUARDED ("ret64", r = ret64 (x));
    check ("ret64", r, (i64) x * 3 + 5);
    GUARDED ("ret64 neg", r = ret64 (-x));
    check ("ret64 neg", r, (i64) -x * 3 + 5);
  }
  {
    S8 s;
    GUARDED ("retS8", s = retS8 (x));
    check ("retS8.a", s.a, x);
    check ("retS8.b", s.b, x + 1);
  }
  {
    S4 s;
    GUARDED ("retS4", s = retS4 (x));
    check ("retS4.a", s.a, x);
  }
  {
    S20 s;
    GUARDED ("retS20", s = retS20 (x));
    check ("retS20.a", s.a, x);
    check ("retS20.b", s.b, 1);
    check ("retS20.c", s.c, 2);
    check ("retS20.d", s.d, 3);
    check ("retS20.e", s.e, 4);
  }
  /* An 8-byte struct in two registers.  */
  {
    S8 s;
    int r;
    s.a = x;
    s.b = 100;
    GUARDED ("takeS8", r = takeS8 (s, 1000));
    check ("takeS8", r, x + 100 + 1000);
  }
  /* A 64-bit argument in r5:r6, no even-register alignment; take64
     stores r6 into the caller's home area at @(8,r15).  */
  {
    int r;
    i64 b = ((i64) 0x7fff << 32) | 20;
    GUARDED ("take64", r = take64 (x, b, 300));
    check ("take64", r, x + 20 + 300);
  }
}

/* Every call reserves the 16-byte home area: a callee that overwrites
   all of it must not disturb the caller, and no live object of the caller
   may lie inside it.  */
static int __attribute__ ((noipa))
home_area_frame (void)
{
  volatile int local = 5;
  int r;
  GUARDED ("home area", r = home_clobber (1, 2, 3, &local));
  check ("home area: local", local, 5);
  return r;
}

static void
test_home_area (void)
{
  int x = seven;
  check ("home area", home_area_frame (), 6);
  /* The same with live values in call-saved registers.  */
  volatile int local = x;
  int a = x * 3, b = x * 5, c = x * 7;
  int r = home_clobber (a, b, c, &local);
  check ("home area: saved regs", a + b + c + r, 2 * (x * 15));
  check ("home area: local 2", local, x);
}

static void
test_msvc_calls_gcc (void)
{
  int x = seven;
  int r;

  GUARDED ("call2", r = call2 (x));
  check ("call2", r, x * 10 + 7 + 1);
  check ("call2: ext2 argc", nrec, 2);
  check ("call2: ext2 a", rec[0], x);
  check ("call2: ext2 b", rec[1], 7);

  /* The fifth argument at @(16,r15) above the home area.  */
  GUARDED ("call5", r = call5 (x));
  check ("call5", r, 1 + 2 * 2 + 3 * 3 + 4 * 4 + x * 5 + 1);
  check ("call5: ext5 e", rec[4], x);
  check ("call5: ext5 a", rec[0], 1);
  check ("call5: ext5 d", rec[3], 4);

  /* A 64-bit result through the hidden pointer.  */
  GUARDED ("use64", r = use64 (x));
  check ("use64", r, x);
  check ("use64: ext64 a", rec[0], x);
  GUARDED ("use64 neg", r = use64 (-x));
  check ("use64 neg", r, -x);

  /* An 8-byte struct result through the hidden pointer.  */
  GUARDED ("useS8", r = useS8 (x));
  check ("useS8", r, x + x * 3);
  check ("useS8: extS8 a", rec[0], x);

  /* Varargs: "a", x, 2, 3 in r4..r7, 4 and 5 at @(16) and @(20).  */
  rec_str = 0;
  GUARDED ("callv", r = callv (x));
  check ("callv", r, ((((x * 10) + 2) * 10 + 3) * 10 + 4) * 10 + 5);
  check ("callv: format", rec_str ? (rec_str[0] << 8 | rec_str[1]) : -1,
	 'a' << 8);
  check ("callv: arg 1", rec[0], x);
  check ("callv: arg 4", rec[3], 4);
  check ("callv: arg 5", rec[4], 5);

  /* Unsigned division through __modlu and __divlu.  */
  {
    unsigned u;
    ndiv = nmod = 0;
    GUARDED ("divu", u = divu (1000u + x, 9));
    check ("divu", u, (1000u + x) / 9 + (1000u + x) % 9);
    check ("divu: __divlu calls", ndiv, 1);
    check ("divu: __modlu calls", nmod, 1);
  }
}

/* A 64-bit variadic argument that starts in r7 is split between r7 and
   @(16) (callv_r7), so the argument words stay contiguous.  Here both
   sides are GCC.  */
static i64 sum64v_result;

static void __attribute__ ((noipa))
sum64v (int n, ...)
{
  __builtin_va_list ap;
  __builtin_va_start (ap, n);
  int a = __builtin_va_arg (ap, int);
  int b = __builtin_va_arg (ap, int);
  i64 c = __builtin_va_arg (ap, i64);
  int d = __builtin_va_arg (ap, int);
  __builtin_va_end (ap);
  sum64v_result = a + b + c + d + n;
}

static void
test_varargs64 (void)
{
  /* n, 10, 20 in r4..r6; big64 in r7 and @(16); 30 at @(20).  */
  i64 big64 = ((i64) seven << 40) | 0x89abcdefLL;
  sum64v (1, 10, 20, big64, 30);
  check ("varargs 64-bit in r7/stack", sum64v_result,
	 1 + 10 + 20 + big64 + 30);
}

/* Points settled by msvc-sh4-abi-open-questions.txt.  */
static void
test_open_questions (void)
{
  int x = seven;
  int r;

  /* A 64-bit argument starting in r7 is split: low word in r7, high
     word at @(16,r15); the next argument at @(20,r15).  */
  GUARDED ("call_r7", r = call_r7 (x));
  check ("call_r7: d low", rec[3], 0);
  check ("call_r7: d high", rec[4], x << 1);
  check ("call_r7: e", rec[5], 4);
  check ("call_r7: c", rec[2], 3);
  GUARDED ("callv_r7", r = callv_r7 (x));
  check ("callv_r7: arg 1", rec[0], 1);
  check ("callv_r7: d low", rec[2], 0);
  check ("callv_r7: d high", rec[3], x << 1);
  check ("callv_r7: arg 5", rec[4], 4);
  {
    i64 d = ((i64) (x + 100) << 32) | 0xdeadu;
    GUARDED ("take_r7", r = take_r7 (1, 2, 3, d, 50));
    check ("take_r7", r, x + 100 + 50);
  }

  /* Structs of up to 4 bytes come back in r0, larger ones through the
     hidden pointer, which the caller then takes from r0 (useS6p).  */
  {
    S3 s;
    GUARDED ("retS3", s = retS3 (x));
    check ("retS3.a", s.a, x);
    check ("retS3.b", s.b, 1);
    check ("retS3.c", s.c, 2);
    GUARDED ("useS3", r = useS3 (x));
    check ("useS3", r, 22);
    check ("useS3: extS3 x", rec[0], x);
  }
  {
    S5 s;
    GUARDED ("retS5", s = retS5 (x));
    check ("retS5.a[0]", s.a[0], x);
    check ("retS5.a[4]", s.a[4], 1);
  }
  {
    S6p s;
    GUARDED ("retS6p", s = retS6p (x));
    check ("retS6p.a", s.a, x);
    check ("retS6p.b", s.b, 1);
    check ("retS6p.c", s.c, 2);
    GUARDED ("useS6p", r = useS6p (x));
    check ("useS6p", r, 33);
    check ("useS6p: extS6p x", rec[0], x);
  }

  /* char and short: both sides extend, neither relies on it.  */
  {
    static volatile int m = -2;
    GUARDED ("callc", r = callc (m));
    check ("callc", r, -2 + 1 + 1);
    check ("callc: extc c", rec[0], -2);
    check ("callc: extc s", rec[1], -2);
    char c;
    GUARDED ("retc", c = retc (m));
    check ("retc", c, -1);
    GUARDED ("takec", r = takec ((char) m, (short) m));
    check ("takec", r, -4);
  }

  /* MACL is call-clobbered: keepmac multiplies again after the call.  */
  GUARDED ("keepmac", r = keepmac (x, 5));
  check ("keepmac", r, (x * 5 + 1) + x * 5 * x);

  /* Signed division through __modls and __divls.  */
  {
    int want = (-1000 - x) / 9 + (-1000 - x) % 9;
    ndivs = nmods = 0;
    GUARDED ("divs", r = divs (-1000 - x, 9));
    check ("divs", r, want);
    check ("divs: __divls calls", ndivs, 1);
    check ("divs: __modls calls", nmods, 1);
  }

  /* Software floating point: double arguments in two registers, double
     results through the hidden pointer.  */
  {
    double d;
    GUARDED ("calldbl", d = calldbl_nofpu (x));
    check ("calldbl", (i64) d, 3 * x + 1);
    check ("calldbl: extdbl d", rec[0], x);
    check ("calldbl: extdbl i", rec[1], x);
  }
}

/* GCC's own division must call the coredll helpers as well.  */
static volatile unsigned big = 0xfffffff1u, nine = 9;
static volatile int sbig = -1000003, snine = 9;

static void
test_gcc_division (void)
{
  unsigned q, m;
  ndiv = nmod = 0;
  q = big / nine;
  m = big % nine;
  check ("udiv", q, 0xfffffff1u / 9);
  check ("umod", m, 0xfffffff1u % 9);
  check ("udiv: __divlu calls", ndiv, 1);
  check ("umod: __modlu calls", nmod, 1);

  int sq, sm;
  ndivs = nmods = 0;
  sq = sbig / snine;
  sm = sbig % snine;
  check ("sdiv", sq, -1000003 / 9);
  check ("smod", sm, -1000003 % 9);
  check ("sdiv: __divls calls", ndivs, 1);
  check ("smod: __modls calls", nmods, 1);
}

int
test_main (void)
{
  test_home_area ();
  test_gcc_calls_msvc ();
  test_msvc_calls_gcc ();
  test_gcc_division ();
  test_varargs64 ();
  test_open_questions ();
  if (failures)
    {
      put ("ABI test: FAILED\n");
      return 1;
    }
  put ("ABI test: all checks passed\n");
  return 0;
}
