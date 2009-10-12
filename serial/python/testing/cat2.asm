;;; Test of software serial comunication.
;;; 
;;; LoadWordFromSerial is function used in boot loader (it uses no extension instructions)
;;; WriteWordToSerial uses shift instruction extension 

        
	.ORIG x3000

Again:  
   jsr LoadWordFromSerial
   jsr WriteWordToSerial
   BR Again
	
;;;;;;;;;;;;;;;;;;;;
; LoadWordFromSerial
;-------------------
; IN:	--
; OUT:	R0 holds word just read
; KILL:	R1, R2
;-------------------
;					proc_size:14
;LOCALS:
; Used to detect shift has been performed 7 times
  Bit9	.FILL	x0100
	
LoadWordFromSerial
	LD R2, OS_SSR	; store OS_SSR and keep it there while in this proc 

	; read the First byte (most significant in LC3)
	LDR R0, R2, 0
	BRzp -2		; poll until next word available
	LDR R0, R2, 2

	; read the Second byte
	LDR R1, R2, 0
	BRzp -2		; poll until next word available
	LDR R1, R2, 2
	
	; shift: R0 := R0 << 8 
	LD R2, Bit9	; load constant
	ADD R0, R2, R0	; prepare
	  ; shift R0 8 times
	ADD R0, R0, R0	; R0 := 2*R0
	BRzp -2			; 7 times
	ADD R0, R0, R0	; once more


	; form result (concat bytes)
	ADD R0, R1, R0
	ret

OS_SSR   .FILL xFe00
OS_SSR0   .FILL xFe04

MASK .FILL x00FF

WriteWordToSerial
	LD R2, OS_SSR0	; store OS_SSR and keep it there while in this proc 

   SRA R1, R0, 8
        
	; write the First byte (most significant in LC3)
	LDR R3, R2, 0
	BRzp -2		; poll until next word available
	STR R1, R2, 2

	; write the Second byte
	LDR R3, R2, 0
	BRzp -2		; poll until next word available
	STR R0, R2, 2
	
	ret

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


