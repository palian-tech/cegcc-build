/* ABI points not covered by msvc-sh4-abi-reference.txt.  The Microsoft
   SH-4 compiler's output for this file is
   docs/sh4/msvc-sh4-abi-open-questions.txt; run-abi-test.sh uses it.  */

typedef struct { char a, b, c; } S3;
typedef struct { int a; short b; } S6;	/* 8 bytes with padding */
typedef struct { short a, b, c; } S6p;	/* 6 bytes */
typedef struct { char a[5]; } S5;

extern int ext_r7 (int a, int b, int c, __int64 d, int e);
extern int extv (const char *f, ...);
extern S3 extS3 (int x);
extern S5 extS5 (int x);
extern S6p extS6p (int x);
extern char extc (char c, short s);
extern int extmul (int a, int b);
extern double extdbl (double d, int i);

/* 64-bit argument starting in r7: split between r7 and @(16,r15)?  */
int call_r7 (int x) { return ext_r7 (1, 2, 3, (__int64) x << 33, 4); }
int callv_r7 (int x) { return extv ("a", 1, 2, (__int64) x << 33, 4); }
int take_r7 (int a, int b, int c, __int64 d, int e) { return (int) (d >> 32) + e; }

/* Small struct results of odd sizes.  */
S3 retS3 (int x) { S3 s; s.a = (char) x; s.b = 1; s.c = 2; return s; }
S5 retS5 (int x) { S5 s; s.a[0] = (char) x; s.a[4] = 1; return s; }
S6p retS6p (int x) { S6p s; s.a = (short) x; s.b = 1; s.c = 2; return s; }
int useS3 (int x) { return extS3 (x).c; }
int useS6p (int x) { return extS6p (x).c; }

/* Narrow arguments and results: who extends?  */
int callc (int x) { return extc ((char) x, (short) x) + 1; }
char retc (int x) { return (char) (x + 1); }
int takec (char c, short s) { return c + s; }

/* MACH/MACL across a call: does the callee preserve them?  */
int keepmac (int a, int b) { int p = a * b; return extmul (a, b) + p * a; }

/* double without /Qsh4 (SH-3 style, if available) and with /Qsh4.  */
double calldbl (int x) { return extdbl ((double) x, x) + 1.0; }

/* Signed division and remainder helpers.  */
int divs (int a, int b) { return a / b + a % b; }
