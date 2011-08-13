
; char getchar(void)
LC3_GFLAG lc3_getchar LC3_GFLAG .FILL getchar

getchar

STR R7, R6, #-3
STR R0, R6, #-2
GETC
OUT
STR R0, R6, #-1
LDR R0, R6, #-2
LDR R7, R6, #-3
ADD R6, R6, #-1
RET
