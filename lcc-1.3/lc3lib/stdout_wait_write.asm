;;;;;;;;;;;;;;;;;;;;;;;;;;;;stdout_wait_write;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; void stdout_wait_write(short val);
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
LC3_GFLAG stdout_wait_write LC3_GFLAG .FILL lc3_stdout_wait_write
sww_STDOUT_S .FILL xfe04
        
lc3_stdout_wait_write
; I know that it is void function, but compiler is a bit stupid,
; and the caller will try to pop return value anyway
ADD R6, R6, #-1    ; reserve place for return val 

STR R7, R6, #-1    ; push R7
STR R0, R6, #-2    ; push R0

LD R7, sww_STDOUT_S
LDR R0, R7, #0     ; read status
BRzp #-2
        
LDR R0, R6, #1     ; get val    
STR R0, R7, #2     ; write IO 
        
STR R7, R6, #0     ; put return val 

LDR R0, R6, #-2    ; pop R0
LDR R7, R6, #-1    ; pop R7
RET
