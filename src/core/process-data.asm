    data
bank_ram:   0
bank1:      0
bank2:      0
bank3:      0
bank_io2:   0
bank_io3:   0
bank5:      0

banks:      %00111111
            fill @(-- (/ 1024 8))

saved_pc:           0 0
saved_a:            0
saved_x:            0
saved_y:            0
saved_flags:        0
saved_sp:           0
saved_stack:        fill 256
saved_zeropage:     fill $a0    ; BASIC part only.

    end