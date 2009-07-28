;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;  LC-3 Simulator
;;  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
;;
;;  This program is free software; you can redistribute it and/or modify
;;  it under the terms of the GNU General Public License as published by
;;  the Free Software Foundation; either version 2 of the License, or
;;  (at your option) any later version.
;;
;;  This program is distributed in the hope that it will be useful,
;;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;  GNU General Public License for more details.
;;
;;  You should have received a copy of the GNU General Public License
;;  along with this program; if not, write to the Free Software
;;  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	.ORIG x0000

	;; -------------------------------------------------------------------
	;;                           VECTORS
	;; -------------------------------------------------------------------

	.BLKWTO x0020
	.FILL GETC_T 		; x0020
	.FILL OUT_T		; x0021
	.FILL PUTS_T		; x0022
	.FILL IN_T		; x0023
	.FILL PUTSP_T		; x0024
	.FILL HALT_T		; x0025

	.BLKWTO x0100
	.FILL PRIV_EXCEPTION	; x0100
	.FILL ILL_EXCEPTION	; x0101

	.BLKWTO x01FF
	.FILL STARTUP		; x01FF

	;; -------------------------------------------------------------------
	;;                           BOOT CODE
	;; -------------------------------------------------------------------

	;; KERNEL BOOT CODE
	;;
	;; This code initializes the supervisor stack and drops to user
	;; privilege mode.  Be very careful modifying it.
	;; 
	.BLKW x80
	.FILL BANNER
	.FILL x0000	
	.FILL x8000
SUPERVISOR_STACK


STARTUP
	LEA R6, SUPERVISOR_STACK
	ADD R6, R6, -3
	RTI

	;; 
	;; USER BOOT CODE
	;;
	;; This code is the piece of code that gets called once the machine is
	;; booted.  It's certainly not necessary.
	;; 
BANNER
	LEA R0,BANNER_STR
	LEA R6, BANNER_STACK
	PUTS
	HALT

BANNER_STR
	 .STRINGZ "LittleOS v0.1\nWritten by Anthony Liguori <aliguori@cs.utexas.edu>\n"
BANNER_STACK_START
	.BLKW 7
BANNER_STACK .FILL x0000

	;; -------------------------------------------------------------------
	;;                           TRAP ROUTINES
	;; -------------------------------------------------------------------
	
	;;
	;; GETC TRAP ROUTINE
	;;
	;; Gets a single character from the input device.
	;; 
	;; [out] R0 <= The character read from the device in R0[7:0]
	;; [in] R6 <= The process stack
	;; 
GETC_T
	LDI R0, KBSR
	BRzp GETC_T
	LDI R0, KBDR
	RET

KBSR	.FILL xFE00
KBDR	.FILL xFE02
	
	
	;;
	;; OUT TRAP ROUTINE
	;;
	;; Displays a single character to the display device
	;; 
	;; [in] R0 <= The character to display in R0[7:0]
	;; [in] R6 <= The process stack
	;; 
OUT_T
	STR R1, R6, 0		; push callee saved registers
	ADD R6, R6, -1

OUT_LOOP
	LDI R1, DSR
	BRzp OUT_LOOP		; loop until MSB(DSR) == 1
	STI R0, DDR		; write to DDR

	ADD R6, R6, 1		; pop callee saved registers
	LDR R1, R6, 0
	RET

DSR	.FILL xFE04
DDR	.FILL xFE06

	;;
	;; PUTS TRAP ROUTINE
	;;
	;; Displays a wide-character string to the display device
	;; 
	;; [in] R0 <= The address of the beginning of the string to display
	;; [in] R6 <= The process stack
	;; 
PUTS_T
	STR R0, R6, 0		; push callee saved registers
	STR R1, R6, -1
	STR R7, R6, -2
	ADD R6, R6, -3

	LDR R1, R6, 3		; R1 <= R0 (we need to use R0 for OUT call)
	BR 1
PUTS_LOOP
	OUT
	ADD R1, R1, 1
	LDR R0, R1, -1		; loop until null terminator
	BRnp PUTS_LOOP

	ADD R6, R6, 3		; pop callee saved registers
	LDR R7, R6, -2
	LDR R1, R6, -1
	LDR R0, R6, 0
	RET

	;;
	;; IN TRAP ROUTINE
	;;
	;; Displays a prompt to the display device and gets a single character.
	;; 
	;; [out] R0 <= The character read from the display device in R0[7:0]
	;; [in] R6 <= The process stack
	;; 
IN_T
        STR R1, R6, 0           ; push callee saved registers
	STR R7, R6, -1
        ADD R6, R6, -2

        LEA R0, IN_PROMPT       ; display prompt
        PUTS

        GETC                    ; input character and display
        OUT

        ADD R1, R0, 0           ; temporarily save character in R1

        AND R0, R0, 0           ; output newline
        ADD R0, R0, x0A
        OUT

        ADD R0, R1, 0           ; restore character to R0

        ADD R6, R6, 2           ; pop callee saved registers
	LDR R7, R6, -1
        LDR R1, R6, 0
        RET

IN_PROMPT
        .STRINGZ        "Enter a character: "

	;;
	;; PUTSP TRAP ROUTINE
	;;
	;; Display a byte-packed string to the display device.
	;; 
	;; [in] R0 <= The address of the first word of the byte packed string
	;; [in] R6 <= The process stack
	;; 
PUTSP_T
	;; not implemented
	RET

	;;
	;; HALT TRAP ROUTINE
	;;
	;; Halts the machine.
	;; 
HALT_T
	LD R0, MASK
	LDI R1, MCR
	AND R0, R1, R0		; load MCR and set MSB[MCR] to 0

	STI R0, MCR		; halt the machine
	BR HALT_T

MCR	.FILL xFFFE
MASK	.FILL x7FFF

	;; -------------------------------------------------------------------
	;;                           EXCEPTIONS
	;; -------------------------------------------------------------------

	;;
	;; PRIVILEGE EXCEPTION
	;; 
	;; This exception is invoked if an RTI instruction is executed when
	;; PSR[15] == 1.
	;; 
PRIV_EXCEPTION
	ADD R6, R6, -2
	STR R0, R6, 1		; push callee saved registers
	
	LEA R0, PRIV_MESSAGE	; display a warning
	PUTS

	LDR R0, R6, 1
	ADD R6, R6, 2		; pop callee saved registers
	RTI

PRIV_MESSAGE
	.STRINGZ	"\n** EXCEPTION: RTI instruction in user mode **\n"

	;;
	;; ILLEGAL INSTRUCTION EXCEPTION
	;; 
	;; This exception is invoked if the reserved opcode is invoked.
	;; 
ILL_EXCEPTION
	ADD R6, R6, -2
	STR R0, R6, 1		; push callee saved registers
	
	LEA R0, ILL_MESSAGE	; display a warning
	PUTS

	LDR R0, R6, 1
	ADD R6, R6, 2		; pop callee saved registers
	RTI

ILL_MESSAGE
	.STRINGZ	"\n** EXCEPTION: Illegal instruction **\n"

	.END
