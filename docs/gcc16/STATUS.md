# gcc16: status

Specification: [SPEC.md](SPEC.md).

| Gate | State |
|---|---|
| 0 — survey, plan, estimate | done (2026-10-23), waiting for the owner |
| 1 — ARM on GCC 14.2 (salman), probe on an ARM CE 6/7 device | not started |
| 2 — ARM on GCC 16.2 | not started |
| 3 — SH-4 on GCC 16.2 | not started |
| 4 — release `v16.2.0-ce.1` | not started |

## Gate 0 findings (short)

* `releases/gcc-16.2.0` exists (tag `1831ac03`).
* Salman's CE series: 20 commits on 14.2.0; replayed onto 16.2.0 with
  `git merge-tree`: 17 clean, 3 textual conflicts (`arm-protos.h`,
  `system_error.cc`, `c++23/print.cc`). Semantic breakage unknown until
  a build.
* Salman's 14.2 is built `--disable-threads` (GCC 13+ `gthr-win32` needs
  Vista APIs). Expect `std::thread`/`std::mutex` = `SKIP` and
  `thread_local` across `CreateThread` = `FAIL` unless we add a CE
  `gthr` (estimated separately below).
* Exceptions: SJLJ on both 9.3 and 14.2; cross-DLL throw likely not
  supported with static libgcc.
* binutils: salman's patch set is 14 small commits on 2.43.1. Latest
  upstream release could not be listed from the container (sourceware
  blocked, GitHub mirror requires auth) — decide at Gate 2 start.

## Cost estimate (session credit, USD)

| Gate | Work | Estimate | Main risk |
|---|---|---|---|
| 1 | Dockerfile stage for salman's trees, test program, Actions workflow with artifacts (builds run on Actions, not in the session) | 8–12 | test program needs CE workarounds in salman's runtime |
| 2 | Forward-port 20 commits + 14 binutils commits, iterate on build errors (ARM backend, libstdc++ C++23 bits) | 12–20 | `arm.cc`/`pe.cc` hook drift; each rebuild cycle ~20 min of CI |
| 2b (optional) | CE `gthr` (CRITICAL_SECTION + TLS) to get `std::thread`/`std::mutex`/per-thread `thread_local` | 6–10 | gthreads API surface (`__gthread_cond_*` for `condition_variable`) |
| 3 | Port SH-4 GCC (4 commits) + binutils (3 commits) to 16.2, ABI harness + mutation control, artifacts | 12–20 | `sh.cc` drift; binutils SH PE on newer bfd |
| 4 | Image tags, README, CI | 3–5 | registry credentials already in CI |
| **Total** | without 2b | **35–57** | |
| | with 2b | **41–67** | |

Budget ~$69 (expires 2026-11-05). Without 2b it fits with margin; with
2b it is at the limit. Recommendation: decide on 2b after the Gate 1
run, when we know whether threads matter on-device.

## Cost so far

| Gate | Spent |
|---|---|
| 0 | ~3 (estimate; survey only, no builds) |
