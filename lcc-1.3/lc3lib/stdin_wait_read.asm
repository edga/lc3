;;;;;;;;;;;;;;;;;;;;;;;;;;;;stdin_wait_read;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; short stdin_wait_read();
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
LC3_GFLAG lc3_stdin_wait_read LC3_GFLAG .FILL stdin_wait_read
swr_STDIN_S .FILL xfe00
        
stdin_wait_read
ADD R6, R6, #-1    ; reserve place for return val 

STR R7, R6, #-1    ; push R7
STR R0, R6, #-2    ; pop R0

LD R7, swr_STDIN_S
LDR R0, R7, #0     ; read status
BRzp #-2
        
LDR R7, R7, #2     ; read IO 
        
STR R7, R6, #0     ; put return val 

LDR R0, R6, #-2    ; pop R0
LDR R7, R6, #-1    ; pop R7
RET
