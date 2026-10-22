! Minimal runtime for running the ABI tests under qemu-sh4 (Linux user mode):
! entry point, exit and write system calls.  Calls into C follow the
! Microsoft convention: 16 bytes of home area below the arguments.
	.text
	.align	2
	.global	_start
_start:
	mov.l	Lmain,r1
	add	#-16,r15
	jsr	@r1
	nop
	mov	r0,r4
	mov	#1,r3		! __NR_exit
	trapa	#0x17
	.align	2
Lmain:	.long	_test_main

	.global	_sys_write
_sys_write:
	mov	#4,r3		! __NR_write
	trapa	#0x17
	rts
	nop

! int home_clobber (int a, int b, int c, void *local): writes all 16 bytes
! of the caller-reserved home area, as Microsoft-compiled callees may (take64
! stores r6 there; wsprintfW spills r4..r7).  Returns a+b+c, or -1 if LOCAL,
! the address of a live object in the caller's frame, lies below
! r15 + 16, i.e. inside the area the callee may overwrite.
	.global	_home_clobber
_home_clobber:
	mov	r15,r1
	add	#16,r1
	cmp/hs	r1,r7		! local >= sp + 16 ?
	bf	Lbad
	mov.l	Lpat,r0
	mov.l	r0,@(0,r15)
	mov.l	r0,@(4,r15)
	mov.l	r0,@(8,r15)
	mov.l	r0,@(12,r15)
	add	r5,r4
	add	r6,r4
	rts
	mov	r4,r0
Lbad:	rts
	mov	#-1,r0
	.align	2
Lpat:	.long	0xdeadbeef
