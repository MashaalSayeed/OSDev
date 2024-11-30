global switch_context
switch_context:
    ; Skip return address, and get the pointer regs
    MOV ebp, [esp+4]

    ; Load the new context
    MOV eax, [ebp+0]
    MOV ds, eax
    MOV es, eax
    MOV fs, eax
    MOV gs, eax

    MOV edi, [ebp+4]
    MOV esi, [ebp+8]
    MOV ebx, [ebp+20]
    MOV edx, [ebp+24]
    MOV ecx, [ebp+28]
    MOV eax, [ebp+32]

    PUSH DWORD [ebp+60] ; Push SS
    PUSH DWORD [ebp+56] ; Push ESP
    PUSH DWORD [ebp+52] ; Push EFLAGS
    PUSH DWORD [ebp+48] ; Push CS
    PUSH DWORD [ebp+44] ; Push EIP

    MOV ebp, [ebp+12]
    IRET



