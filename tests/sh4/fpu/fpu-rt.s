! FPU thunk test (run-fpu-test.sh): import pointers that send mingwrt's
! coredll thunks to Microsoft /Qsh4 code instead of coredll.dll, and a
! callee in the /Qsh4 convention that records what it received.
	.little
	.text
	.align	2
	.global	_get_fpscr
_get_fpscr:
	rts
	sts	fpscr,r0

	.global	_set_fpscr
_set_fpscr:
	rts
	lds	r4,fpscr

! fpu_callee: rec[] = fr4, fr5, fr6, fr7, r4, r5, r6, r7, @(16,r15)
! (first stack argument), FPSCR.  Returns dr0 = dr4 and r0 = 1234.
! extdbl, which the /Qsh4 calldbl calls, is the same function.
	.global	_fpu_callee
	.global	_extdbl
_fpu_callee:
_extdbl:
	mov.l	Lrec,r1
	flds	fr4,fpul
	sts	fpul,r0
	mov.l	r0,@r1
	flds	fr5,fpul
	sts	fpul,r0
	mov.l	r0,@(4,r1)
	flds	fr6,fpul
	sts	fpul,r0
	mov.l	r0,@(8,r1)
	flds	fr7,fpul
	sts	fpul,r0
	mov.l	r0,@(12,r1)
	mov.l	r4,@(16,r1)
	mov.l	r5,@(20,r1)
	mov.l	r6,@(24,r1)
	mov.l	r7,@(28,r1)
	mov.l	@(16,r15),r0
	mov.l	r0,@(32,r1)
	sts	fpscr,r0
	mov.l	r0,@(36,r1)
	fmov	fr4,fr0
	fmov	fr5,fr1
	mov.w	L1234,r0
	rts
	nop
L1234:	.word	1234
	.align	2
Lrec:	.long	_rec

	.data
	.align	2
! atof (kind D_I) -> calldbl (/Qsh4): double calldbl (int x)
!   { return extdbl ((double) x, x) + 1.0; }
	.global	__imp__atof
__imp__atof:	.long	_calldbl_fpu
! fmod (kind D_DD) -> fl (/Qsh4): double fl (double x, float y)
!   { return x * y + 1.0; }, y read from fr6
	.global	__imp__fmod
__imp__fmod:	.long	_fl_fpu
	.global	__imp___ecvt
__imp___ecvt:	.long	_fpu_callee
	.global	__imp__sqrt
__imp__sqrt:	.long	_fpu_callee
