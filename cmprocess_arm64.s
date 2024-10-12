TEXT cmprocess(SB), $0
	MOV in+8(FP), R1
	MOV out+16(FP), R2
	MOVW cnt+24(FP), R3

	WORD $0x4c40a000 // ld1.16b {v0, v1}, [x0]
	WORD $0x6e3d1fbd // eor.16b v29, v29, v29
	WORD $0x4f0707fe // movi v30.4s, 0xff, lsl 0
conv:
	WORD $0x0ddf803f // ld1 {v31.s}[0], [x1], 4
	WORD $0x0f0777ff // orr v31.2s, 0xff, lsl 24
	WORD $0x2f08a7ff // uxtl.8h v31, v31
	WORD $0x4e0807ff // dup v31.2d, v31.d[0]
	WORD $0x0e60c3e2 // smull v2.4s, v31.4h, v0.4h
	WORD $0x4e60c3e3 // smull2 v3.4s, v31.8h, v0.8h
	WORD $0x0e61c3e4 // smull v4.4s, v31.4h, v1.4h
	WORD $0x4e61c3e5 // smull2 v5.4s, v31.8h, v1.8h
	WORD $0x4eb1b842 // addv s2, v2.4s
	WORD $0x4eb1b863 // addv s3, v3.4s
	WORD $0x4eb1b884 // addv s4, v4.4s
	WORD $0x4eb1b8a5 // addv s5, v5.4s
	WORD $0x4e832842 // trn1.4s v2, v2, v3
	WORD $0x4e852884 // trn1.4s v4, v4, v5
	WORD $0x4ec42842 // trn1.2d v2, v2, v4
	WORD $0x4f340442 // sshr.4s v2, v2, 12
	WORD $0x4ebd6442 // smax.4s v2, v2, v29
	WORD $0x4ebe6c42 // smin.4s v2, v2, v30
	WORD $0x0e612842 // xtn v2.4h, v2.4s
	WORD $0x0e212842 // xtn v2.8b, v2.8h
	WORD $0x0d9f8042 // st1.s {v2}[0], [x2], 4

	SUBW $1, R3, R3
	CBNZW R3, conv

	RETURN
