;;;;;;;;;;;;;;;;;;;;;;;;;;;;io_write;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; void io_write(unsigned short io_addr, short val);
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
LC3_GFLAG lc3_io_write LC3_GFLAG .FILL io_write
io_write
; I know that it is void function, but compiler is a bit stupid,
; and the caller will try to pop return value anyway
ADD R6, R6, #-1    ; reserve place for return value 

STR R7, R6, #-1    ; push R7
STR R0, R6, #-2    ; push R0

LDR R7, R6, #1     ; get io_addr     
LDR R0, R6, #2     ; get val    
STR R0, R7, #0     ; write IO

LDR R0, R6, #-2    ; pop R0
LDR R7, R6, #-1    ; pop R7
RET
