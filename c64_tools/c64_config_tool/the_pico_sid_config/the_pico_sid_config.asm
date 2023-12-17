;//////////////////////////////////////////////////
;//                                              //
;// ThePicoSID                                   //
;// von Thorsten Kattanek                        //
;//                                              //
;// #file: the_pico_sid_config.asm               //
;//                                              //
;// https://github.com/ThKattanek/the_pico_sid   //
;//                                              //
;//////////////////////////////////////////////////

PICO_SID_REG = $d400+$1d

PRINT = $ab1e ; basic text output AC/YR Pointer to text
CLEAR_SCREEN = $e544
SET_CURSOR = $e50a ; CARRY = 0, XR=row /YR=coloumn
GETIN = $FFE4
CLEAR_SCREEN_LINE = $e9ff ; AX=row
PRINT_COLOR = $0286 ; Current Print Color

*=$0801

!byte $0b,$08,$e7,$07,$9e
!text "2064"

*=$0810
start:	
	; -----------------------------------------------------------------
	; Check of a ThePicoSid in the sic sockel 
	
	jsr exist_thepicosid
	cmp #$01
	bne not_found	; normal bne nur zum test beq
	jmp start_01
not_found:	
	lda #<error_txt_pico_not_found	
	ldy #>error_txt_pico_not_found	
	jsr PRINT	
	rts	
	
	; -----------------------------------------------------------------
	; ThePicoSid is ready
	
start_01:
	jsr CLEAR_SCREEN
	lda #$00
	sta $d020
	sta $d021
	
	lda #05
	sta PRINT_COLOR
	
	; -----------------------------------------------------------------
	; Firmware Version auslesen und anzeigen
	
	lda #$fd				; read the firmware version - major number
	jsr write_command
	jsr read_picosid
	clc
	adc #48
	sta version_txt + 18
	
	lda #$fe				; read the firmware version - minor number
	jsr write_command
	jsr read_picosid
	clc
	adc #48
	sta version_txt + 20
	
	lda #$ff				; read the firmware version - patch number
	jsr write_command
	jsr read_picosid
	clc
	adc #48
	sta version_txt + 22
	
	clc
	ldx #24
	ldy #08
	jsr SET_CURSOR
	
	lda #<version_txt
	ldy #>version_txt
	jsr PRINT
	
	; -----------------------------------------------------------------
	; print title and author
	
	clc
	ldx #02
	ldy #05
	jsr SET_CURSOR
	
	lda #<title_txt
	ldy #>title_txt
	jsr PRINT
	
	clc
	ldx #03
	ldy #06
	jsr SET_CURSOR
	
	lda #<author_txt
	ldy #>author_txt
	jsr PRINT
	
	; -----------------------------------------------------------------
	; draw menu

	lda #<menu
	ldy #>menu
	jsr draw_menu
	
	; -----------------------------------------------------------------
	
	lda #$02
	jsr write_command
	jsr read_picosid
	
key_wait:	
	jsr GETIN
	cmp #$00	
	beq key_wait
	
	cmp #$91
	beq key_up
	cmp #$11
	beq key_down
	cmp #$0D
	beq key_return
	cmp #$20
	beq key_return
	jmp key_wait
	
key_up:
	lda #<menu
	ldy #>menu
	jsr menu_control_up
	jmp new_draw_menu
key_down:
	lda #<menu
	ldy #>menu
	jsr menu_control_down
	jmp new_draw_menu
key_return:
	lda #<menu
	ldy #>menu
	jsr menu_control_action
	bcs ende
	;jmp key_wait
new_draw_menu:	
	lda #<menu
	ldy #>menu
	jsr draw_menu

	jmp key_wait
	
ende:
	jsr $ff81
	rts

; #################################
; Set the color for a complet row
; AC = color ($00 - $0f)
; YR = Row

set_row_color:
	
	pha 
	lda #$d8
	sta $04
	
	lda #$00
set_row_color_01:	
	clc
	adc #40
	bcc set_row_color_02
	inc $04
set_row_color_02:
	dey
	bne set_row_color_01
	sta $03
	pla
	
	ldy #40
set_row_color_03:
	sta ($03),y
	dey 
	bne set_row_color_03
	
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
	
; ##################################	
; Draw a menu entry
; AC = Color
; YX = Row
; $03-$04 = Pointer to Menu Entry
draw_menu_entry:
	sta PRINT_COLOR			; set color
	tya	
	pha
	tax
	;jsr CLEAR_SCREEN_LINE	; clear line
	pla
	tax
	ldy #$00				; get x pos
	lda ($03),y
	tay
	clc
	jsr SET_CURSOR			;set cursor position
	
	ldy #$01				; get text pointer
	lda ($03),y
	tax
	iny
	lda ($03),y
	tay
	txa
	jsr PRINT
		
	ldy #$03				; value count	
	lda ($03),y	
	beq draw_menu_entry_00	
		
	iny						; get current selected value
	lda ($03),y
	
	asl						; mul 2
	sta $02
	clc
	lda #$05
	adc $02					
	tay						; in yr is now the position of the pointer from the current value
	
	lda ($03),y
	tax
	iny
	lda ($03),y
	tay
	txa
	jsr PRINT
	
