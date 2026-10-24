// Runtime checks for a Windows CE C++ toolchain (arm-mingw32ce, sh4-mingw32ce).
//
// One line per check:
//   PASS  <name>
//   FAIL  <name>: <reason>
//   XFAIL <name>: <reason>   expected failure of a known limitation
//   XPASS <name>             an expected failure that passed
//   SKIP  <name>: <reason>   not applicable to this build
// and a last line
//   DONE <pass>/<run> xfail=<n> skip=<n>
// where run counts PASS, FAIL and XPASS. The lines go to OutputDebugStringW
// and to cetest.txt next to the EXE. The file is written line by line, so
// after a crash the check that crashed is the one after its last line
// (the order is the order of the calls in WinMain).
//
// Build (see tests/gcc16/build.sh):
//   arm-mingw32ce-g++ -std=c++20 -O2 -static cetest.cpp -o cetest.exe
//
// The static destructor check runs this EXE again with the argument
// --child-dtor and reads the file the child writes at exit.

#include <windows.h>

#include <algorithm>
#include <any>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <deque>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#if __cplusplus > 202002L
#include <expected>
#endif
#if defined(_GLIBCXX_HAS_GTHREADS)
#include <mutex>
#include <thread>
#endif

namespace {

// ---------------------------------------------------------------- output

HANDLE g_log = INVALID_HANDLE_VALUE;
wchar_t g_dir[MAX_PATH];
int g_pass, g_fail, g_xfail, g_xpass, g_skip;

void exe_dir()
{
    DWORD n = GetModuleFileNameW(nullptr, g_dir, MAX_PATH);
    while (n > 0 && g_dir[n - 1] != L'\\')
        --n;
    g_dir[n] = 0;
}

std::wstring path_in_dir(const wchar_t *name)
{
    return std::wstring(g_dir) + name;
}

void out(const char *line)
{
    wchar_t w[512];
    int n = MultiByteToWideChar(CP_ACP, 0, line, -1, w, 510);
    if (n <= 0)
        n = 1;
    w[n - 1] = L'\n';
    w[n] = 0;
    OutputDebugStringW(w);
    if (g_log != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(g_log, line, (DWORD)strlen(line), &written, nullptr);
        WriteFile(g_log, "\r\n", 2, &written, nullptr);
        FlushFileBuffers(g_log);
    }
}

void report(const char *tag, const char *name, const char *why)
{
    char line[512];
    if (why && *why)
        snprintf(line, sizeof line, "%s %s: %s", tag, name, why);
    else
        snprintf(line, sizeof line, "%s %s", tag, name);
    out(line);
}

// A check returns an empty string on success or the reason it failed.
using Result = std::string;

#define EXPECT(cond)                                                   \
    do {                                                               \
        if (!(cond))                                                   \
            return std::string("line ") + std::to_string(__LINE__) +   \
                   ": " #cond;                                         \
    } while (0)

void run(const char *name, Result (*fn)())
{
    Result r;
    try {
        r = fn();
    } catch (const std::exception &e) {
        r = std::string("uncaught exception: ") + e.what();
    } catch (...) {
        r = "uncaught exception";
    }
    if (r.empty()) {
        ++g_pass;
        report("PASS", name, nullptr);
    } else {
        ++g_fail;
        report("FAIL", name, r.c_str());
    }
}

// A check that is expected to fail with this build.
void run_xfail(const char *name, Result (*fn)(), const char *why)
{
    Result r;
    try {
        r = fn();
    } catch (...) {
        r = "uncaught exception";
    }
    if (r.empty()) {
        ++g_xpass;
        report("XPASS", name, nullptr);
    } else {
        ++g_xfail;
        std::string s = std::string(why) + " (" + r + ")";
        report("XFAIL", name, s.c_str());
    }
}

void skip(const char *name, const char *why)
{
    ++g_skip;
    report("SKIP", name, why);
}

// ---------------------------------------------------- static ctors/dtors

int g_ctor_order[3];
int g_ctor_count;

struct Ctor {
    int id;
    explicit Ctor(int i) : id(i) { g_ctor_order[g_ctor_count++] = i; }
};
Ctor g_ctor_a(1), g_ctor_b(2), g_ctor_c(3);

Result static_ctors()
{
    EXPECT(g_ctor_count == 3);
    EXPECT(g_ctor_order[0] == 1 && g_ctor_order[1] == 2 &&
           g_ctor_order[2] == 3);
    EXPECT(g_ctor_a.id == 1 && g_ctor_c.id == 3);
    return {};
}

int &local_static_counter()
{
    static int n = [] { return 41; }();
    return ++n;
}

Result local_statics()
{
    EXPECT(local_static_counter() == 42);
    EXPECT(local_static_counter() == 43);
    return {};
}

// In the child process these append to cetest-dtor.txt at exit. The
// expected file content is "fBA": the function-local static and the
// atexit handler registered after A and B run first, then B, then A.
bool g_child;

void append_marker(const char *s)
{
    if (!g_child)
        return;
    HANDLE h = CreateFileW(path_in_dir(L"cetest-dtor.txt").c_str(),
                           GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    SetFilePointer(h, 0, nullptr, FILE_END);
    DWORD written;
    WriteFile(h, s, (DWORD)strlen(s), &written, nullptr);
    CloseHandle(h);
}

struct Dtor {
    const char *tag;
    explicit Dtor(const char *t) : tag(t) {}
    ~Dtor() { append_marker(tag); }
};
Dtor g_dtor_a("A"), g_dtor_b("B");

void touch_local_dtor()
{
    static Dtor f("f");
    (void)f;
}

Result static_dtors()
{
    std::wstring marker = path_in_dir(L"cetest-dtor.txt");
    DeleteFileW(marker.c_str());

    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    wchar_t args[] = L"--child-dtor";
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, args, nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, nullptr, &pi))
        return "CreateProcessW failed, error " +
               std::to_string(GetLastError());
    DWORD wait = WaitForSingleObject(pi.hProcess, 30000);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    EXPECT(wait == WAIT_OBJECT_0);
    EXPECT(code == 7);

