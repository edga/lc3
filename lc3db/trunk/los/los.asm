;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;  LC-3 Simulator
;;  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
;;  Copyright (C) 2010  Edgar Lakis <edgar.lakis@gmail.com>
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
;;
;;  Change log
;;
;;  2010-11-11: Removed stack dependency from traps to make it more general
;;              (not all user code uses R6 as stack pointer)
;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	.ORIG x0000

	;; -------------------------------------------------------------------
	;;                           VECTORS
	;; -------------------------------------------------------------------

	.FILL UNDEFINED_T	; x00
	.FILL UNDEFINED_T	; x01
	.FILL UNDEFINED_T	; x02
	.FILL UNDEFINED_T	; x03
	.FILL UNDEFINED_T	; x04
	.FILL UNDEFINED_T	; x05
	.FILL UNDEFINED_T	; x06
	.FILL UNDEFINED_T	; x07
	.FILL UNDEFINED_T	; x08
	.FILL UNDEFINED_T	; x09
	.FILL UNDEFINED_T	; x0A
	.FILL UNDEFINED_T	; x0B
	.FILL UNDEFINED_T	; x0C
	.FILL UNDEFINED_T	; x0D
	.FILL UNDEFINED_T	; x0E
	.FILL UNDEFINED_T	; x0F
	.FILL UNDEFINED_T	; x10
	.FILL UNDEFINED_T	; x11
	.FILL UNDEFINED_T	; x12
	.FILL UNDEFINED_T	; x13
	.FILL UNDEFINED_T	; x14
	.FILL UNDEFINED_T	; x15
	.FILL UNDEFINED_T	; x16
	.FILL UNDEFINED_T	; x17
	.FILL UNDEFINED_T	; x18
	.FILL UNDEFINED_T	; x19
	.FILL UNDEFINED_T	; x1A
	.FILL UNDEFINED_T	; x1B
	.FILL UNDEFINED_T	; x1C
	.FILL UNDEFINED_T	; x1D
	.FILL UNDEFINED_T	; x1E
	.FILL UNDEFINED_T	; x1F
	.FILL GETC_T 		; x20
	.FILL OUT_T		; x21
	.FILL PUTS_T		; x22
	.FILL IN_T		; x23
	.FILL PUTSP_T		; x24
	.FILL HALT_T		; x25
	.FILL UNDEFINED_T	; x26
	.FILL UNDEFINED_T	; x27
	.FILL UNDEFINED_T	; x28
	.FILL UNDEFINED_T	; x29
	.FILL UNDEFINED_T	; x2A
	.FILL UNDEFINED_T	; x2B
	.FILL UNDEFINED_T	; x2C
	.FILL UNDEFINED_T	; x2D
	.FILL UNDEFINED_T	; x2E
	.FILL UNDEFINED_T	; x2F
	.FILL UNDEFINED_T	; x30
	.FILL UNDEFINED_T	; x31
	.FILL UNDEFINED_T	; x32
	.FILL UNDEFINED_T	; x33
	.FILL UNDEFINED_T	; x34
	.FILL UNDEFINED_T	; x35
	.FILL UNDEFINED_T	; x36
	.FILL UNDEFINED_T	; x37
	.FILL UNDEFINED_T	; x38
	.FILL UNDEFINED_T	; x39
	.FILL UNDEFINED_T	; x3A
	.FILL UNDEFINED_T	; x3B
	.FILL UNDEFINED_T	; x3C
	.FILL UNDEFINED_T	; x3D
	.FILL UNDEFINED_T	; x3E
	.FILL UNDEFINED_T	; x3F
	.FILL UNDEFINED_T	; x40
	.FILL UNDEFINED_T	; x41
	.FILL UNDEFINED_T	; x42
	.FILL UNDEFINED_T	; x43
	.FILL UNDEFINED_T	; x44
	.FILL UNDEFINED_T	; x45
	.FILL UNDEFINED_T	; x46
	.FILL UNDEFINED_T	; x47
	.FILL UNDEFINED_T	; x48
	.FILL UNDEFINED_T	; x49
	.FILL UNDEFINED_T	; x4A
	.FILL UNDEFINED_T	; x4B
	.FILL UNDEFINED_T	; x4C
	.FILL UNDEFINED_T	; x4D
	.FILL UNDEFINED_T	; x4E
	.FILL UNDEFINED_T	; x4F
	.FILL UNDEFINED_T	; x50
	.FILL UNDEFINED_T	; x51
	.FILL UNDEFINED_T	; x52
	.FILL UNDEFINED_T	; x53
	.FILL UNDEFINED_T	; x54
	.FILL UNDEFINED_T	; x55
	.FILL UNDEFINED_T	; x56
	.FILL UNDEFINED_T	; x57
	.FILL UNDEFINED_T	; x58
	.FILL UNDEFINED_T	; x59
	.FILL UNDEFINED_T	; x5A
	.FILL UNDEFINED_T	; x5B
	.FILL UNDEFINED_T	; x5C
	.FILL UNDEFINED_T	; x5D
	.FILL UNDEFINED_T	; x5E
	.FILL UNDEFINED_T	; x5F
	.FILL UNDEFINED_T	; x60
	.FILL UNDEFINED_T	; x61
	.FILL UNDEFINED_T	; x62
	.FILL UNDEFINED_T	; x63
	.FILL UNDEFINED_T	; x64
	.FILL UNDEFINED_T	; x65
	.FILL UNDEFINED_T	; x66
	.FILL UNDEFINED_T	; x67
	.FILL UNDEFINED_T	; x68
	.FILL UNDEFINED_T	; x69
	.FILL UNDEFINED_T	; x6A
	.FILL UNDEFINED_T	; x6B
	.FILL UNDEFINED_T	; x6C
	.FILL UNDEFINED_T	; x6D
	.FILL UNDEFINED_T	; x6E
	.FILL UNDEFINED_T	; x6F
	.FILL UNDEFINED_T	; x70
	.FILL UNDEFINED_T	; x71
	.FILL UNDEFINED_T	; x72
	.FILL UNDEFINED_T	; x73
	.FILL UNDEFINED_T	; x74
	.FILL UNDEFINED_T	; x75
	.FILL UNDEFINED_T	; x76
	.FILL UNDEFINED_T	; x77
	.FILL UNDEFINED_T	; x78
	.FILL UNDEFINED_T	; x79
	.FILL UNDEFINED_T	; x7A
	.FILL UNDEFINED_T	; x7B
	.FILL UNDEFINED_T	; x7C
	.FILL UNDEFINED_T	; x7D
	.FILL UNDEFINED_T	; x7E
	.FILL UNDEFINED_T	; x7F
	.FILL UNDEFINED_T	; x80
	.FILL UNDEFINED_T	; x81
	.FILL UNDEFINED_T	; x82
	.FILL UNDEFINED_T	; x83
	.FILL UNDEFINED_T	; x84
	.FILL UNDEFINED_T	; x85
	.FILL UNDEFINED_T	; x86
	.FILL UNDEFINED_T	; x87
	.FILL UNDEFINED_T	; x88
	.FILL UNDEFINED_T	; x89
	.FILL UNDEFINED_T	; x8A
	.FILL UNDEFINED_T	; x8B
	.FILL UNDEFINED_T	; x8C
	.FILL UNDEFINED_T	; x8D
	.FILL UNDEFINED_T	; x8E
	.FILL UNDEFINED_T	; x8F
	.FILL UNDEFINED_T	; x90
	.FILL UNDEFINED_T	; x91
	.FILL UNDEFINED_T	; x92
	.FILL UNDEFINED_T	; x93
	.FILL UNDEFINED_T	; x94
	.FILL UNDEFINED_T	; x95
	.FILL UNDEFINED_T	; x96
	.FILL UNDEFINED_T	; x97
	.FILL UNDEFINED_T	; x98
	.FILL UNDEFINED_T	; x99
	.FILL UNDEFINED_T	; x9A
	.FILL UNDEFINED_T	; x9B
	.FILL UNDEFINED_T	; x9C
	.FILL UNDEFINED_T	; x9D
	.FILL UNDEFINED_T	; x9E
	.FILL UNDEFINED_T	; x9F
	.FILL UNDEFINED_T	; xA0
	.FILL UNDEFINED_T	; xA1
	.FILL UNDEFINED_T	; xA2
	.FILL UNDEFINED_T	; xA3
	.FILL UNDEFINED_T	; xA4
	.FILL UNDEFINED_T	; xA5
	.FILL UNDEFINED_T	; xA6
	.FILL UNDEFINED_T	; xA7
	.FILL UNDEFINED_T	; xA8
	.FILL UNDEFINED_T	; xA9
	.FILL UNDEFINED_T	; xAA
	.FILL UNDEFINED_T	; xAB
	.FILL UNDEFINED_T	; xAC
	.FILL UNDEFINED_T	; xAD
	.FILL UNDEFINED_T	; xAE
	.FILL UNDEFINED_T	; xAF
	.FILL UNDEFINED_T	; xB0
	.FILL UNDEFINED_T	; xB1
	.FILL UNDEFINED_T	; xB2
	.FILL UNDEFINED_T	; xB3
	.FILL UNDEFINED_T	; xB4
	.FILL UNDEFINED_T	; xB5
	.FILL UNDEFINED_T	; xB6
	.FILL UNDEFINED_T	; xB7
	.FILL UNDEFINED_T	; xB8
	.FILL UNDEFINED_T	; xB9
	.FILL UNDEFINED_T	; xBA
	.FILL UNDEFINED_T	; xBB
	.FILL UNDEFINED_T	; xBC
	.FILL UNDEFINED_T	; xBD
	.FILL UNDEFINED_T	; xBE
	.FILL UNDEFINED_T	; xBF
	.FILL UNDEFINED_T	; xC0
	.FILL UNDEFINED_T	; xC1
	.FILL UNDEFINED_T	; xC2
	.FILL UNDEFINED_T	; xC3
	.FILL UNDEFINED_T	; xC4
	.FILL UNDEFINED_T	; xC5
	.FILL UNDEFINED_T	; xC6
	.FILL UNDEFINED_T	; xC7
	.FILL UNDEFINED_T	; xC8
	.FILL UNDEFINED_T	; xC9
	.FILL UNDEFINED_T	; xCA
	.FILL UNDEFINED_T	; xCB
	.FILL UNDEFINED_T	; xCC
	.FILL UNDEFINED_T	; xCD
	.FILL UNDEFINED_T	; xCE
	.FILL UNDEFINED_T	; xCF
	.FILL UNDEFINED_T	; xD0
	.FILL UNDEFINED_T	; xD1
	.FILL UNDEFINED_T	; xD2
	.FILL UNDEFINED_T	; xD3
	.FILL UNDEFINED_T	; xD4
	.FILL UNDEFINED_T	; xD5
	.FILL UNDEFINED_T	; xD6
	.FILL UNDEFINED_T	; xD7
	.FILL UNDEFINED_T	; xD8
	.FILL UNDEFINED_T	; xD9
	.FILL UNDEFINED_T	; xDA
	.FILL UNDEFINED_T	; xDB
	.FILL UNDEFINED_T	; xDC
	.FILL UNDEFINED_T	; xDD
	.FILL UNDEFINED_T	; xDE
	.FILL UNDEFINED_T	; xDF
	.FILL UNDEFINED_T	; xE0
	.FILL UNDEFINED_T	; xE1
	.FILL UNDEFINED_T	; xE2
	.FILL UNDEFINED_T	; xE3
	.FILL UNDEFINED_T	; xE4
	.FILL UNDEFINED_T	; xE5
	.FILL UNDEFINED_T	; xE6
	.FILL UNDEFINED_T	; xE7
	.FILL UNDEFINED_T	; xE8
	.FILL UNDEFINED_T	; xE9
	.FILL UNDEFINED_T	; xEA
	.FILL UNDEFINED_T	; xEB
	.FILL UNDEFINED_T	; xEC
	.FILL UNDEFINED_T	; xED
	.FILL UNDEFINED_T	; xEE
	.FILL UNDEFINED_T	; xEF
	.FILL UNDEFINED_T	; xF0
	.FILL UNDEFINED_T	; xF1
	.FILL UNDEFINED_T	; xF2
	.FILL UNDEFINED_T	; xF3
	.FILL UNDEFINED_T	; xF4
	.FILL UNDEFINED_T	; xF5
	.FILL UNDEFINED_T	; xF6
	.FILL UNDEFINED_T	; xF7
	.FILL UNDEFINED_T	; xF8
	.FILL UNDEFINED_T	; xF9
	.FILL UNDEFINED_T	; xFA
	.FILL UNDEFINED_T	; xFB
	.FILL UNDEFINED_T	; xFC
	.FILL UNDEFINED_T	; xFD
	.FILL UNDEFINED_T	; xFE
	.FILL UNDEFINED_T	; xFF

	.FILL PRIV_EXCEPTION	; x0100
	.FILL ILL_EXCEPTION	; x0101

	.BLKWTO x01FE
