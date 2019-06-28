# save_restore.s
# 暂且就先保存sp,bp,r12~r15这几个寄存器，不够可以再添。
# as --64 -o save_restore.o save_restore.s
.global save, restore
save:
    leaq 8(%rsp), %rdx
    movq %rdx, (0)(%rdi)
    movq %rbp, (8)(%rdi)
    # 下面这个(%rsp)取当前rsp处的值，它是谁？它是call save时压栈的save函数返回地址啊！这是关键！
    # 返回地址保存到参数的第三个8字节处，即rip字段！修改它即修改save从restore返回的地址。
    movq (%rsp), %rdx
    movq %rdx, (16)(%rdi)
    movq %r12, (24)(%rdi)
    movq %r13, (32)(%rdi)
    movq %r14, (40)(%rdi)
    movq %r15, (48)(%rdi)
    movq $0, %rax   # 主动调用时，返回0，这个是跟setjmp学的。
    retq

restore:
    movq (0)(%rdi), %rsp
    movq (8)(%rdi), %rbp
    # 从参数第三个8字节处取出返回地址，压入到stack的rsp处，即栈顶，ret时将自动跳转到此处！
    movq (16)(%rdi), %rdx
    movq %rdx, (%rsp)
    movq (24)(%rdi), %r12
    movq (32)(%rdi), %r13
    movq (40)(%rdi), %r14
    movq (48)(%rdi), %r15
    movq $1, %rax   # 从restore返回时，返回1，这个写死了，就是1，不是别的。
    retq

