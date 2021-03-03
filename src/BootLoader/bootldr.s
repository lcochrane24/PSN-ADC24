.ifdef __30F3014
		.include "p30f3014.inc"
.else
       	.include "p30f4013.inc"
.endif

;******************************************************************************
; Configuration bits:
;******************************************************************************

    config __FOSC, CSW_FSCM_OFF & XT_PLL8
    config __FWDT, WDT_OFF
	config __FBORPOR, PBOR_ON & BORV_42 & PWRT_64 & MCLR_EN
    config __FGS, CODE_PROT_OFF
	
;******************************************************************************
; Program Specific Constants (literals used in code)
;******************************************************************************
	.equ CRC, W4
    .equ ACK, 0x55
    .equ NACK, 0xFF
    .equ USER_ADDRESS, 0x0100
    .equ START_ADDRESS, 0x3D00		; Relative to 0x0100
    .equ CFG_M, 0xF8
    .equ EE_M, 0x7F

    .equ C_READ, 0x01
    .equ C_WRITE, 0x02
    .equ C_VERSION, 0x03
    .equ C_USER, 0x0F
    .equ MAX_WORD_ROW, 64

    .equ MAJOR_VERSION, 0x01
    .equ MINOR_VERSION, 0x02

;******************************************************************************
; Global Declarations:
;******************************************************************************
    .global __reset				  ;The label for the first line of code.
    .global recBuf

;******************************************************************************
;Uninitialized variables in X-space in data memory
;******************************************************************************
	.section bss, xmemory
recBuf:   .space 2 * MAX_WORD_ROW

;******************************************************************************
;Code Section in Program Memory
;******************************************************************************
        .text                     ; Start of Code section
        .org #START_ADDRESS
__reset:
        mov #__SP_init, W15       ; Initialize the Stack Pointer
        mov #__SPLIM_init, W0     ; Initialize the Stack Pointer Limit Register
        mov W0, SPLIM
        nop                       ; Add NOP to follow SPLIM initialization
		
		mov #0xffff, W0			  ; Init ports
		mov W0, ADPCFG
		mov #0x0000, W0
		mov W0, TRISA
		mov #0x1e0f, W0			  ; pin B12 = USB reset
		mov W0, TRISB
		mov #0x4000, W0
		mov W0, TRISC
		mov #0x0100, W0
		mov W0, TRISD
		mov #0x0010, W0
		mov W0, TRISF
		mov #0x1fff, W0
		mov W0, PORTB
		mov #0x0001, W0
		mov W0, PORTD

		bset LATD, #2			  ; Turn LED On	
		
        ; Uart init
		mov #0x8420, W0           ; W0 = 0x8420 -> 1000 0100 0010 0000b
        mov W0, U1MODE            ; Enable UART with Alternate IO, AutoBaud and 8N1
        clr U1STA

        ; Timer 3 init
        clr T3CON                 ; Stops any 16-bit Timer3 operation
        bclr IEC0, #T3IE          ; Disable Timer 3 interrupt
        setm PR3                  ; Set Timer 3 period to maximum value 0xFFFF
        mov #0x8000, W0           ; Start Timer 3 with 1:1 prescaler and clock source set to internal cycle
        mov W0, T3CON

        ; Input Capture init
        clr IC1CON                ; Turn off Input Capture 1 module
        bset IC1CON, #1           ; Input Capture Mode every risind edge
        bclr IFS0, #IC1IF         ; Clear Input Capture flag
        bclr IEC0, #IC1IE         ; Disable Input Capture interrupts

        ; Start Autobaud detection
        mov #0x0004, W0           ; W0 = 0x0004
        rcall WaitRising          ; Wait until the first Rising edge is detected
        clr TMR3                  ; Clear content of the Timer 3 timer register