USER_START_ADDR
	.FILL 0				; x01FE: user app start address
	.FILL STARTUP		; x01FF: kernel start address

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
	LEA R0, BANNER_STR
	LEA R6, BANNER_STACK
	PUTS
	;; Checking the user program
	LD R0, USER_START_ADDR
	BRz DONE

	LEA R0, STARTING_STR
	PUTS
	LD R0, USER_START_ADDR
	JSRR R0

DONE:
	HALT

BANNER_STR
	.STRINGZ "LittleOS v0.1\nWritten by Anthony Liguori <aliguori@cs.utexas.edu>\n"
STARTING_STR
	.STRINGZ "Jumping to user program\n"
BANNER_STACK_START
	.BLKW 7
BANNER_STACK .FILL x0000

	;; -------------------------------------------------------------------
	;;                           TRAP ROUTINES
	;; -------------------------------------------------------------------

	;;
	;; Undefined TRAP routine
	;; 
	;; Informs the user about the error
	;; 
UNDEFINED_T
	ST R0, UND_R0_save		; callee saved registers
	ST R7, UND_R7_save
	
	LEA R0, PRIV_MESSAGE	; display a warning
	PUTS

	LD R0, UND_R0_save		; callee saved registers
	LD R7, UND_R7_save
	RET

UND_R0_save .BLKW 1
UND_R7_save .BLKW 1
UNDEFINED_MESSAGE
	.STRINGZ	"\n** ERROR: undefined trap executed **\n"

	
	;;
	;; GETC TRAP ROUTINE
	;;
	;; Gets a single character from the input device.
	;; 
	;; [out] R0 <= The character read from the device in R0[7:0]
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
	;; 
