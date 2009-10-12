;;; Test of software serial comunication.
;;; 
;;; The data is read/written byte by byte
;;; 

	.ORIG x3000

	LD R2, OS_SSR	; store OS_SSR and keep it there while in this proc 

Again:  
	; read the byte
	LDR R0, R2, 0
	BRzp -2		; poll until data available
	LDR R0, R2, 2

	; write the byte
   LDR R1, R2, 4
	BRzp -2		; poll until transmission buffers ready
   STR R0, R2, 6
        
   BR Again

OS_SSR .FILL xFE00

SSEGS	.FILL xFE10
SSEGD	.FILL xFE12
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Write R0 onto 7 segment display
;-------------------------------
Write7seg
	LDI R1,SSEGS		; wait for the display to be ready
	BRzp -2
	STI R0,SSEGD		; write the character and return

	RET




.END