    HANDLE h = CreateFileW(marker.c_str(), GENERIC_READ, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return "child wrote no cetest-dtor.txt (no static dtor ran)";
    char buf[16] = {};
    DWORD got = 0;
    ReadFile(h, buf, sizeof buf - 1, &got, nullptr);
    CloseHandle(h);
    DeleteFileW(marker.c_str());
    if (strcmp(buf, "fBA") != 0)
        return std::string("dtor order \"") + buf + "\", expected \"fBA\"";
    return {};
}

int child_dtor_main()
{
    g_child = true;
    touch_local_dtor();
    return 7;
}

// ------------------------------------------------------------ new/delete

int g_live;
struct Counted {
    int v;
    Counted() : v(5) { ++g_live; }
    ~Counted() { --g_live; }
};

struct alignas(64) Overaligned {
    char c[64];
};

Result new_delete()
{
    int *p = new int(42);
    EXPECT(*p == 42);
    delete p;

    Counted *a = new Counted[10];
    EXPECT(g_live == 10 && a[9].v == 5);
    delete[] a;
    EXPECT(g_live == 0);

    int *q = new (std::nothrow) int[16];
    EXPECT(q != nullptr);
    delete[] q;

    Overaligned *o = new Overaligned;
    EXPECT(reinterpret_cast<uintptr_t>(o) % 64 == 0);
    delete o;

    auto u = std::make_unique<Counted>();
    auto s = std::make_shared<Counted>();
    auto s2 = s;
    EXPECT(g_live == 2 && s.use_count() == 2);
    u.reset();
    s.reset();
    s2.reset();
    EXPECT(g_live == 0);
    return {};
}

// ------------------------------------------------------------ containers

Result containers()
{
    std::vector<int> v(100);
    std::iota(v.begin(), v.end(), 0);
    std::reverse(v.begin(), v.end());
    std::sort(v.begin(), v.end());
    EXPECT(v.front() == 0 && v.back() == 99);
    EXPECT(std::accumulate(v.begin(), v.end(), 0) == 4950);

    std::string s = "hello";
    s += ", world";
    s.insert(0, ">");
    EXPECT(s == ">hello, world" && s.size() == 13);
    EXPECT(s.find("world") == 8);
    std::wstring ws = L"wide";
    ws += L" string";
    EXPECT(ws.size() == 11 && ws.compare(L"wide string") == 0);
    EXPECT(std::to_string(-12345) == "-12345");

    std::map<std::string, int> m{{"b", 2}, {"a", 1}, {"c", 3}};
    EXPECT(m.begin()->first == "a" && m.at("c") == 3);

    std::unordered_map<int, std::string> um;
    for (int i = 0; i < 1000; ++i)
        um[i] = std::to_string(i);
    EXPECT(um.size() == 1000 && um[777] == "777");

    std::set<int> st{5, 1, 3};
    EXPECT(*st.begin() == 1 && st.count(3) == 1);

    std::deque<int> d;
    for (int i = 0; i < 1000; ++i)
        d.push_front(i);
    EXPECT(d.front() == 999 && d.back() == 0);

    std::list<int> l{3, 1, 2};
    l.sort();
    EXPECT(l.front() == 1 && l.back() == 3);
    return {};
}

// ------------------------------------------------------ vocabulary types

Result vocabulary()
{
    std::optional<int> o;
    EXPECT(!o);
    o = 7;
    EXPECT(o.value_or(0) == 7);

    std::variant<int, std::string> var = std::string("x");
    EXPECT(std::holds_alternative<std::string>(var));
    int visited = std::visit(
        [](auto &&x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, int>)
                return 1;
            else
                return 2;
        },
        var);
    EXPECT(visited == 2);

