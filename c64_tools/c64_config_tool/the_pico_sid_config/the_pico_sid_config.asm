*=$0801

!byte $0b,$08,$e7,$07,$9e
!text "2064"

*=$0810
start:
	lda #$00
	sta $d020
	sta $d021
	jsr write_cfg
	rts

write_cfg:
	ldx #$00
write_cfg_01:
	lda code_str,x
	sta $d400+29
	inx
	cpx #10
	bne write_cfg_01
	rts

code_str:
!text "THEPICOSID"