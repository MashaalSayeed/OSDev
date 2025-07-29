; void switch_context(uint32_t *prev_stack, thread_context_t* context)
global switch_context
switch_context:
    ; save current context in previous stack
    pusha
    mov eax, [esp + 36] ; prev_thread addr of stack_ptr
    mov [eax], esp     ; Save current ESP in prev_thread context

    ; load next user context
    MOV eax, [esp+40]
    
    MOV cx, 0x23
    MOV ds, cx
    MOV es, cx
    MOV fs, cx
    MOV gs, cx

    PUSH DWORD [eax+48] ; SS
    PUSH DWORD [eax+44] ; USER ESP
    PUSH DWORD [eax+40] ; EFLAGS
    PUSH DWORD [eax+36] ; CS 
    PUSH DWORD [eax+32] ; EIP

    MOV edi, [eax+0]
    MOV esi, [eax+4]
    MOV ebp, [eax+8]
    MOV ebx, [eax+16]
    MOV edx, [eax+20]
    MOV ecx, [eax+24]
    MOV eax, [eax+28]
    
    IRET


; void switch_task(uint32_t *prev_stack, uint32_t next_stack);
global switch_task
switch_task:
    pusha

    mov eax, [esp+36] ; prev_thread addr of stack_ptr
    mov [eax], esp     ; Save current ESP in prev_thread context
    mov eax, [esp+40] ; next_thread addr of stack_ptr
    mov esp, eax     ; Restore ESP from next_thread context

    popa
    ret

global read_eip
read_eip:
	pop eax
	jmp eax

global user_exit
user_exit:
    mov ebx, eax
    mov eax, 60
    int 0x80