.set ALIGN,    1 << 0
.set MEMINFO,  1 << 1
.set VIDEO,    1 << 2
.set FLAGS,    ALIGN | MEMINFO | VIDEO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0
.long 1024
.long 768
.long 32

.section .bss
.align 16
stack_bottom:
.skip 32768          /* 16 KB Kernel Yığını */
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    cli
    mov $stack_top, %esp

    /* ---- KRITIK: .bss TEMIZLEME ----
     * GRUB/multiboot .bss alanını sıfırlamaz; bu kernelin sorumluluğundadır.
     * linker.ld dosyasında tanımlı bss_start ve bss_end etiketleri aralarındaki
     * tüm statik tamponları sıfırlar. */
    mov $bss_start, %edi
    mov $bss_end, %ecx
    sub %edi, %ecx          /* ecx = bss_end - bss_start (bayt sayısı) */
    xor %eax, %eax
    cld
    rep stosb

    /* Multiboot yapı adresini ve sihirli sayıyı argüman olarak aktar */
    push %ebx
    push %eax
    call kernel_main
	
hang:
    cli
    hlt
    jmp hang

.size _start, . - _start


.global enter_user_mode
.type enter_user_mode, @function
enter_user_mode:
    /* Argümanları stack üzerinden al */
    mov 12(%esp), %edx   

   
    mov %edx, %cr3

    
    mov $0x23, %dx
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %fs
    mov %dx, %gs

    /* 3. IRET için yığın yapısını hazırla: (SS, ESP, EFLAGS, CS, EIP) sırasıyla push edilir */
    push $0x23           /* User Stack Segment (DATA_SEL | RPL 3 -> 0x20 | 3 = 0x23) */
    push %ecx            /* User Stack Pointer (ESP) */

    pushf                /* EFLAGS bayraklarını al */
    pop %edx
    or $0x200, %edx      /* Interrupt Enable (IF) bitini aç (Klavye vb. kesmeler çalışsın) */
    push %edx            /* Güncellenmiş EFLAGS'i geri yükle */

    push $0x1B           /* User Code Segment (CODE_SEL | RPL 3 -> 0x18 | 3 = 0x1B) */
    push %eax            /* User Instruction Pointer (EIP) */

    iret                 /* Zıpla! Artık Ring 3 Kullanıcı Modundayız. */

.size enter_user_mode, . - enter_user_mode