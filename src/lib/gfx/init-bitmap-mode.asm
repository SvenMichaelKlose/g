init_bitmap_mode:
    ; Fill color RAM.
    lda #0
    tax
l:  sta colors,x
    sta @(+ 256 colors),x
    dex
    bne -l

    ; Make bitmap columns on screen.
    lda #<screen
    sta d
    lda #>screen
    sta @(++ d)
    ldx #screen_rows
    lda #@(+ 16 (* (-- screen_columns) screen_rows))

m:  pha

    ; Fill character row.
    ldy #@(-- screen_columns)
l:  sta (d),y
    sec
    sbc #screen_rows
    dey
    bpl -l

    lda d
    clc
    adc #screen_columns
    sta d
    bcc +n
    inc @(++ d)
n:

    pla
    clc
    adc #1
    dex
    bne -m

    ; Initialise VIC.
    ldx $ede4
    inx
    inx
    stx $9000
    ldx $ede5
    dex
    stx $9001
    lda #@screen_columns
    sta $9002
    lda #@(+ (* 2 screen_rows) 1)
    sta $9003
    lda #$cc    ; screen=$1e00, chars=$1000
    sta $9005
    rts