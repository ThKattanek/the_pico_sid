PICO_SID_REG = $d400+29
PRINT = $ab1e ; basic textausgabe AC/YR Pointer zum Text

*=$0801

!byte $0b,$08,$e7,$07,$9e
!text "2064"

*=$0810
start:	
	jsr exist_thepicosid
	cmp #$01
	bne not_found
	jmp start_01
not_found:	
	lda #<error_txt_pico_not_found	
	ldy #>error_txt_pico_not_found	
	jsr PRINT	
	rts	
start_01:
	lda #$00
	sta $d020
	sta $d021
	
	lda #$fd				; read the firmware version - major number
	jsr write_command
	jsr read_picosid
	clc
	adc #48
	sta version_txt + 12
	
	lda #$fe				; read the firmware version - minor number
	jsr write_command
	jsr read_picosid
	clc
	adc #48
	sta version_txt + 14
	
	lda #$ff				; read the firmware version - patch number
	jsr write_command
	jsr read_picosid
	clc
	adc #48
	sta version_txt + 16
	
	lda #<version_txt
	ldy #>version_txt
	jsr PRINT
	
ende:
	rts

; #################################
; Exist ThePicoSid 
; AC = Return 0=NotFound 1=Found

exist_thepicosid:
	lda #$00
	ldx #$74
	jsr write_command
	jsr read_picosid
	
	eor #$88
	cmp #$74
	beq exist
	; not_exist
	lda #$00
	rts
exist:
	lda #$01
	rts

; ##################################
; Write Command to ThePicoSid
; AC = ThePicoSid Command
; XR = ThePicoSid Value

write_command:
	pha
	ldy #$0
write_command_01:
	lda code_str,y
	sta PICO_SID_REG
	iny
	cpy #10
	bne write_command_01
	
	pla
	sta PICO_SID_REG
	txa	
	sta PICO_SID_REG
	
	rts

; ##################################	
; Read from ThePicoSid
read_picosid:
	lda PICO_SID_REG
	rts
	
code_str:
!text "THEPICOSID"

error_txt_pico_not_found:
!text "KEIN THE PICO SID GEFUNDEN!",13, 0

version_txt:
!text "FW VERSION: 0.0.0",13,0