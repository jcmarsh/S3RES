#define PRINT_REG(NAME, REG) printf("%s\t: %lX\t%lu\n", #NAME, REG->NAME, REG->NAME)

#if __WORDSIZE == 64

void printRegs(struct user_regs_struct * regs) {
  PRINT_REG(r15, regs);      //  0
  PRINT_REG(r14, regs);      //  1
  PRINT_REG(r13, regs);      //  2
  PRINT_REG(r12, regs);      //  3
  PRINT_REG(rbp, regs);      //  4
  PRINT_REG(rbx, regs);      //  5
  PRINT_REG(r11, regs);      //  6
  PRINT_REG(r10, regs);      //  7
  PRINT_REG(r9, regs);       //  8
  PRINT_REG(r8, regs);       //  9
  PRINT_REG(rax, regs);      // 10
  PRINT_REG(rcx, regs);      // 11
  PRINT_REG(rdx, regs);      // 12
  PRINT_REG(rsi, regs);      // 13
  PRINT_REG(rdi, regs);      // 14
  PRINT_REG(orig_rax, regs); // 15
  PRINT_REG(rip, regs);      // 16
  PRINT_REG(cs, regs);       // 17
  PRINT_REG(eflags, regs);   // 18
  PRINT_REG(rsp, regs);      // 19
  PRINT_REG(ss, regs);       // 20
  PRINT_REG(fs_base, regs);  // 21
  PRINT_REG(gs_base, regs);  // 22
  PRINT_REG(ds, regs);       // 23
  PRINT_REG(es, regs);       // 24
  PRINT_REG(fs, regs);       // 25
  PRINT_REG(gs, regs);       // 26
}

#else // 32 bit wordsize... x86

void printRegs(struct user_regs_struct * regs) {
  PRINT_REG(ebx, regs);      //  0
  PRINT_REG(ecx, regs);      //  1
  PRINT_REG(edx, regs);      //  2
  PRINT_REG(esi, regs);      //  3
  PRINT_REG(edi, regs);      //  4
  PRINT_REG(ebp, regs);      //  5
  PRINT_REG(eax, regs);      //  6
  PRINT_REG(xds, regs);      //  7
  PRINT_REG(xes, regs);      //  8
  PRINT_REG(xfs, regs);      //  9
  PRINT_REG(xgs, regs);      // 10
  PRINT_REG(orig_eax, regs); // 11
  PRINT_REG(eip, regs);      // 12
  PRINT_REG(xcs, regs);      // 13
  PRINT_REG(eflags, regs);   // 14
  PRINT_REG(esp, regs);      // 15
  PRINT_REG(xss, regs);      // 16
}

#endif // __WORDSIZE