OUT_T
	ST R1, OUT_R1_save		; callee saved registers

OUT_LOOP
	LDI R1, DSR
	BRzp OUT_LOOP		; loop until MSB(DSR) == 1
	STI R0, DDR		; write to DDR

	LD R1, OUT_R1_save		; callee saved registers
	RET

DSR	.FILL xFE04
DDR	.FILL xFE06

OUT_R1_save .BLKW 1

	;;
	;; PUTS TRAP ROUTINE
	;;
	;; Displays a wide-character string to the display device
	;; 
	;; [in] R0 <= The address of the beginning of the string to display
	;; 
PUTS_T
	ST R0, PUTS_R0_save		; callee saved registers
	ST R1, PUTS_R1_save
	ST R7, PUTS_R7_save

	ADD R1, R0, 0		; R1 <= R0 (we need to use R0 for OUT call)
	BR 1
PUTS_LOOP
	OUT
	ADD R1, R1, 1
	LDR R0, R1, -1		; loop until null terminator
	BRnp PUTS_LOOP

	LD R0, PUTS_R0_save		; callee saved registers
	LD R1, PUTS_R1_save
	LD R7, PUTS_R7_save
	RET

PUTS_R0_save .BLKW 1
PUTS_R1_save .BLKW 1
PUTS_R7_save .BLKW 1

	;;
	;; IN TRAP ROUTINE
	;;
	;; Displays a prompt to the display device and gets a single character.
	;; 
	;; [out] R0 <= The character read from the display device in R0[7:0]
	;; 
