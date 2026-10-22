! Minimal SH-4 Windows CE program in assembly: MessageBoxW (0, msg, msg, 0).
! Calls follow the Microsoft convention (16-byte home area, see
! docs/sh4/msvc-sh4-abi-reference.txt, call2).
	.text
	.align	2
	.global	_WinMainCRTStartup
_WinMainCRTStartup:
	sts.l	pr,@-r15
	add	#-16,r15
	mov	#0,r4
	mov.l	Lmsg,r5
	mov.l	Lmsg,r6
	mov.l	Lfn,r3
	jsr	@r3
	mov	#0,r7
	add	#16,r15
	lds.l	@r15+,pr
	rts
	mov	#0,r0
	.align	2
Lmsg:	.long	_msg
Lfn:	.long	_MessageBoxW

	.data
	.align	1
_msg:	.short	'h','i',0
