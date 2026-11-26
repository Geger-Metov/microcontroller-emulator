; button_led.hex is assembled by hand from this listing (no avr-gcc needed).
; A button between PD2 and ground (pull-up enabled) lights the LED on PB5.
        ldi   r16, 0x04
        out   PORTD, r16      ; pull-up on PD2
        sbi   DDRB, 5         ; PB5 is an output
loop:   sbic  PIND, 2         ; skip the next instruction if PD2 = 0 (pressed)
        rjmp  off
        sbi   PORTB, 5        ; pressed -> LED on
        rjmp  loop
off:    cbi   PORTB, 5        ; released -> LED off
        rjmp  loop