ByteLoop:
        rcall WaitRising
        dec W0, W0                ; W0--
        bra NZ, ByteLoop          ; if W0 != 0 jump to ByteLoop
        bclr T3CON, #TON          ; Last Rising edge detected so Stop Timer 3
        mov TMR3, W0              ; W0 = TMR3
        add #0x40, W0             ; For rounding: +64 >> 7 is equal to +0.5
        asr W0, #7, W0            ; W0 = ((Tend - Tini + 64) / 128)
        dec W0, W0                ; W0--

        ; Uart re-init
        mov W0, U1BRG             ; U1BRG = W0 -> Configs UART with the detected baudrate
        bclr U1MODE, #ABAUD       ; Disable AutoBaud
        bset U1STA, #UTXEN        ; Enable transmition
        bra SendAck

StartIn:
		; Setup timeer 
        mov #0x7a, W2             ; W2 = 0x007f
        mov #0xffff, W1           ; W1 = 0xffff
		
StartFrame:
        dec W1, W1                ; W1--
        bra NZ, TestChar          ; if W1 != 0 jump to StartLoop
        dec W2, W2                ; W2--
        bra NZ, TestChar          ; if W2 != 0 jump to StartLoop
	    goto USER_ADDRESS		  ; goto user code

TestChar:
        btss U1STA, #URXDA        ; Wait until a character is received
        bra StartFrame
        mov U1RXREG, W0
        cp.b W0, #C_USER          ; Compare received Character with USER character
        btsc SR, #Z
        goto USER_ADDRESS
        cp.b W0, #C_READ          ; Compare received Character with READ character
        bra Z, ReadMemCmd
        cp.b W0, #C_WRITE         ; Compare received Character with WRITE character
        bra Z, WriteMemCmd
        cp.b W0, #C_VERSION       ; Compare received Character with VERSION character
        bra Z, VersionCmd
        bra StartFrame

VersionCmd:
        mov #MAJOR_VERSION, W0    ; Send Major Version
        mov W0, U1TXREG
        mov #MINOR_VERSION, W0    ; Send Minor Version
        mov W0, U1TXREG
        bra SendAck
;
ReadMemCmd:
		rcall ReceiveChar	  	  ; Get count
 		mov W0, W3
        rcall ReceiveChar         ; Receive high byte of the address
        mov W0, TBLPAG            ; High address byte
        rcall ReceiveChar         ; Receive medium byte of the address
        swap W0
        rcall ReceiveChar         ; Receive low byte of the address
		mov W0, W2			
		clr W4					  ; CRC accum
ReadLoop:
        tblrdh [W0], W1           ; Read high word to W1
		rcall SendChr
	
        tblrdl [W0++], W1         ; Read low word to W1
        swap W1
		rcall SendChr
        swap W1
		rcall SendChr
        dec W3, W3
        bra nz, ReadLoop          ; Loop till done 
		mov W4, W1
		rcall SendChr
SendAck:
        mov #ACK, W1              ; Send an ACK character
		rcall SendChr
        bra StartIn
SendNack:
        mov #NACK, W0             ; Send a KO character
Send:
        mov W0, U1TXREG
        bra StartIn

WriteMemCmd:
        clr W4                    ; Reset W4 = Checkbyte
        rcall ReceiveChar         ; Receive high byte of the initial address
        mov W0, TBLPAG            ; For latch loading and programming
        mov W0, NVMADRU           ; For erase cycle - in program are written auto. from TBLPAG
        rcall ReceiveChar         ; Receive medium byte of the initial address
        mov.b WREG, NVMADR + 1
        rcall ReceiveChar         ; Receive low byte of the initial address
        mov.b WREG, NVMADR

        rcall ReceiveChar         ; Receive the number of bytes to be received
        mov W0, W3
        mov #recBuf, W2           ; W2 = recBuf
FillBufLoop:
        rcall ReceiveChar
        mov.b W0, [W2++]          ; Move received byte to recBuf
        dec W3, W3
        bra nz, FillBufLoop       ; Fill reception buffer

        cp0.b W4                  ; Check (INTEL HEX8 Checksum - Sum modulo 256)
        bra nz, SendNack          ; if Checkbyte != 0 jump to SendNack
        mov #recBuf, W2           ; W2 = recBuf
        mov NVMADR, W5            ; Use W5 as low word address

        mov #CFG_M, W0            ; Check if destination is Config Memory
        cp.b TBLPAG
        bra nz, noCFM

        mov #0x4008, W8           ; Assigns Write Config Row Code - Config Mem doesn't need to be erased
        mov #1, W3                ; Assigns Number of 16bits words per Row
        bra LoadLatch
