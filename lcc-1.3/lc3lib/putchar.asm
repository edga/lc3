

; void putchar(char)
LC3_GFLAG lc3_putchar LC3_GFLAG .FILL putchar

putchar

STR R7, R6, #-3
STR R0, R6, #-2

LDR R0, R6, #0
OUT

LDR R0, R6, #-2
LDR R7, R6, #-3
ADD R6, R6, #-1
RET
