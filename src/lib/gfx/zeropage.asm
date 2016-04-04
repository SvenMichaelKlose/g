    data
    org zp_end_core

p:      0
ph:     0

sa:     0 0     ; Bytecode argument list.
sp:     0 0     ; Bytecode pointer.
srx:    0       ; Saved X register.

scr:    0 0     ; Current screen address.
xcpos:  0       ; X columns.


; Utils

font_compression: 0

;;;;;;;;;;;;;;;;;;;;;;;
;;; Drawing context ;;;
;;;;;;;;;;;;;;;;;;;;;;;

context_start:

; Visible region.
rxl:    0
ryt:    0
rxr:    0
ryb:    0

; Cursor
xpos:   0       ; X position
ypos:   0       ; Y position
xpos2:  0       ; X position
ypos2:  0       ; Y position
width:  0       ; Width
height: 0       ; Height

pattern: 0
patternh: 0

font:   0       ; Font starting page.
font_space_size: 0 ; Width of an empty character.
do_compress_font_gaps: 0

masks:  0       ; Source mask.
maskd:  0       ; Destination mask.

context_end:

context_size = @(- context_end context_start)

;;;;;;;;;;;;;;;;;;;;;;;;;
;;; Application space ;;;
;;;;;;;;;;;;;;;;;;;;;;;;;
    end