    std::any a = 3.5;
    EXPECT(std::any_cast<double>(a) == 3.5);
    a = std::string("any");
    EXPECT(std::any_cast<std::string &>(a) == "any");

    std::string_view sv = "string_view";
    EXPECT(sv.substr(7) == "view");

    int arr[4] = {1, 2, 3, 4};
    std::span<int> sp(arr);
    EXPECT(sp.size() == 4 && sp.last(2)[0] == 3);

    auto [x, y] = std::pair{1, 2.0};
    EXPECT(x == 1 && y == 2.0);
    return {};
}

#if __cplusplus > 202002L
std::expected<int, std::string> parse_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    return std::unexpected(std::string("not a digit"));
}

Result cxx23()
{
    EXPECT(parse_digit('7').value() == 7);
    EXPECT(!parse_digit('x') && parse_digit('x').error() == "not a digit");
    enum class E : short { a = 3 };
    EXPECT(std::to_underlying(E::a) == 3);
    std::string s = "abc";
    EXPECT(s.contains('b'));
    return {};
}
#endif

// ------------------------------------------------ lambdas, std::function

Result lambdas()
{
    int base = 10;
    auto add = [base](int v) { return base + v; };
    EXPECT(add(5) == 15);

    std::function<int(int)> f = add;
    EXPECT(f(1) == 11);
    f = [&base](int v) { return base * v; };
    base = 3;
    EXPECT(f(4) == 12);

    // A capture too big for the small-object buffer.
    std::vector<int> big(64, 1);
    std::function<int()> g = [big] {
        return std::accumulate(big.begin(), big.end(), 0);
    };
    std::function<int()> g2 = g;
    EXPECT(g2() == 64);

    auto generic = [](auto a, auto b) { return a + b; };
    EXPECT(generic(1, 2) == 3 && generic(std::string("a"), "b") == "ab");

    std::function<int(int)> fact = [&](int n) {
        return n <= 1 ? 1 : n * fact(n - 1);
    };
    EXPECT(fact(10) == 3628800);
    return {};
}

// ------------------------------------------------------------ exceptions

int g_unwound;
struct Guard {
    ~Guard() { ++g_unwound; }
};

[[gnu::noinline]] void thrower(int depth)
{
    Guard g;
    if (depth == 0)
        throw std::runtime_error("deep");
    thrower(depth - 1);
}

struct Base {
    virtual ~Base() = default;
};
struct Derived : Base {
    int v = 9;
};

struct Member {
    explicit Member(bool fail)
    {
        if (fail)
            throw 5;
        ++g_live;
    }
    ~Member() { --g_live; }
};
struct Holder {
    Member a, b;
    Holder() : a(false), b(true) {}
};

Result exceptions_local()
{
    int caught = 0;
    try {
        throw 42;
    } catch (int v) {
        caught = v;
    }
    EXPECT(caught == 42);
    return {};
}

Result exceptions_unwind()
{
    g_unwound = 0;
    std::string what;
    try {
        thrower(5);
    } catch (const std::exception &e) {
        what = e.what();
    }
    EXPECT(what == "deep");
    EXPECT(g_unwound == 6);
    return {};
}

Result exceptions_types()
{
    int got = 0;
    try {
        throw Derived();
    } catch (const Base &b) {
        got = dynamic_cast<const Derived &>(b).v;
    }
    EXPECT(got == 9);

    bool rethrown = false;
    try {
        try {
            throw std::logic_error("inner");
        } catch (...) {
            throw;
        }
    } catch (const std::logic_error &) {
        rethrown = true;
    }
    EXPECT(rethrown);

    std::exception_ptr ep;
    try {
        throw std::out_of_range("ptr");
    } catch (...) {
        ep = std::current_exception();
    }
    bool from_ptr = false;
    try {
        std::rethrow_exception(ep);
    } catch (const std::out_of_range &e) {
        from_ptr = std::string(e.what()) == "ptr";
    }
    EXPECT(from_ptr);

    g_live = 0;
    bool ctor_threw = false;
    try {
        Holder h;
    } catch (int v) {
        ctor_threw = v == 5;
    }
    EXPECT(ctor_threw && g_live == 0);
    return {};
}

