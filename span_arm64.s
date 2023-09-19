TEXT	dospan+0(SB), 1, $-4
	MOV pbase+8(FP), R1
	MOVW s+16(FP), R2
	MOVW t+24(FP), R3
	MOVW sstep+32(FP), R4
	MOVW tstep+40(FP), R5
	MOVW spancount+48(FP), R6
	MOVW cachewidth+56(FP), R7
	LSRW $16, R3, R9
	LSRW $16, R2, R8

_l:
	SUBW $1, R6
	MADDW R7, R9, R8, R8
	ADDW R5, R3
	MOVBU (R1)[R8], R10
	ADDW R4, R2
	MOVBU R10, 1(R0)!
	LSRW $16, R3, R9
	LSRW $16, R2, R8
	CBNZ R6, _l

	RETURN
