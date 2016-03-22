launch:
    ;;; Stop multitasking.
    jsr take_over

    ;;; Load the program.
    jsr load
    bcs +error
    txa     ; Save process info slot index.

    ;;; Save state for switching to it.
    ;;; The next task switch back to the current process will return from
    ;;; this system call.
    jsr save_process_state

    ;;; Initialise process info.
    ; Switch to master core.
    ldx #0
    sta $9ff4
    ldx program_start
    ldy @(++ program_start)
    pha
    jsr init_process

    ; Save process info slot index.
    lda #0
    sta $9ff4
    stx current_process

    ;;; Run the new process.
    pla
    jmp switch_to_process

error:
    ;;; Enable multitasking again and return.
    jsr release
    sec     ; Signal error.
    rts
