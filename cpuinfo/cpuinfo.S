# save_restore.s
# 暂且就先保存sp,bp,r12~r15这几个寄存器，不够可以再添。
# as --64 -o save_restore.o save_restore.s
.global cpuinfo_rbx, cpuinfo_rcx, cpuinfo_rdx
cpuinfo_rbx:
    movq $0, %rax
    cpuid
    movq %rbx, %rax
    retq

cpuinfo_rcx:
    movq $0, %rax
    cpuid
    movq %rcx, %rax
    retq

cpuinfo_rdx:
    movq $0, %rax
    cpuid
    movq %rdx, %rax
    retq

