	LWS F0 10(R1)
	LWS F2 20(R2)
	ADDS F3 F0 F2
	MULTS F4 F0 F5
	BEQZ R3 TARGET
	SUBS F2 F5 F4
	DIVS F6 F10 F0
	ADDS F5 F7 F8
TARGET:	ADDS F4 F2 F3
	SUBS F2 F5 F4
	EOP
