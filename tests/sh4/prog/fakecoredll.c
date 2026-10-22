/* Stand-ins for the coredll functions that crt3 and hello.c use, so that
   hello runs under qemu-sh4 (see run-hello.sh): the real startup code
   (WinMainCRTStartup in crt3.o), WinMain and GCC's calls to the division
   helpers.  Compiled by sh4-mingw32ce-gcc, so the calls between hello and
   these functions follow the Microsoft convention.

   MessageBoxW prints its caption and text, TerminateProcess exits with
   the given code.  The division helpers count their calls.  Nothing here
   may divide: GCC would call the helpers themselves.  */

typedef unsigned short wchar;
typedef unsigned int size_t;

extern int sys_write (int fd, const void *buf, int n);	/* rt.s */
extern void WinMainCRTStartup (void *, void *, wchar *, int);

static void
out (const char *s)
{
  int n = 0;
  while (s[n])
    n++;
  sys_write (1, s, n);
}

static void
outw (const wchar *s)
{
  char c;
  for (; *s; s++)
    {
      c = *s < 0x80 ? (char) *s : '?';
      sys_write (1, &c, 1);
    }
}

static void __attribute__ ((noreturn))
sys_exit (int code)
{
  register int r3 __asm__ ("r3") = 1;	/* __NR_exit */
  register int r4 __asm__ ("r4") = code;
  __asm__ volatile ("trapa #0x17" : : "r" (r3), "r" (r4));
  for (;;)
    ;
}

static unsigned
udivmod (unsigned a, unsigned b, unsigned *rem)
{
  unsigned q = 0, bit = 1;
  if (b == 0)
    {
      out ("fakecoredll: division by zero\n");
      sys_exit (99);
    }
  while (b < a && !(b & 0x80000000u))
    b <<= 1, bit <<= 1;
  for (; bit; b >>= 1, bit >>= 1)
    if (a >= b)
      a -= b, q |= bit;
  *rem = a;
  return q;
}

int ndivls, ndivlu, nmodls, nmodlu;

unsigned
_divlu (unsigned a, unsigned b)
{
  unsigned r;
  ndivlu++;
  return udivmod (a, b, &r);
}

unsigned
_modlu (unsigned a, unsigned b)
{
  unsigned r;
  nmodlu++;
  udivmod (a, b, &r);
  return r;
}

int
_divls (int a, int b)
{
  unsigned r, q;
  ndivls++;
  q = udivmod (a < 0 ? -(unsigned) a : a, b < 0 ? -(unsigned) b : b, &r);
  return (a < 0) != (b < 0) ? -(int) q : (int) q;
}

int
_modls (int a, int b)
{
  unsigned r;
  nmodls++;
  udivmod (a < 0 ? -(unsigned) a : a, b < 0 ? -(unsigned) b : b, &r);
  return a < 0 ? -(int) r : (int) r;
}

/* wsprintfW with the conversions hello.c uses: %s (wide), %d, %u, %x
   and %lx, with a width and the 0 flag.  */
static wchar *
num (wchar *p, unsigned v, unsigned base, int neg, int width, int zero)
{
  wchar tmp[12];
  int n = 0, len;
  unsigned d;
  do
    {
      v = udivmod (v, base, &d);
      tmp[n++] = "0123456789abcdef"[d];
    }
  while (v);
  len = n + neg;
  if (neg && zero)
    *p++ = '-';
  for (; len < width; len++)
    *p++ = zero ? '0' : ' ';
  if (neg && !zero)
    *p++ = '-';
  while (n)
    *p++ = tmp[--n];
  return p;
}

int
wsprintfW (wchar *buf, const wchar *fmt, ...)
{
  __builtin_va_list ap;
  wchar *p = buf;
  __builtin_va_start (ap, fmt);
  for (; *fmt; fmt++)
    {
      int width = 0, zero = 0;
      if (*fmt != '%')
	{
	  *p++ = *fmt;
	  continue;
	}
      fmt++;
      if (*fmt == '0')
	zero = 1, fmt++;
      while (*fmt >= '0' && *fmt <= '9')
	width = width * 10 + (*fmt++ - '0');
      if (*fmt == 'l')
	fmt++;
      switch (*fmt)
	{
	case 's':
	  {
	    const wchar *s = __builtin_va_arg (ap, const wchar *);
	    while (*s)
	      *p++ = *s++;
	  }
	  break;
	case 'd':
	  {
	    int v = __builtin_va_arg (ap, int);
	    p = num (p, v < 0 ? -(unsigned) v : v, 10, v < 0, width, zero);
	  }
	  break;
	case 'u':
	  p = num (p, __builtin_va_arg (ap, unsigned), 10, 0, width, zero);
	  break;
	case 'x':
	  p = num (p, __builtin_va_arg (ap, unsigned), 16, 0, width, zero);
	  break;
	default:
	  *p++ = '%';
	  *p++ = *fmt;
	}
    }
  __builtin_va_end (ap);
  *p = 0;
  return p - buf;
}

int
MessageBoxW (void *hwnd, const wchar *text, const wchar *caption,
	     unsigned type)
{
  out ("MessageBoxW [");
  outw (caption);
  out ("]\n");
  outw (text);
  out ("\n");
  return 1;
}

int
TerminateProcess (void *process, unsigned code)
{
  static char line[] = "exit 0\n";
  line[5] = '0' + (code & 7);
  out ("division helper calls:");
  out (ndivls ? " _divls" : "");
  out (ndivlu ? " _divlu" : "");
  out (nmodls ? " _modls" : "");
  out (nmodlu ? " _modlu" : "");
  out ("\n");
  out (line);
  sys_exit (code);
}

void
_fpreset (void)
{
}

/* A bump allocator for the startup code (atexit table).  */
static char heap[16384] __attribute__ ((aligned (4)));
static size_t heap_used;

void *
malloc (size_t n)
{
  void *p = heap + heap_used;
  n = (n + 3) & ~3u;
  if (heap_used + n > sizeof heap)
    return 0;
  heap_used += n;
  return p;
}

void *
memcpy (void *d, const void *s, size_t n)
{
  char *dp = d;
  const char *sp = s;
  while (n--)
    *dp++ = *sp++;
  return d;
}

void *
realloc (void *p, size_t n)
{
  void *q = malloc (n);
  if (q && p)
    memcpy (q, p, n);	/* The old block is never larger.  */
  return q;
}

void
free (void *p)
{
}

int
fflush (void *f)
{
  return 0;
}

int
_fcloseall (void)
{
  return 0;
}

/* Only reached on error paths of the runtime.  */
static void __attribute__ ((noreturn))
unexpected (const char *what)
{
  out ("fakecoredll: unexpected call to ");
  out (what);
  out ("\n");
  sys_exit (98);
}

void *_getstdfilex (int n) { unexpected ("_getstdfilex"); }
size_t fwrite (const void *p, size_t s, size_t n, void *f) { unexpected ("fwrite"); }
int vfprintf (void *f, const char *fmt, __builtin_va_list ap) { unexpected ("vfprintf"); }
int VirtualProtect (void *a, size_t n, unsigned p, unsigned *o) { unexpected ("VirtualProtect"); }
size_t VirtualQuery (const void *a, void *b, size_t n) { unexpected ("VirtualQuery"); }

/* Called by rt.s: the arguments the CE loader passes to the entry point.  */
int
test_main (void)
{
  static wchar cmd[] = { 0 };
  WinMainCRTStartup ((void *) 0x10000, 0, cmd, 1);
  out ("WinMainCRTStartup returned\n");
  return 97;
}
