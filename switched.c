/*  switched.c - a switched interpreter for a stack virtual machine.
    Copyright (c) 2015, 2016 Grigory Rechistov. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of interpreters-comparison nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#include "common.h"

static inline Instr_t fetch(const cpu_t *pcpu) {
    assert(pcpu);
    assert(pcpu->pc < PROGRAM_SIZE);
    return pcpu->pmem[pcpu->pc];
};

static inline Instr_t fetch_checked(cpu_t *pcpu) {
    if (!(pcpu->pc < PROGRAM_SIZE)) {
        printf("PC out of bounds\n");
        pcpu->state = Cpu_Break;
        return Instr_Break;
    }
    return fetch(pcpu);
}

static inline decode_t decode(Instr_t raw_instr, const cpu_t *pcpu) {
    assert(pcpu);
    decode_t result = {0};
    result.opcode = raw_instr;
    switch (raw_instr) {
    case Instr_Nop:
    case Instr_Halt:
    case Instr_Print:
    case Instr_Swap:
    case Instr_Dup:
    case Instr_Inc:
    case Instr_Add:
    case Instr_Sub:
    case Instr_Mul:
    case Instr_Rand:
    case Instr_Dec:
    case Instr_Drop:
    case Instr_Over:
    case Instr_Mod:
    /* Added instructions */
    case Instr_And:
    case Instr_Or:
    case Instr_Xor:
    case Instr_SHL:
    case Instr_SHR:
    case Instr_Rot:
    case Instr_SQRT:
    case Instr_Pick:
        result.length = 1;
        break;
    case Instr_Push:
    case Instr_JNE:
    case Instr_JE:
    case Instr_Jump:
        result.length = 2;
        if (!(pcpu->pc+1 < PROGRAM_SIZE)) {
            printf("PC+1 out of bounds\n");
            result.length = 1;
            result.opcode = Instr_Break;
            break;
        }
        result.immediate = (int32_t)pcpu->pmem[pcpu->pc+1];
        break;
    case Instr_Break:
    default: /* Undefined instructions equal to Break */
        result.length = 1;
        result.opcode = Instr_Break;
        break;
    }
    return result;
}

/*** Service routines ***/
#define BAIL_ON_ERROR() if (cpu.state != Cpu_Running) break;

static inline void push(cpu_t *pcpu, uint32_t v) {
    assert(pcpu);
    if (pcpu->sp >= STACK_CAPACITY-1) {
        printf("Stack overflow\n");
        pcpu->state = Cpu_Break;
        return;
    }
    pcpu->stack[++pcpu->sp] = v;
}

static inline uint32_t pop(cpu_t *pcpu) {
    assert(pcpu);
    if (pcpu->sp < 0) {
        printf("Stack underflow\n");
        pcpu->state = Cpu_Break;
        return 0;
    }
    return pcpu->stack[pcpu->sp--];
}

static inline uint32_t pick(cpu_t *pcpu, int32_t pos) {
    assert(pcpu);
    if (pcpu->sp - 1 < pos) {
        printf("Out of bound picking\n");
        pcpu->state = Cpu_Break;
        return 0;
    }
    return pcpu->stack[pcpu->sp - pos];
}

int main(int argc, char **argv) {
    uint64_t steplimit = parse_args(argc, argv);
    cpu_t cpu = init_cpu();

    while (cpu.state == Cpu_Running && cpu.steps < steplimit) {
        Instr_t raw_instr = fetch_checked(&cpu);
        BAIL_ON_ERROR();
        decode_t decoded = decode(raw_instr, &cpu);

        uint32_t tmp1 = 0, tmp2 = 0, tmp3 = 0;
        /* Execute - a big switch */
        switch(decoded.opcode) {
        case Instr_Nop:
            /* Do nothing */
            break;
        case Instr_Halt:
            cpu.state = Cpu_Halted;
            break;
        case Instr_Push:
            push(&cpu, decoded.immediate);
            break;
        case Instr_Print:
            tmp1 = pop(&cpu); BAIL_ON_ERROR();
            printf("[%d]\n", tmp1);
            break;
        case Instr_Swap:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1);
            push(&cpu, tmp2);
            break;
        case Instr_Dup:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1);
            push(&cpu, tmp1);
            break;
        case Instr_Over:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp2);
            push(&cpu, tmp1);
            push(&cpu, tmp2);
            break;
        case Instr_Inc:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1+1);
            break;
        case Instr_Add:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 + tmp2);
            break;
        case Instr_Sub:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 - tmp2);
            break;
        case Instr_Mod:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            if (tmp2 == 0) {
                cpu.state = Cpu_Break;
                break;
            }
            push(&cpu, tmp1 % tmp2);
            break;
        case Instr_Mul:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 * tmp2);
            break;
        case Instr_Rand:
            tmp1 = rand();
            push(&cpu, tmp1);
            break;
        case Instr_Dec:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1-1);
            break;
        case Instr_Drop:
            (void)pop(&cpu);
            break;
        case Instr_JE:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            if (tmp1 == 0)
                cpu.pc += decoded.immediate;
            break;
        case Instr_JNE:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            if (tmp1 != 0)
                cpu.pc += decoded.immediate;
            break;
        case Instr_Jump:
            cpu.pc += decoded.immediate;
            break;
        case Instr_And:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 & tmp2);
            break;
        case Instr_Or:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 | tmp2);
            break;
        case Instr_Xor:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 ^ tmp2);
            break;
        case Instr_SHL:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 << tmp2);
            break;
        case Instr_SHR:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1 >> tmp2);
            break;
        case Instr_Rot:
            tmp1 = pop(&cpu);
            tmp2 = pop(&cpu);
            tmp3 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, tmp1);
            push(&cpu, tmp3);
            push(&cpu, tmp2);
            break;
        case Instr_SQRT:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, sqrt(tmp1));
            break;
        case Instr_Pick:
            tmp1 = pop(&cpu);
            BAIL_ON_ERROR();
            push(&cpu, pick(&cpu, tmp1));
            break;
        case Instr_Break:
            cpu.state = Cpu_Break;
            break;
        default:
            assert("Unreachable" && false);
            break;
        }
        cpu.pc += decoded.length; /* Advance PC */
        cpu.steps++;
    }

    assert(cpu.state != Cpu_Running || cpu.steps == steplimit);
    /* Print CPU state */
    printf("CPU executed %ld steps. End state \"%s\".\n",
            cpu.steps, cpu.state == Cpu_Halted? "Halted":
                       cpu.state == Cpu_Running? "Running": "Break");
    printf("PC = %#x, SP = %d\n", cpu.pc, cpu.sp);
    printf("Stack: ");
    for (int32_t i=cpu.sp; i >= 0 ; i--) {
        printf("%#10x ", cpu.stack[i]);
    }
    printf("%s\n", cpu.sp == -1? "(empty)": "");

    free(LoadedProgram);

    return cpu.state == Cpu_Halted ||
           (cpu.state == Cpu_Running &&
            cpu.steps == steplimit)?0:1;
}
