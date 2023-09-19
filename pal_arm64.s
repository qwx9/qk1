TEXT	pal2xrgb+0(SB), 1, $-4
	MOV pal+8(FP), R1
	MOV s+16(FP), R2
	MOV d+24(FP), R3

	ADD R2, R0

	CMP $8, R0
	BLE _l1

	AND $7, R0, R12
	SUB R12, R0, R12

_l8:
	MOVWU 0(R2), R4
	MOVWU 4(R2), R8
	UBFXW $8, $8, R4, R5
	UBFXW $8, $8, R8, R9
	UBFXW $16, $8, R4, R6
	UBFXW $16, $8, R8, R10
	UBFXW $24, $8, R4, R7
	MOVBU R4, R4
	UBFXW $24, $8, R8, R11
	MOVBU R8, R8
	MOVWU (R1)[R4], R13
	MOVWU (R1)[R5], R14
	MOVWU (R1)[R6], R15
	MOVWU (R1)[R7], R16
	MOVWU (R1)[R8], R17
	MOVWU (R1)[R9], R18
	MOVWU (R1)[R10], R19
	MOVWU (R1)[R11], R20
	MOVPW R13, R14, 0(R3)
	MOVPW R15, R16, 8(R3)
	MOVPW R17, R18, 16(R3)
	MOVPW R19, R20, 24(R3)
	ADD $32, R3
	ADD $8, R2
	CMP R2, R12
	BNE _l8
	CMP R2, R0
	BEQ _end

_l1:
	MOVBU 1(R2)!, R4
	MOVWU (R1)[R4], R4
	MOVWU R4, 4(R3)!
	CMP R2, R0
	BNE _l1

_end:
	RETURN