IN_T
	ST R1, IN_R1_save		; callee saved registers
	ST R7, IN_R7_save

        LEA R0, IN_PROMPT       ; display prompt
        PUTS

        GETC                    ; input character and display
        OUT

        ADD R1, R0, 0           ; temporarily save character in R1

        AND R0, R0, 0           ; output newline
        ADD R0, R0, x0A
        OUT

        ADD R0, R1, 0           ; restore character to R0

	LD R1, IN_R1_save		; callee saved registers
	LD R7, IN_R7_save
        RET

IN_PROMPT
        .STRINGZ        "Enter a character: "
IN_R0_save .BLKW 1
IN_R1_save .BLKW 1
IN_R7_save .BLKW 1

	;;
	;; PUTSP TRAP ROUTINE
	;;
	;; Display a byte-packed string to the display device.
	;; 
	;; [in] R0 <= The address of the first word of the byte packed string
	;; 
PUTSP_T
	ST R7, PUTSP_R7_save		; callee saved registers

	LEA R0, PUTSP_MESSAGE
	PUTS

	LD R7, PUTSP_R7_save		; callee saved registers
	RET

PUTSP_MESSAGE
	.STRINGZ	"\n** ERROR: TRAP x24 (PUTSP) is not supported **\n"
PUTSP_R7_save .BLKW 1

	;;
	;; HALT TRAP ROUTINE
	;;
	;; Halts the machine.
	;; 
