.section .init
.global _start
_start:
ldr r0, =0x3F200000 @ r0, GPIO base
mov r2, #1
lsl r2, #27
str r2, [r0, #8] @ set output mode
lsl r2, #2 @ r2, Open switch value
ldr r1, =0x3F003000 @ r1, 1MHZ counter base
ldr r4, =0x000F4240 @ r4, 1 second
loop3:
str r2, [r0, #28] @ enable LED
ldr r3, [r1, #4] 
add r3, r3, r4 @ 1 second after
loop1:@ poll until 1 second pass
ldr r5, [r1, #4]
cmp r3, r5
bhs loop1
str r2, [r0, #40] @ disable LED
ldr r3, [r1, #4] 
add r3, r3, r4 @ 1 second after
loop2:@ poll until 1 second pass
ldr r5, [r1, #4]
cmp r3, r5
bhs loop2
b loop3