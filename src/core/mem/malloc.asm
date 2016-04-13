; Allocator for chunks in a single bank which is in block 1.
;
; This basically uses a doubly–linked list.  Unlike freeing
; chunks, allocating them can get very slow – and that will
; become a problem with g, so you're welcome to update this.

malloc_chunk_header_size = 6

malloc_init:
    lda #$00
    sta d
    sta c
    lda #$20
    sta @(++ d)
    sta @(++ c)
    jsr clrram
    lda #$20
    sta $2001
    rts

; Returns:
; s: Allocated block.
malloc:
    ;; Point to first and previous record.
    lda #$00
    sta s
    sta tmp3
    sta tmp4
    lda #$20
    sta @(++ s)

    ;; Check if record is unallocated.
l:  ldy #1
    lda (s),y
    bpl +f

    ;; Save record as previous one.
    lda s
    sta tmp3
    lda @(++ s)
    sta tmp4

    ;; Get next record.
n:  ldy #3
    lda (s),y
    tax
    iny
    lda (s),y
    stx s
    sta @(++ s)

    ;; Check if there're no more records.
    ora s
    bne -l

    lda #ENOMEM
    jmp return_error

    ;; Check if record is large enough.
f:  ldy #1
    lda (s),y
    cmp @(++ c)
    beq +l
    bcc -n
l:  dey
    lda (s),y
    cmp c
    beq +l
    bcc -n
l:

    ;; Split up record.
    ldy #0
    lda c
    sec
    sbc (s),y
    sta tmp
    iny
    lda @(++ c)
    sbc (c),y
    sta tmp2
    bne +split
    lda tmp
    cmp #7
    bcs +split

    ;; Don't split record – it's too small.
l:  ldy #0
    lda (s),y
    ora #128
    sta (s),y

    ;; Make pointer to data part of chunk.
    lda s
    clc
    adc #malloc_chunk_header
    sta s
    bcc +l
    inc @(++ s)
    clc
l:  rts

split:
    ;; Make pointer to new record following.
    lda s
    clc
    adc c
    sta d
    lda @(++ s)
    adc @(++ c)
    sta @(++ d)

    ;; Link new record back.
    lda s
    ldy #2
    sta (d),y
    iny
    lda @(++ s)
    sta (d),y

    ;; Link new record to next.
    ldy #4
    lda (s),y
    sta (d),y
    iny
    lda (s),y
    sta (d),y

    ;; Link to new record.
    lda d
    ldy #4
    sta (s),y
    iny
    lda @(++ d)
    sta (s),y

    ;; Set size of new record.
    lda tmp
    ldy #0
    sta (d),y
    iny
    lda tmp2
    sta (d),y

    ;; Correct size of allocated block.
    lda c
    ldy #0
    sta (s),y
    iny
    lda @(++ c)
    sta (s),y

    jmp -l

err_inval:
    lda #EINVAL
    jmp return_error
    
; s: Chunk to free.
malloc_free:
    ;; Make pointer to header of chunk.
    lda s
    sec
    sbc #malloc_chunk_header_size
    sta s
    bcs +n
    dec @(++ s)
n:

    ;; Check if chunk has been allocated before.
    ldy #1
    lda (s),y
    bpl -err_inval

    ;; Mark chunk to free.
    and #%0111111
    sta (d),y

    ldy #2
    lda (s),y
    iny
    ora (s),y
    beq +n

    ;; Get back link.
    ldy #2
    lda (s),y
    tax
    iny
    lda (s),y
    stx s
    sta @(++ s)

    ;; Get link to next.
n:  ldy #4
    lda (s),y
    sta d
    iny
    lda (s),y
    sta @(++ s)

    ;; Check if it's allocated.
    ldy #1
    lda (d),y
    bmi +n

    ;; Merge chunks.
    ; Add up size.
    ldy #0
    lda (s),y
    clc
    adc (d),y
    sta (s),y
    iny
    lda (s),y
    adc (d),y
    sta (s),y

    ; Make link to next.
    ldy #4
    lda (d),y
    sta (d),y
    iny
    lda (d),y
    sta (d),y
    jmp -n

n:  clc
    rts
