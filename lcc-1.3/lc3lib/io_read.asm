;;;;;;;;;;;;;;;;;;;;;;;;;;;;io_read;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; short io_read(unsigned short io_addr);
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
LC3_GFLAG lc3_io_read LC3_GFLAG .FILL io_read
io_read
ADD R6, R6, #-1    ; reserve place for return val 

STR R7, R6, #-1    ; push R7

LDR R7, R6, #1     ; get io_addr
LDR R7, R7, #0     ; read IO
        
STR R7, R6, #0     ; put return val 

LDR R7, R6, #-1    ; pop R7
RET
