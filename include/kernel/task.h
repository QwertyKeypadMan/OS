#include "task.h"
#include <stddef.h>

// paging.c içindeki sayfa dağıtıcımızı çağırıyoruz
extern uint32_t kmalloc_page();
// switch.asm içindeki assembly fonksiyonumuz
extern void switch_task(uint32_t *old_esp, uint32_t new_esp);

task_t *current_task = NULL;
int next_pid = 1;

void init_tasking() {
    // 1. Ana (Kernel) Görevi oluştur. Şu an çalışan kodun kendisidir.
    task_t *kernel_task = (task_t*)kmalloc_page();
    kernel_task->id = next_pid++;
    kernel_task->esp = 0; // İlk switch esnasında assembly tarafından doldurulacak
    kernel_task->next = kernel_task; // Kendine dönsün (Dairesel liste)

    current_task = kernel_task;
}

void create_task(void (*entry_point)()) {
    // 1. Yeni görev yapısı ve stack için 4KB yer ayır
    uint32_t page = kmalloc_page();
    task_t *new_task = (task_t*)page;
    new_task->id = next_pid++;

    // Stack, sayfanın en üstünden (sonundan) aşağıya doğru büyür
    uint32_t *stack = (uint32_t*)(page + 4096);

    // 2. SAHTE STACK ÇERÇEVESİ (Fake Stack Frame)
    // switch_task fonksiyonunun 'ret' ve 'pop' komutlarıyla sorunsuz 
    // açılması için stack'i sanki önceden çalışıyormuş gibi dolduruyoruz.
    *(--stack) = (uint32_t)entry_point; // EIP (ret komutu buraya zıplayacak)
    
    *(--stack) = 0; // EBP
    *(--stack) = 0; // EDI
    *(--stack) = 0; // ESI
    *(--stack) = 0; // EBX

    new_task->esp = (uint32_t)stack;

    // 3. Yeni görevi dairesel listeye (Round-Robin) ekle
    new_task->next = current_task->next;
    current_task->next = new_task;
}

// Görevler arasında manuel veya otomatik geçişi sağlayan fonksiyon
void task_switch() {
    if (!current_task) return;

    task_t *old_task = current_task;
    current_task = current_task->next; // Sıradaki göreve geç

    // Assembly modülünü ateşle
    switch_task(&(old_task->esp), current_task->esp);
}