;;;;;;;;;;;;;;;;;;;;;;;;;;;;kb_wait_read;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; short kb_wait_read();
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
LC3_GFLAG lc3_ps2kbd_wait_read LC3_GFLAG .FILL ps2kbd_wait_read
kwr_KB_S .FILL xfe18
        
ps2kbd_wait_read
ADD R6, R6, #-1    ; reserve place for return val 

STR R7, R6, #-1    ; push R7
STR R0, R6, #-2    ; push R0

LD R7, kwr_KB_S
LDR R0, R7, #0     ; read status
BRzp #-2
        
LDR R7, R7, #2     ; read IO 
        
STR R7, R6, #0     ; put return val 

LDR R0, R6, #-2    ; pop R0
LDR R7, R6, #-1    ; pop R7
RET
