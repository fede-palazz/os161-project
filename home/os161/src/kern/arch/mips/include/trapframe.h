/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MIPS_TRAPFRAME_H_
#define _MIPS_TRAPFRAME_H_

/*
 * Structure describing what is saved on the stack during entry to
 * the exception handler.
 *
 * This must agree with the code in exception-*.S.
 */

struct trapframe {
    uint32_t tf_vaddr;  /* Virtual address that caused the exception (from coprocessor 0 vaddr register) */
    uint32_t tf_status; /* Processor status register (coprocessor 0 status register) */
    uint32_t tf_cause;  /* Cause of the exception (coprocessor 0 cause register) */
    uint32_t tf_lo;     /* Low-order result of multiplication/division */
    uint32_t tf_hi;     /* High-order result of multiplication/division */

    /* General-purpose registers (saved during the exception): */
    uint32_t tf_ra; /* Return address (register 31) */
    uint32_t tf_at; /* Assembler temporary register (register 1) */
    uint32_t tf_v0; /* Function return value or syscall return (register 2) */
    uint32_t tf_v1; /* Secondary return value (register 3) */

    /* Function argument registers (used to pass arguments to functions): */
    uint32_t tf_a0; /* First argument to a function or syscall (register 4) */
    uint32_t tf_a1; /* Second argument (register 5) */
    uint32_t tf_a2; /* Third argument (register 6) */
    uint32_t tf_a3; /* Fourth argument (register 7) */

    /* Temporary registers (not preserved across function calls): */
    uint32_t tf_t0; /* Temporary register 0 (register 8) */
    uint32_t tf_t1; /* Temporary register 1 (register 9) */
    uint32_t tf_t2; /* Temporary register 2 (register 10) */
    uint32_t tf_t3; /* Temporary register 3 (register 11) */
    uint32_t tf_t4; /* Temporary register 4 (register 12) */
    uint32_t tf_t5; /* Temporary register 5 (register 13) */
    uint32_t tf_t6; /* Temporary register 6 (register 14) */
    uint32_t tf_t7; /* Temporary register 7 (register 15) */

    /* Callee-saved registers (must be preserved across function calls): */
    uint32_t tf_s0; /* Saved register 0 (register 16) */
    uint32_t tf_s1; /* Saved register 1 (register 17) */
    uint32_t tf_s2; /* Saved register 2 (register 18) */
    uint32_t tf_s3; /* Saved register 3 (register 19) */
    uint32_t tf_s4; /* Saved register 4 (register 20) */
    uint32_t tf_s5; /* Saved register 5 (register 21) */
    uint32_t tf_s6; /* Saved register 6 (register 22) */
    uint32_t tf_s7; /* Saved register 7 (register 23) */

    /* Additional temporary registers: */
    uint32_t tf_t8; /* Temporary register 8 (register 24) */
    uint32_t tf_t9; /* Temporary register 9 (register 25) */

    /* Special purpose registers: */
    uint32_t tf_gp; /* Global pointer (register 28) */
    uint32_t tf_sp; /* Stack pointer (register 29) */
    uint32_t tf_s8; /* Saved register 8 or frame pointer (register 30) */

    uint32_t tf_epc; /* Exception program counter (address of the instruction causing the exception) */
};

/*
 * MIPS exception codes.
 */
#define EX_IRQ 0  /* Interrupt */
#define EX_MOD 1  /* TLB Modify (write to read-only page) */
#define EX_TLBL 2 /* TLB miss on load */
#define EX_TLBS 3 /* TLB miss on store */
#define EX_ADEL 4 /* Address error on load */
#define EX_ADES 5 /* Address error on store */
#define EX_IBE 6  /* Bus error on instruction fetch */
#define EX_DBE 7  /* Bus error on data load *or* store */
#define EX_SYS 8  /* Syscall */
#define EX_BP 9   /* Breakpoint */
#define EX_RI 10  /* Reserved (illegal) instruction */
#define EX_CPU 11 /* Coprocessor unusable */
#define EX_OVF 12 /* Arithmetic overflow */

/*
 * Function to enter user mode. Does not return. The trapframe must
 * be on the thread's own stack or bad things will happen.
 */
__DEAD void mips_usermode(struct trapframe *tf);

/*
 * Arrays used to load the kernel stack and curthread on trap entry.
 */
extern vaddr_t cpustacks[];
extern vaddr_t cputhreads[];

#endif /* _MIPS_TRAPFRAME_H_ */
