 /*
 * Public API (System V AMD64 ABI):
 * int tm_step(TMState *state)   one transition, returns halted
 * int tm_run (TMState *state)   loop until halt, returns halted
 *
 * Stack layout (higher to lower addresses):
 *   C stack (caller frames)
 *   saved callee regs <- saved_c_rsp
 *   ...
 *   TM private stack (64 KiB) <- tm_stack_top (RSP during sim)
 *   ...
*/

    .intel_syntax noprefix
    .text

// TMState field offsets (must match tm_types.h)
.equ OFF_STATE, 0  /*int32 current_state*/
.equ OFF_HEAD, 4  /*int32 head_pos*/
.equ OFF_STEPS, 8  /*int32 step_count*/
.equ OFF_HALTED, 12  /*int32 halted*/
.equ OFF_ACCEPTED,16  /*int32 accepted*/
.equ OFF_TAPELEN, 20  /*int32 tape_len*/
.equ OFF_TAPE, 24  /*ptr tape*/
.equ OFF_NTRANS, 32  /*int32 num_transitions*/
.equ OFF_TRANS, 40  /*ptr transitions*/
.equ OFF_CSAVE, 48  /*uint64 saved_c_rsp*/
.equ OFF_TMSTACK, 56  /*uint64 tm_stack_top*/

.equ TR_FROM, 0
.equ TR_READ, 4
.equ TR_TO, 8
.equ TR_WRITE, 12
.equ TR_DIR, 16
.equ TR_SIZE, 20


    .global tm_step
    .type tm_step, @function
tm_step:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi  /*r12 = TMState pointer*/

    mov QWORD PTR [r12 + OFF_CSAVE], rsp
    mov rsp, QWORD PTR [r12 + OFF_TMSTACK]

    cmp DWORD PTR [r12 + OFF_HALTED], 0
    jne .Lstep_halted

    mov eax, DWORD PTR [r12 + OFF_STATE]  /*eax = state*/
    mov ebx, DWORD PTR [r12 + OFF_HEAD]  /*ebx = head*/

    mov rcx, QWORD PTR [r12 + OFF_TAPE]
    movzx edx, BYTE PTR [rcx + rbx]  /*edx = tape[head]*/

    mov r13d, DWORD PTR [r12 + OFF_NTRANS]
    mov r14, QWORD PTR [r12 + OFF_TRANS]
    xor r15d, r15d  /*index = 0*/

.Lstep_search:
    cmp r15d, r13d
    jge .Lstep_no_match

    cmp eax, DWORD PTR [r14 + TR_FROM]
    jne .Lstep_next
    cmp edx, DWORD PTR [r14 + TR_READ]
    jne .Lstep_next

    mov esi, DWORD PTR [r14 + TR_WRITE]
    mov rcx, QWORD PTR [r12 + OFF_TAPE]
    mov BYTE PTR [rcx + rbx], sil

    mov esi, DWORD PTR [r14 + TR_TO]
    mov DWORD PTR [r12 + OFF_STATE], esi

    cmp DWORD PTR [r14 + TR_DIR], 1  /*1 = L*/
    je .Lstep_go_left
    inc ebx  /*R*/
    jmp .Lstep_head_ok
.Lstep_go_left:
    dec ebx
.Lstep_head_ok:
    test ebx, ebx
    jns .Lstep_not_neg
    xor ebx, ebx
.Lstep_not_neg:
    cmp ebx, DWORD PTR [r12 + OFF_TAPELEN]
    jl .Lstep_in_range
    mov ebx, DWORD PTR [r12 + OFF_TAPELEN]
    dec ebx
.Lstep_in_range:
    mov DWORD PTR [r12 + OFF_HEAD], ebx

    inc DWORD PTR [r12 + OFF_STEPS]

    cmp DWORD PTR [r12 + OFF_STATE], 2
    jne .Lstep_still_running
    mov DWORD PTR [r12 + OFF_HALTED], 1
    mov DWORD PTR [r12 + OFF_ACCEPTED], 1
    jmp .Lstep_halted

.Lstep_still_running:
    mov rsp, QWORD PTR [r12 + OFF_CSAVE]
    xor eax, eax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

.Lstep_next:
    add r14, TR_SIZE
    inc r15d
    jmp .Lstep_search

.Lstep_no_match:
    mov DWORD PTR [r12 + OFF_HALTED], 1

.Lstep_halted:
    mov rsp, QWORD PTR [r12 + OFF_CSAVE]
    mov eax, 1
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

    .size tm_step, . - tm_step
    .global tm_run
    .type tm_run, @function
tm_run:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov r12, rdi  /*TMState ptr*/

    mov QWORD PTR [r12 + OFF_CSAVE], rsp
    mov rsp, QWORD PTR [r12 + OFF_TMSTACK]

    mov r13d, DWORD PTR [r12 + OFF_NTRANS]
    mov r14, QWORD PTR [r12 + OFF_TRANS]
    mov r8, QWORD PTR [r12 + OFF_TAPE]
    mov r9d, DWORD PTR [r12 + OFF_TAPELEN]

    cmp DWORD PTR [r12 + OFF_HALTED], 0
    jne .Lrun_done

.Lrun_loop:
    mov eax, DWORD PTR [r12 + OFF_STATE]
    mov ebx, DWORD PTR [r12 + OFF_HEAD]
    movzx edx, BYTE PTR [r8 + rbx]

    mov rcx, r14  /*scan ptr*/
    xor esi, esi  /*index*/

.Lrun_search:
    cmp esi, r13d
    jge .Lrun_no_match

    cmp eax, DWORD PTR [rcx + TR_FROM]
    jne .Lrun_next
    cmp edx, DWORD PTR [rcx + TR_READ]
    jne .Lrun_next

    mov edi, DWORD PTR [rcx + TR_WRITE]
    mov BYTE PTR [r8 + rbx], dil

    mov edi, DWORD PTR [rcx + TR_TO]
    mov DWORD PTR [r12 + OFF_STATE], edi

    cmp DWORD PTR [rcx + TR_DIR], 1
    je .Lrun_left
    inc ebx
    jmp .Lrun_head_ok
.Lrun_left:
    dec ebx
.Lrun_head_ok:
    test ebx, ebx
    jns .Lrun_not_neg
    xor ebx, ebx
.Lrun_not_neg:
    cmp ebx, r9d
    jl .Lrun_in_range
    mov ebx, r9d
    dec ebx
.Lrun_in_range:
    mov DWORD PTR [r12 + OFF_HEAD], ebx

    inc DWORD PTR [r12 + OFF_STEPS]

    cmp DWORD PTR [r12 + OFF_STATE], 2
    jne .Lrun_loop
    mov DWORD PTR [r12 + OFF_HALTED], 1
    mov DWORD PTR [r12 + OFF_ACCEPTED], 1
    jmp .Lrun_done

.Lrun_next:
    add rcx, TR_SIZE
    inc esi
    jmp .Lrun_search

.Lrun_no_match:
    mov DWORD PTR [r12 + OFF_HALTED], 1

.Lrun_done:
    mov rsp, QWORD PTR [r12 + OFF_CSAVE]
    mov eax, 1
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

    .size tm_run, . - tm_run

    .section .note.GNU-stack, "", @progbits
