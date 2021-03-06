.export _main
.import _ultimem_erase_chip
.import _ultimem_burn_byte
.import pushax
.importzp s

.code

.proc _main
    lda #<txt_welcome
    ldy #>txt_welcome
    jsr $cb1e

    jsr _ultimem_erase_chip
    lda #<txt_chip_erased
    ldy #>txt_chip_erased
    jsr $cb1e

    lda #<txt_installing
    ldy #>txt_installing
    jsr $cb1e

    lda #2
    ldx #8
    ldy #2
    jsr $ffba   ; SETLF
    lda #fn_data_end-fn_data
    ldx #<fn_data
    ldy #>fn_data
    jsr $ffbd
    jsr $ffc0
    ldx #2
    jsr $ffc6

    lda #$00
    sta s
    lda #$a0
    sta s+1

l:  jsr $ffcf
    pha
    lda $90
    cmp #1
    pla
    bcc l

    pha
    lda s
    ldx s+1
    jsr pushax
    pla
    jsr _ultimem_burn_byte

    jsr $ffcc
    lda #2
    jsr $ffc3
    rts
.endproc

txt_welcome:
    .byte $93, "G INSTALLER", 13, 0

txt_installing:
    .byte $93, "INSTALLING G", 13, 0

txt_chip_erased:
    .byte "FLASH ROM ERASED.", 13,0

fn_data:
    .byte "GDATA.BIN,S,R"
fn_data_end:
