devcon_keyboard_vops:
    <devcon_read >devcon_read
    <devcon_error >devcon_error

devcon_screen_vops:
    <devcon_error >devcon_error
    <devcon_write_screen >devcon_write_screen

devcon_init:
    lda #$93            ; Clear screen.
    jsr chrout
    lda #%11110010      ; Up/locase chars.
    sta $9005
    lda #$00            ; Blink cursor.
    sta $cc             ; (BLNSW)

    lda #FILE_OPENED
    sta vfile_states
    sta @(++ vfile_states)
    sta @(+ 2 vfile_states)
    ldy #0
    sty vfile_handles
    iny
    sty @(++ vfile_handles)
    sty @(+ 2 vfile_handles)
    lda #CBMDEV_KEYBOARD
    sta devcon_logical_file_numbers
    lda #<devcon_keyboard_vops
    sta vfile_ops_l
    lda #>devcon_keyboard_vops
    sta vfile_ops_h
    lda #CBMDEV_SCREEN
    sta @(++ devcon_logical_file_numbers)
    sta @(+ 2 devcon_logical_file_numbers)
    lda #<devcon_screen_vops
    sta @(++ vfile_ops_l)
    sta @(+ 2 vfile_ops_l)
    lda #>devcon_screen_vops
    sta @(++ vfile_ops_h)
    sta @(+ 2 vfile_ops_h)

    ; Initialse stdin.
    lda #CBMDEV_KEYBOARD
    ldx #CBMDEV_KEYBOARD
    ldy #0              ; (read)
    jsr setlfs
    jsr open

    ; Initialse stdout and stderr.
    lda #CBMDEV_SCREEN
    ldx #CBMDEV_SCREEN  ; Screen
    ldy #1              ; (write)
    jsr setlfs
    jmp open

devcon_error:
    sec
    rts

; X: vfile index
devcon_read:
    jsr take_over
    lda vfile_handles,x
    tax
    lda devcon_logical_file_numbers,x
    tax
    jsr chkin
    jsr chrin
    jmp release

; X: vfile index
; A: character
devcon_write_screen:
    cmp #@(char-code #\A)
    bcc +n
    cmp #@(++ (char-code #\Z))
    bcs +n
    sbc #@(- (char-code #\A) (char-code #\a) 1)
    jmp +l

n:  cmp #@(char-code #\a)
    bcc +l
    cmp #@(++ (char-code #\z))
    bcs +l
    sbc #@(- (char-code #\a) (char-code #\A) 1)

devcon_write:
l:  jsr take_over

    pha
    lda vfile_handles,x
    tax
    lda devcon_logical_file_numbers,x
    tax
    jsr chkout
    pla
    jsr chrout

    jmp release
