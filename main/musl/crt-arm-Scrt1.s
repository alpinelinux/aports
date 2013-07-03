.weak _init
.weak _fini
.text
.global _start
_start:
        mov fp,#0
        mov lr,#0

        pop { a2 }
        mov a3, sp
        push { a3 }
        push { a1 }

        ldr sl, .L_GOT
        adr a4, .L_GOT
        add sl, sl, a4
        ldr ip, .L_GOT+4
        ldr ip, [sl, ip]
        push { ip }
        ldr a4, .L_GOT+8
        ldr a4, [sl, a4]
        ldr a1, .L_GOT+12
        ldr a1, [sl, a1]

        bl __libc_start_main(PLT)
1:      b 1b

        .align 2
.L_GOT:
        .word _GLOBAL_OFFSET_TABLE_ - .L_GOT
        .word _fini(GOT)
        .word _init(GOT)
        .word main(GOT)