noCFM:
        mov #EE_M, W0             ; Check if destination is EEPROM Memory
        cp.b TBLPAG
        bra NZ, noEEM
        mov #0x4075, W0           ; Assigns Erase EEPROM Row Code
        mov #0x4005, W8           ; Assigns Write EEPROM Row Code
        mov #32, W3               ; Assigns Number of 16bits word per Row
        bra StartWritingCycle     ; Erase and Write Memory
noEEM:
        mov #0x4071, W0           ; Assigns Erase PGM Row Code
        mov #0x4001, W8           ; Assigns Write PGM Row Code
        mov #64, W3               ; Assigns Number of 16bits word per Row (32instr - 64word16)

StartWritingCycle:
        rcall WriteKey            ; Erase selected Row
LoadLatch:
        tblwtl [W2++], [W5]       ; Load low word to latch
        dec W3, W3
        bra Z, EndLatch
        tblwth [W2++], [W5++]     ; Load high word to latch
        dec W3, W3                ; Repeat until whole row is loaded
        bra NZ, LoadLatch
EndLatch:
        mov W8, W0                ; Write selected Row
        rcall WriteKey
        bra SendAck               ; Send an ACK character

;******************************************************************************
;Procedures
;******************************************************************************
WaitRising:
        mov #0x5A, W2             ; W2 = 0x5A
MajorLRise:
        setm W1                   ; W1 = 0xFFFF
MinorLRise:
        btsc IFS0, #IC1IF         ; Rising edge detected?
        bra EndRising             ; Yes -> Jump to finish detection
        dec W1, W1                ; W1--
        bra NZ, MinorLRise        ; if W1 != 0 jump MinorLRise
        dec W2, W2                ; W2--
        bra NZ, MajorLRise        ; if W2 != 0 jump MajorLRise
        goto USER_ADDRESS         ; Timeout aprox. = 0x5A * 0xFFFF * 5 clocks -> Jump to user soft

EndRising:
        bclr IFS0, #IC1IF         ; Clear Interrupt Flag
        return

;******************************************************************************
ReceiveChar:
        mov #0xFFFF, W10          ; W10 = 0xFFFF
MajorLChar:
        setm W11                  ; W11 = 0xFFFF
MinorLChar:
        btsc U1STA, #URXDA        ; Character received ?
        bra EndReceiveChar        ; Yes -> Jump to Finish reception
        dec W11, W11              ; W1--
        bra NZ, MinorLChar        ; if W1 != 0 jump MinorLChar
        dec W10, W10              ; W2--
        bra NZ, MajorLChar        ; if W2 != 0 jump MajorLChar
        mov #__SP_init, W15       ; Initialize Stack Pointer
        bra SendNack              ; Timeout aprox. = 0xFFFF * 0xFFFF * 5 clocks -> Jump to Send Nack
EndReceiveChar:
        mov.b U1RXREG, WREG       ; W0 = U1RXREG
        add.b W4, W0, W4          ; Checkbyte += W0 -> Performs a Sum modulo 256 checksum (INTEL HEX8)
        return

;******************************************************************************
WriteKey:
        mov W0, NVMCON
        mov #0x55, W0
        mov W0, NVMKEY
        mov #0xAA, W0
        mov W0, NVMKEY
        bset NVMCON, #WR          ; Start Writing
        nop
        nop
WaitWriting:
        btsc NVMCON, #WR          ; WR or WREN - Wait until operation is finished
        bra WaitWriting
        return
;
SendChr:
        btsc U1STA, #UTXBF        ; Wait until a character is received
        bra SendChr
        mov W1, U1TXREG           ; Send W1 low byte
        add.b W4, W1, W4          ; Checkbyte += W0 -> Performs a Sum modulo 256 checksum (INTEL HEX8)
		return
;
;--------End of All Code Sections ---------------------------------------------

.end                              ; End of program code in this file
