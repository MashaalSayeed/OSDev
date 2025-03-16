global switch_context
switch_context:
    CLI

    ; Load the new context
    MOV ds, [eax]
    MOV es, [eax]
    MOV fs, [eax]
    MOV gs, [eax]

    MOV edi, [eax+4]
    MOV esi, [eax+8]
    MOV ebp, [eax+12]
    MOV ebx, [eax+20]
    MOV edx, [eax+24]
    MOV ecx, [eax+28]

    PUSH DWORD [eax+60] ; Push SS
    PUSH DWORD [eax+56] ; Push ESP
    PUSH DWORD [eax+52] ; Push EFLAGS
    PUSH DWORD [eax+48] ; Push CS
    PUSH DWORD [eax+44] ; Push EIP

    MOV eax, [eax+32] ; Restore EAX

    STI
    IRET


global read_eip
read_eip:
	pop eax
	jmp eax

global user_exit
user_exit:
    mov ebx, eax
    mov eax, 5
    int 0x80