draw_menu_entry_00:
	
	rts
	
; ##################################	
; Draw the menu
; AC = lobyte from pointer to the menu
; YR = hibyte from pointer to the menu
draw_menu:
	sta $05
	sty $06
	
	ldy #$00		; load entry count 
	lda ($05),y
	sta $02
	
	iny				; load current selected entry
	lda ($05),y
	sta $b4
	
	iny				; load start y-pos
	lda ($05),y
	sta $b5
	
	iny				; load start y-pos
	lda ($05),y
	sta $bd
	
	ldx #$00
draw_menu_00:
	txa
	pha
	
	lda $bd			
	cpx	$b4			; set draw color
	bne color_01 
	lsr
	lsr
	lsr
	lsr
color_01
	sta $b6
	
	txa
	asl			; mul 2	/ every two rows
	clc
	adc	#$04 	; start pointer to menu entrys
	tay
	lda ($05),y
	sta $03
	iny
	lda ($05),y
	sta $04
	
	txa
	asl
	clc
	adc $b5
	tay
	
	lda $02
	pha
	
	lda $b6
	jsr draw_menu_entry
	
	pla
	sta $02
	
	pla
	tax
	inx
	cpx $02
	bne draw_menu_00
	
	rts
	
; ##################################	
; menu control up	
; AC = lobyte from pointer to the menu
; YR = hibyte from pointer to the menu

menu_control_up:
	sta $05
	sty $06
	
	ldy #$00		; load entry count 
	lda ($05),y
	sta $02
	
	iny				; load current selected entry
	lda ($05),y
	
	cmp #$00
	beq menu_control_up_end
	
	tax
	dex
	txa
	sta ($05),y
	
menu_control_up_end:
	rts

; ##################################	
; menu control down	
; AC = lobyte from pointer to the menu
; YR = hibyte from pointer to the menu

menu_control_down:
	sta $05
	sty $06
	
	ldy #$00		; load entry count 
	lda ($05),y
	sta $02
	dec $02
	
	iny				; load current selected entry
	lda ($05),y
	
	cmp $02
	beq menu_control_down_end
	
	tax
	inx
	txa
	sta ($05),y
	
menu_control_down_end:
	rts

; ##################################	
; menu control change value	
; AC = lobyte from pointer to the menu
; YR = hibyte from pointer to the menu

menu_control_action:
	sta $05
	sty $06

	ldy #$01		; load current entry 
	lda ($05),y
	asl
	clc
	adc #04
	tay
	
	lda ($05),y		; set menu_entry
	sta $03
	iny
	lda ($05),y
	sta $04
	
	ldy #$03		; load current values
	lda ($03),y
	bne menu_control_action_00
	sec
	rts

menu_control_action_00:	

	; AC = current entry
	tax
	dex
	stx $02
	iny
	lda ($03),y		; current value
	
	cmp $02
	beq menu_control_action_01
	
	tax
	inx
	jmp menu_control_action_02
	
menu_control_action_01:		

	ldx #$00
	
menu_control_action_02:	
	
	txa
	sta ($03),y
	
	clc
	rts

code_str:
!text "THEPICOSID"

error_txt_pico_not_found:
!text "KEIN THE PICO SID GEFUNDEN!",13, 0
version_txt:
!text "FIRMWARE VERSION: 0.1.0",0	
title_txt:
!text "THE-PICO-SID CONFIG TOOL  V1.0",0 	
author_txt:
!text "BY THORSTEN KATTANEK (C)2023",0			

menu:			
!8 5				; entry count
!8 0				; current selected entry
!8 8				; y start
!8 $1c				; color lo-nibble normal color / hi-nibble selected color
; all pointers to menu_entrys
!16	menu_entry_00, menu_entry_01, menu_entry_02, menu_entry_03, menu_entry_04	

menu_entry_00:
!8 10 						; x-start
!16 menu_00					; entry text
!8 2						; 2 values (0+1)
!8 0						; current selected value
!16 model_6581, model_8580

menu_entry_01:
!8 9 						; x-start
!16 menu_01					; entry text
!8 2						; 2 values (0+1)
!8 0						; current selected value
!16 no, yes

menu_entry_02:
!8 8 						; x-start

!16 menu_02					; entry text
!8 2						; 2 values (0+1)
!8 0						; current selected value
!16 no, yes

menu_entry_03:
!8 11 						; x-start
!16 menu_03					; entry text
!8 2						; 2 values (0+1)
!8 0						; current selected value
!16 no, yes

menu_entry_04:
!8 18 						; x-start
!16 menu_04					; entry text
!8 0						; 2 values (0+1)
!8 0						; current selected value

menu_00:
!text "SID MODEL: ",0				

menu_01:
!text "FILTER EMULATION: ",0				

menu_02:
!text "EXT FILTER EMULATION: ",0			

menu_03:
!text "8580 DIGIBOOST: ",0					

menu_04:
!text "EXIT",0					

model_6581:
!text "MOS-6581",0
model_8580:
!text "MOS-8580",0

yes:
!text "YES",0
no:
!text "NO ",0