Result exceptions_library()
{
    std::vector<int> v(3);
    bool caught = false;
    try {
        (void)v.at(10);
    } catch (const std::out_of_range &) {
        caught = true;
    }
    EXPECT(caught);

    caught = false;
    try {
        std::function<void()> empty;
        empty();
    } catch (const std::bad_function_call &) {
        caught = true;
    }
    EXPECT(caught);

    caught = false;
    try {
        std::variant<int, double> var = 1;
        (void)std::get<double>(var);
    } catch (const std::bad_variant_access &) {
        caught = true;
    }
    EXPECT(caught);

    caught = false;
    try {
        std::function<void()> f = [] { throw std::string("lambda"); };
        f();
    } catch (const std::string &s) {
        caught = s == "lambda";
    }
    EXPECT(caught);
    return {};
}

// ------------------------------------------------- thread_local, threads

thread_local int t_value = 1;
thread_local std::string t_string = "init";

DWORD WINAPI tls_thread(LPVOID p)
{
    int *seen = static_cast<int *>(p);
    seen[0] = t_value;
    seen[1] = t_string == "init";
    t_value = 99;
    return 0;
}

Result thread_local_main()
{
    t_value = 2;
    EXPECT(t_value == 2);
    t_string += "!";
    EXPECT(t_string == "init!");
    t_value = 1;
    t_string = "init";
    return {};
}

Result thread_local_createthread()
{
    t_value = 42;
    int seen[2] = {-1, -1};
    HANDLE h = CreateThread(nullptr, 0, tls_thread, seen, 0, nullptr);
    if (!h)
        return "CreateThread failed";
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    int mine = t_value;
    t_value = 1;
    if (seen[0] != 1)
        return "thread saw t_value=" + std::to_string(seen[0]) +
               ", expected its own copy (1)";
    EXPECT(seen[1] == 1);
    if (mine != 42)
        return "thread changed the main thread's copy";
    return {};
}

#if defined(_GLIBCXX_HAS_GTHREADS)
Result std_thread_mutex()
{
    std::mutex m;
    long sum = 0;
    std::vector<std::thread> ts;
    for (int i = 0; i < 4; ++i)
        ts.emplace_back([&] {
            for (int k = 0; k < 1000; ++k) {
                std::lock_guard<std::mutex> lock(m);
                ++sum;
            }
        });
    for (auto &t : ts)
        t.join();
    EXPECT(sum == 4000);
    return {};
}
#endif

// ---------------------------------------------------------- C library

Result c_snprintf()
{
    // Volatile inputs, so the compiler cannot fold the calls.
    volatile int vi = -17, vx = 255;
    const char *volatile vs = "str";
    volatile double vd = 3.14159;
    char buf[64];
    int n = snprintf(buf, sizeof buf, "%d|%s|%x|%c|%5.2f", (int)vi,
                     (const char *)vs, (int)vx, 'z', (double)vd);
    if (strcmp(buf, "-17|str|ff|z| 3.14") != 0)
        return std::string("got \"") + buf + "\"";
    EXPECT(n == 18);

    // C99: truncate, terminate, return the full length.
    const char *volatile long_str = "truncated";
    char trunc[5];
    n = snprintf(trunc, sizeof trunc, "%s", (const char *)long_str);
    EXPECT(n == 9);
    EXPECT(strcmp(trunc, "trun") == 0);

    volatile long long vll = -1234567890123LL;
    volatile unsigned long long vull = 18446744073709551615ULL;
    snprintf(buf, sizeof buf, "%lld|%llu", (long long)vll,
             (unsigned long long)vull);
    if (strcmp(buf, "-1234567890123|18446744073709551615") != 0)
        return std::string("%lld/%llu gave \"") + buf + "\"";
    return {};
}

// coredll's floating-point functions, called through volatile pointers so
// that the compiler neither folds nor expands them. On SH-4, coredll is
// /Qsh4 code that takes and returns double in FPU registers; mingwrt's
// thunks convert from -m4-nofpu (docs/sh4/ABI.md).
#ifdef __STRICT_ANSI__
extern "C" char *_ecvt(double, int, int *, int *); // stdlib.h: GNU modes only
#endif

