;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;; START of the header ;;;;;

.Orig x0500
INIT_CODE
LD R5, STACK_ADDR
ADD R6, R5, #0
LD R4, GLOBAL_DATA_POINTER
LD R7, GLOBAL_MAIN_POINTER
jsrr R7

; The main function has finished, so we do nothing
BUSY_LOOP: BR BUSY_LOOP


GLOBAL_DATA_POINTER .FILL GLOBAL_DATA_START
GLOBAL_MAIN_POINTER .FILL main
STACK_ADDR .FILL xdfff

;;;; END of the header ;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;
