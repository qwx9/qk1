TEXT DotProduct+0(SB), 1, $-4
	MOV	v2+8(FP), R1
	WORD $0x0d40a000 // ld3 {v0.s, v1.s, v2.s}[0], [x0]
	WORD $0x0d40a023 // ld3 {v3.s, v4.s, v5.s}[0], [x1]
	WORD $0x1e230800 // fmul s0, s0, s3
	WORD $0x1f040020 // fmadd s0, s1, s4, s0
	WORD $0x1f050040 // fmadd s0, s2, s5, s0
	RETURN