Result coredll_double()
{
    double (*volatile p_sqrt)(double) = ::sqrt;
    double (*volatile p_floor)(double) = ::floor;
    double (*volatile p_pow)(double, double) = ::pow;
    double (*volatile p_fmod)(double, double) = ::fmod;
    double (*volatile p_atan2)(double, double) = ::atan2;
    double (*volatile p_ldexp)(double, int) = ::ldexp;
    double (*volatile p_frexp)(double, int *) = ::frexp;
    double (*volatile p_modf)(double, double *) = ::modf;
    double (*volatile p_strtod)(const char *, char **) = ::strtod;
    char *(*volatile p_ecvt)(double, int, int *, int *) = ::_ecvt;
    float (*volatile p_sqrtf)(float) = ::sqrtf;

    EXPECT(p_sqrt(2.25) == 1.5);
    EXPECT(p_floor(-2.5) == -3.0);
    EXPECT(p_pow(2.0, 10.0) == 1024.0);
    EXPECT(p_fmod(7.5, 2.0) == 1.5);
    double q = p_atan2(1.0, 1.0);
    EXPECT(q > 0.785398163397 && q < 0.785398163398);
    EXPECT(p_ldexp(0.75, 4) == 12.0);
    int e = 0;
    EXPECT(p_frexp(12.0, &e) == 0.75 && e == 4);
    double ip = 0;
    EXPECT(p_modf(3.25, &ip) == 0.25 && ip == 3.0);
    const char *s = "1.5e3x";
    char *end = nullptr;
    EXPECT(p_strtod(s, &end) == 1500.0 && end == s + 5);
    int dec = 0, sign = 1;
    const char *dig = p_ecvt(3.14159, 3, &dec, &sign);
    if (strcmp(dig, "314") != 0 || dec != 1 || sign != 0)
        return std::string("_ecvt gave \"") + dig + "\" dec=" +
               std::to_string(dec) + " sign=" + std::to_string(sign);
    EXPECT(p_sqrtf(2.25f) == 1.5f);
    return {};
}

// printf's floating-point conversions (mingwex, which gets its digits
// from coredll's _ecvt/_fcvt) and strtod/wcstod (coredll). On SH-4 the
// -m4-nofpu build reaches coredll through mingwrt's thunks, the
// -m4-single build directly (docs/sh4/ABI.md).
Result c_float_format()
{
    volatile double vd = 3.14159, vsmall = -0.000123456, vbig = 1234567.0;
    volatile float vf = 2.5f;
    char buf[128];
    snprintf(buf, sizeof buf, "%.3f|%.3e|%g|%g|%g|%.1f", (double)vd,
             (double)vd, (double)vsmall, (double)vbig, (double)vf,
             (double)(vf * 2.0f));
    // Windows CE's CRT, like Microsoft's on the desktop, prints a
    // three-digit exponent.
    if (strcmp(buf, "3.142|3.142e+00|-0.000123456|1.23457e+06|2.5|5.0") != 0
        && strcmp(buf,
                  "3.142|3.142e+000|-0.000123456|1.23457e+006|2.5|5.0") != 0)
        return std::string("got \"") + buf + "\"";

    const char *s = "-1.5e2,0.15625";
    char *end = nullptr;
    double d = strtod(s, &end);
    EXPECT(d == -150.0 && *end == ',');
    d = strtod(end + 1, &end);
    EXPECT(d == 0.15625 && *end == 0);
    float f = (float)strtod("6.25", nullptr);
    EXPECT(f == 6.25f);
    wchar_t *wend = nullptr;
    EXPECT(wcstod(L"-0.75x", &wend) == -0.75 && *wend == L'x');
    return {};
}

Result c_wide()
{
    wchar_t w[64];
    wcscpy(w, L"Hello");
    wcscat(w, L", CE");
    EXPECT(wcslen(w) == 9);
    EXPECT(wcscmp(w, L"Hello, CE") == 0);
    EXPECT(wcsncmp(w, L"Help", 3) == 0);
    EXPECT(wcschr(w, L',') == w + 5);
    EXPECT(wcsstr(w, L"CE") == w + 7);
    EXPECT(wcstol(L"-123", nullptr, 10) == -123);
    EXPECT(wcstoul(L"ff", nullptr, 16) == 255);

    wchar_t f[64];
    int n = _snwprintf(f, 64, L"%d-%s", 12, L"ab");
    EXPECT(n == 5 && wcscmp(f, L"12-ab") == 0);
    n = _snwprintf(f, 64, L"%x", 0xbeef);
    EXPECT(n == 4 && wcscmp(f, L"beef") == 0);

    char mb[16];
    EXPECT(WideCharToMultiByte(CP_ACP, 0, L"abc", -1, mb, sizeof mb,
                               nullptr, nullptr) == 4);
    EXPECT(strcmp(mb, "abc") == 0);
    return {};
}

