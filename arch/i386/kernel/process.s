; void switch_context(thread_context_t* context)
global switch_context
switch_context:
    MOV eax, [esp+4]
    
    MOV cx, 0x23
    MOV ds, cx
    MOV es, cx
    MOV fs, cx

    PUSH DWORD 0x23     ; SS
    PUSH DWORD [eax+16] ; USER ESP
    PUSH DWORD 0x202    ; EFLAGS
    PUSH DWORD 0x1B     ; CS 
    PUSH DWORD [eax+4]  ; EIP
    
    IRET


; void switch_task(uint32_t *prev_stack, uint32_t next_stack);
global switch_task
switch_task:
    pushad

    xor eax, eax      ; 
    mov ax, gs
    push eax          ; Save gs of outgoing thread into kernel stack

    mov eax, [esp+40] ; prev_thread addr of stack_ptr
    mov [eax], esp    ; Save current ESP in prev_thread context
    mov eax, [esp+44]     ; next_thread addr of stack_ptr
    mov esp, eax      ; Restore ESP from next_thread context

    pop eax
    mov gs, ax        ; Restore gs for incoming thread

    popad
    ret

global read_eip
read_eip:
	pop eax
	jmp eax