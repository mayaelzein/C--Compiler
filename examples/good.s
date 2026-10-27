.text
.globl main
main:
 j _main
_f:
    la $sp, -8($sp)
    sw $fp, 4($sp)
    sw $ra, 0($sp)
    la $fp, 0($sp)
    la $sp, -8($sp)
    li $t0, 0
    sw $t0, -4($fp)
    lw $t0, 8($fp)
    lw $t1, -4($fp)
    ble $t0, $t1, __L1
    j __L3
__L1:
    li $t0, 0
    sw $t0, -8($fp)
    lw $v0, -8($fp)
    la $sp, 0($fp)
    lw $ra, 0($sp)
    lw $fp, 4($sp)
    la $sp, 8($sp)
    jr $ra
__L3:
    li $t0, 1
    sw $t0, 0($fp)
    li $t0, 1
    sw $t0, 0($fp)
    lw $t0, 8($fp)
    lw $t1, 0($fp)
    sub $t2, $t0, $t1
    sw $t2, 0($fp)
    lw $t0, 0($fp)
    la $sp, -4($sp)
    sw $t0, 0($sp)
    jal _f
    la $sp, 4($sp)
    move $t0, $v0
    sw $t0, 0($fp)
    lw $t0, 0($fp)
    lw $t1, 0($fp)
    add $t2, $t0, $t1
    sw $t2, 0($fp)
    lw $v0, 0($fp)
    la $sp, 0($fp)
    lw $ra, 0($sp)
    lw $fp, 4($sp)
    la $sp, 8($sp)
    jr $ra
    la $sp, 0($fp)
    lw $ra, 0($sp)
    lw $fp, 4($sp)
    la $sp, 8($sp)
    jr $ra
_main:
    la $sp, -8($sp)
    sw $fp, 4($sp)
    sw $ra, 0($sp)
    la $fp, 0($sp)
    la $sp, -16($sp)
    li $t0, 5
    sw $t0, -4($fp)
    lw $t0, -4($fp)
    la $sp, -4($sp)
    sw $t0, 0($sp)
    jal _f
    la $sp, 4($sp)
    move $t0, $v0
    sw $t0, -8($fp)
    lw $t0, -8($fp)
    la $sp, -4($sp)
    sw $t0, 0($sp)
    jal _println
    la $sp, 4($sp)
    li $t0, 0
    sw $t0, -16($fp)
    lw $v0, -16($fp)
    la $sp, 0($fp)
    lw $ra, 0($sp)
    lw $fp, 4($sp)
    la $sp, 8($sp)
    jr $ra
    la $sp, 0($fp)
    lw $ra, 0($sp)
    lw $fp, 4($sp)
    la $sp, 8($sp)
    jr $ra
.align 2
.data
_nl: .asciiz "\n"

.align 2
.text
_println:
    li $v0, 1
    lw $a0, 0($sp)
    syscall
    li $v0, 4
    la $a0, _nl
    syscall
    jr $ra

