/*  asmopt.c - a tail recursion optimization interpreter
    for a stack virtual machine.
    Copyright (c) 2024 Glukhov Mikhail, based on work of
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

/* TODO:a global - not good. Should be moved into cpu state or somewhere else */
static uint64_t steplimit = LLONG_MAX;

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
        if (!(pcpu->pc+1 < PROGRAM_SIZE)) {
            printf("PC+1 out of bounds\n");
            result.length = 1;
            result.opcode = Instr_Break;
            break;
        }
        result.length = 2;
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

static inline decode_t fetch_decode(cpu_t *pcpu) {
    return decode(fetch_checked(pcpu), pcpu);
}

/*** Service routines ***/
#define BAIL_ON_ERROR() if (pcpu->state != Cpu_Running) return;

#define DISPATCH() service_routines[pdecoded->opcode](pcpu, pdecoded);

#define ADVANCE_PC() do {\
    pcpu->pc += pdecoded->length;\
    pcpu->steps++; \
    if (pcpu->state != Cpu_Running || pcpu->steps >= steplimit) return;\
} while(0);

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

typedef void (*service_routine_t)(cpu_t *pcpu, decode_t* pdecode);
service_routine_t service_routines[];

void sr_Nop(cpu_t *pcpu, decode_t *pdecoded) {
    /* Do nothing */
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Halt(cpu_t *pcpu, decode_t *pdecoded) {
    pcpu->state = Cpu_Halted;
    ADVANCE_PC();
    return;
}

void sr_Push(cpu_t *pcpu, decode_t *pdecoded) {
    push(pcpu, pdecoded->immediate);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Print(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    printf("[%d]\n", tmp1);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Swap(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Dup(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1);
    push(pcpu, tmp1);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Over(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp2);
    push(pcpu, tmp1);
    push(pcpu, tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Inc(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1+1);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Add(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 + tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Sub(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 - tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Mod(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    if (tmp2 == 0) {
        pcpu->state = Cpu_Break;
        return;
    }
    push(pcpu, tmp1 % tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Mul(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 * tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Rand(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = rand();
    push(pcpu, tmp1);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Dec(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1-1);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Drop(cpu_t *pcpu, decode_t *pdecoded) {
    (void)pop(pcpu);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Je(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    if (tmp1 == 0)
        pcpu->pc += pdecoded->immediate;
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Jne(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    if (tmp1 != 0)
        pcpu->pc += pdecoded->immediate;
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Jump(cpu_t *pcpu, decode_t *pdecoded) {
    pcpu->pc += pdecoded->immediate;
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_And(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 & tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Or(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 | tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Xor(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 ^ tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_SHL(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 << tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_SHR(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1 >> tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Rot(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    uint32_t tmp2 = pop(pcpu);
    uint32_t tmp3 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, tmp1);
    push(pcpu, tmp3);
    push(pcpu, tmp2);
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_SQRT(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, sqrt(tmp1));
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Pick(cpu_t *pcpu, decode_t *pdecoded) {
    uint32_t tmp1 = pop(pcpu);
    BAIL_ON_ERROR();
    push(pcpu, pick(pcpu, tmp1));
    ADVANCE_PC();
    *pdecoded = fetch_decode(pcpu);
    DISPATCH();
}

void sr_Break(cpu_t *pcpu, decode_t *pdecoded) {
    pcpu->state = Cpu_Break;
    ADVANCE_PC();
    /* No need to dispatch after Break */
    return;
}


extern void srv_Halt(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Break(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Nop(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Push(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Drop(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Dup(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Swap(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Over(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Sub(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Inc(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Mod(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Jump(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Je(cpu_t *pcpu, decode_t *pdecoded);
extern void srv_Print(cpu_t *pcpu, decode_t *pdecoded);

service_routine_t service_routines[] = {
        &srv_Break, &srv_Nop, &srv_Halt, &srv_Push, &srv_Print,
        &sr_Jne, &srv_Swap, &srv_Dup, &srv_Je, &srv_Inc,
        &sr_Add, &srv_Sub, &sr_Mul, &sr_Rand, &sr_Dec,
        &srv_Drop, &srv_Over, &srv_Mod, &srv_Jump,
        &sr_And, &sr_Or, &sr_Xor,
        &sr_SHL, &sr_SHR,
        &sr_SQRT,
        &sr_Rot,
        &sr_Pick
    };

extern uint64_t cnt_VM_Push;
extern uint64_t cnt_VM_Pop;


extern uint64_t cnt_LPush;
extern uint64_t cnt_LPop;

extern uint64_t cnt_Print;
extern uint64_t cnt_Je;
extern uint64_t cnt_Mod;
extern uint64_t cnt_Sub;
extern uint64_t cnt_Over;
extern uint64_t cnt_Swap;
extern uint64_t cnt_Dup;
extern uint64_t cnt_Drop;
extern uint64_t cnt_Push;
extern uint64_t cnt_Nop;
extern uint64_t cnt_Halt;
extern uint64_t cnt_Break;
extern uint64_t cnt_Inc;
extern uint64_t cnt_Jump;

extern uint64_t asm_main();

extern uint64_t ret_steps;
extern uint64_t ret_state;
extern uint64_t ret_pc;
extern uint64_t ret_sp;
extern char * ret_err_ptr;
extern uint32_t * ret_stack;


int main(int argc, char **argv) {

    steplimit = parse_args(argc, argv);

    uint32_t stack[STACK_CAPACITY];

    asm_main(service_routines, DefProgram, Cpu_Running, steplimit);

    /* /\* decode_t decoded = fetch_decode(&cpu); *\/ */
    /* /\* service_routines[decoded.opcode](&cpu, &decoded); *\/ */

    /* Print CPU state */
    printf("CPU executed %ld steps. End state \"%s\".\n",
            ret_steps, ret_state == Cpu_Halted? "Halted":
                       ret_state == Cpu_Running? "Running": "Break");

    printf("PC = %lu, SP = %lu\n\n", ret_pc, ret_sp);

    printf("Errors: %s\n\n", ret_err_ptr);

    printf("Counters     :\n cnt_VM_Push : %20lu\n cnt_VM_Pop  : %20lu\n cnt_LPush   : %20lu\n cnt_LPop    : %20lu\n cnt_Print   : %20lu\n cnt_Je      : %20lu\n cnt_Mod     : %20lu\n cnt_Sub     : %20lu\n cnt_Over    : %20lu\n cnt_Swap    : %20lu\n cnt_Dup     : %20lu\n cnt_Drop    : %20lu\n cnt_Push    : %20lu\n cnt_Nop     : %20lu\n cnt_Halt    : %20lu\n cnt_Break   : %20lu\n cnt_Inc     : %20lu\n cnt_Jump    : %20lu\n",
           cnt_VM_Push, cnt_VM_Pop, cnt_LPush, cnt_LPop, cnt_Print, cnt_Je, cnt_Mod, cnt_Sub, cnt_Over, cnt_Swap, cnt_Dup, cnt_Drop, cnt_Push, cnt_Nop, cnt_Halt, cnt_Break, cnt_Inc, cnt_Jump);
    printf("Stack (%ld): \n", ret_sp);
    for (uint64_t i=0; i < ret_sp ; i++) {
        printf("%2lu : %20lu : %20d\n",
               i,
               ((uintptr_t)(&ret_stack + (i))),
               (*(uint32_t *)(&ret_stack + (i)))
            );
    }

    free(LoadedProgram);

    return ret_state == Cpu_Halted ||
           (ret_state == Cpu_Running &&
            ret_steps == steplimit)?0:1;
}

void fail(const char *message) {
    /* printf("CPU executed %ld steps. End state \"%s\".\n", */
    /*        ret_steps, ret_state == Cpu_Halted? "Halted": */
    /*        ret_state == Cpu_Running? "Running": "Break"); */

    fprintf(stderr, "FAIL: %s\n", message);
    exit(EXIT_FAILURE);
}

/*
CPU executed 5462956110 steps. End state "Halted".
PC = 32, SP = 2
    Errors: no errors.
    Counters :
    cnt_Print :                 9592
    cnt_Je    :            910487889
    cnt_Mod   :            455189149
    cnt_Add   :                    0
    cnt_Sub   :            455298740
    cnt_Over  :           1820985370
    cnt_Swap  :            910387890
    cnt_Dup   :                    0
    cnt_Drop  :                99998
    cnt_Push  :               100000
    cnt_Nop   :                    0
    cnt_Halt  :                    1
    cnt_Break :                    0
    cnt_Inc   :            455198741
    cnt_Jump  :            455198741
    Stack (2):
    0 :      100715924241663 :               100000
    1 :      100715924241671 :               100000
*/
