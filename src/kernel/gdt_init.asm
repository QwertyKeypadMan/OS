global gdt_flush

gdt_flush:
    mov eax, [esp+4]  ; C dilinden gelen gdt_ptr adresini al
    lgdt [eax]        ; Yeni GDT tablosunu işlemciye yükle!

    ; Yeni Data segmentlerimizi (0x10 = Kernel Data) register'lara doldur
    mov ax, 0x10      
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Kod segmentini (0x08 = Kernel Code) tazelemek için Far Jump
    jmp 0x08:.flush

.flush:
    ret               ; gdt_flush işlemi burada güvenle biter ve C koduna döner

global tss_flush

tss_flush:
    ; TSS segmentimiz GDT'de 5. sırada. 5 * 8 bayt = 40 (0x28)
    ; Ayrıca User Mode (Ring 3) erişimi için alt iki biti 1 yapıyoruz (0x28 | 0x03 = 0x2B)
    mov ax, 0x2B
    ltr ax       ; Load Task Register komutu
    ret