HALT_T
	ST R0, HALT_R0_save		; callee saved registers
	ST R1, HALT_R1_save
	
	LD R0, MASK
	LDI R1, MCR
	AND R0, R1, R0		; load MCR and set MSB[MCR] to 0

	STI R0, MCR		; halt the machine

	; Return if user asked to continue (whithout changing the PC)

	LD R0, HALT_R0_save		; callee saved registers
	LD R1, HALT_R1_save
	RET

MCR	.FILL xFFFE
MASK	.FILL x7FFF
HALT_R0_save .BLKW 1
HALT_R1_save .BLKW 1

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
	ST R0, PRIV_R0_save		; callee saved registers
	ST R7, PRIV_R7_save
	
	LEA R0, PRIV_MESSAGE	; display a warning
	PUTS

	LD R0, PRIV_R0_save		; callee saved registers
	LD R7, PRIV_R7_save
	RTI

PRIV_MESSAGE
	.STRINGZ	"\n** EXCEPTION: RTI instruction in user mode **\n"
PRIV_R0_save .BLKW 1
PRIV_R7_save .BLKW 1

	;;
	;; ILLEGAL INSTRUCTION EXCEPTION
	;; 
	;; This exception is invoked if the reserved opcode is invoked.
	;; 
ILL_EXCEPTION
	ST R0, ILL_R0_save		; callee saved registers
	ST R7, ILL_R7_save
	
	LEA R0, ILL_MESSAGE	; display a warning
	PUTS

	LD R0, ILL_R0_save		; callee saved registers
	LD R7, ILL_R7_save
	RTI

ILL_MESSAGE
	.STRINGZ	"\n** EXCEPTION: Illegal instruction **\n"
ILL_R0_save .BLKW 1
ILL_R7_save .BLKW 1


	.END
