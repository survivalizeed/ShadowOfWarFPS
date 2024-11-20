.data
savedRAX QWORD 0
savedRBX QWORD 0
savedRCX QWORD 0
savedRDX QWORD 0
savedRSI QWORD 0
savedRDI QWORD 0
savedRBP QWORD 0
savedRSP QWORD 0
savedR8  QWORD 0
savedR9  QWORD 0
savedR10 QWORD 0
savedR11 QWORD 0
savedR12 QWORD 0
savedR13 QWORD 0
savedR14 QWORD 0
savedR15 QWORD 0

.code
StoreAllRegisters PROC
    mov savedRAX, rax
    mov savedRBX, rbx
    mov savedRCX, rcx
    mov savedRDX, rdx
    mov savedRSI, rsi
    mov savedRDI, rdi
    mov savedRBP, rbp
    mov savedRSP, rsp
    mov savedR8,  r8
    mov savedR9,  r9
    mov savedR10, r10
    mov savedR11, r11
    mov savedR12, r12
    mov savedR13, r13
    mov savedR14, r14
    mov savedR15, r15
    ret
StoreAllRegisters ENDP

RestoreAllRegisters PROC
    mov rax, savedRAX
    mov rbx, savedRBX
    mov rcx, savedRCX
    mov rdx, savedRDX
    mov rsi, savedRSI
    mov rdi, savedRDI
    mov rbp, savedRBP
    mov rsp, savedRSP
    mov r8,  savedR8
    mov r9,  savedR9
    mov r10, savedR10
    mov r11, savedR11
    mov r12, savedR12
    mov r13, savedR13
    mov r14, savedR14
    mov r15, savedR15
    ret
RestoreAllRegisters ENDP

END