// ------------------------------------------------------- 64-bit integers

Result int64_division()
{
    volatile int64_t a = -9000000000000000007LL;
    volatile int64_t b = 1000000007LL;
    volatile uint64_t ua = 18000000000000000005ULL;
    volatile uint64_t ub = 3000000019ULL;
    volatile int64_t one = 1;

    EXPECT(a / b == -8999999937LL);
    EXPECT(a % b == -448LL);
    EXPECT(ua / ub == 5999999962ULL);
    EXPECT(ua % ub == 727ULL);
    EXPECT(a / one == a && a % one == 0);
    EXPECT(-a / -b == -8999999937LL);

    volatile uint64_t big = 0xFFFFFFFFFFFFFFFFULL;
    volatile uint64_t three = 3;
    EXPECT(big / three == 0x5555555555555555ULL);

    volatile int64_t m = 123456789LL;
    EXPECT(m * m == 15241578750190521LL);
    volatile int sh = 40;
    EXPECT((uint64_t(1) << sh) == 1099511627776ULL);
    EXPECT((a >> 60) == -8);

    volatile int64_t d = 1234567890123LL;
    EXPECT(double(d) == 1234567890123.0);
    volatile double dd = -9876543210.0;
    EXPECT(int64_t(dd) == -9876543210LL);

    volatile int32_t i = -7, j = 2;
    volatile uint32_t u = 4000000000u, v = 7;
    EXPECT(i / j == -3 && i % j == -1);
    EXPECT(u / v == 571428571u && u % v == 3);
    return {};
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPWSTR cmdline, int)
{
    exe_dir();
    if (cmdline && wcsstr(cmdline, L"--child-dtor"))
        return child_dtor_main();

    g_log = CreateFileW(path_in_dir(L"cetest.txt").c_str(), GENERIC_WRITE,
                        FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr);

    char head[128];
    snprintf(head, sizeof head, "cetest GCC %d.%d.%d, C++ %ld, %s",
             __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
             (long)__cplusplus,
#if defined(__arm__)
             "arm"
#elif defined(__SH4_SINGLE__)
             "sh4 -m4-single"
#elif defined(__SH4__) || defined(__sh__)
             "sh4"
#else
             "other"
#endif
    );
    out(head);

    run("static ctors", static_ctors);
    run("function-local statics", local_statics);
    run("static dtors (child process)", static_dtors);
    run("new/delete", new_delete);
    run("containers", containers);
    run("vocabulary types", vocabulary);
#if __cplusplus > 202002L
    run("C++23 library", cxx23);
#else
    skip("C++23 library", "built as C++20");
#endif
    run("lambdas and std::function", lambdas);
    run("exception in one function", exceptions_local);
    run("exception unwinds 6 frames", exceptions_unwind);
    run("exception types, rethrow, exception_ptr", exceptions_types);
    run("exceptions from the library", exceptions_library);
    skip("exception across a DLL",
         "runtime linked statically, no shared libgcc/libstdc++");
    run("thread_local in the main thread", thread_local_main);
#if defined(_GLIBCXX_HAS_GTHREADS)
    run("thread_local with CreateThread", thread_local_createthread);
    run("std::thread and std::mutex", std_thread_mutex);
#else
    run_xfail("thread_local with CreateThread", thread_local_createthread,
              "threads disabled, emutls has one copy for all threads");
    skip("std::thread and std::mutex", "GCC built with --disable-threads");
#endif
    run("coredll double functions", coredll_double);
    run("snprintf", c_snprintf);
    run("printf %f %e %g, strtod", c_float_format);
    run("wide strings", c_wide);
    run("64-bit and 32-bit division", int64_division);

    char done[96];
    snprintf(done, sizeof done, "DONE %d/%d xfail=%d skip=%d", g_pass,
             g_pass + g_fail + g_xpass, g_xfail, g_skip);
    out(done);
    if (g_log != INVALID_HANDLE_VALUE)
        CloseHandle(g_log);
    return g_fail + g_xpass;
}
