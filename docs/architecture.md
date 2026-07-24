# KayaOS Architecture

KayaOS su katmanlardan olusur:

1. `src/boot/boot.s`
   - Multiboot header'i tanimlar.
   - Stack hazirlar ve C tarafindaki `kernel_main` fonksiyonuna gecer.

2. `src/kernel/kernel.c`
   - Terminali, RAM filesystem'i ve shell'i baslatan ana kernel girisidir.

3. `src/kernel/terminal.c`
   - VGA text-mode ekran cikisi, renkler, kaydirma ve basit sayi yazdirma.

4. `src/kernel/keyboard.c`
   - PS/2 klavyeden scancode okur, ASCII karaktere cevirir.

5. `src/kernel/mouse.c`
   - PS/2 mouse'u etkinlestirir ve 3 byte'lik mouse paketlerini polling ile
     okur.

6. `src/kernel/ramfs.c`
   - Sabit kapasiteli, RAM tabanli, yazilabilir dosya agaci.
   - Mutlak ve goreli yollar, `.` ve `..` destegi vardir.

7. `src/kernel/shell.c`
   - Komut satiri dongusu ve filesystem komutlari.
   - `.op` dosyalarini `run` komutu ile satir satir calistiran basit program
     sistemi.

8. `src/kernel/gui.c`
   - `gui` komutu ile acilan text-mode GUI katmani.
   - Mouse imleci, pencere, durum cubugu ve tiklanabilir demo butonu cizer.

Bu proje bilincli olarak kucuk tutuldu: heap, process scheduler, paging ve disk
suruculeri henuz yok. Kaynak agaci bunlarin eklenebilecegi sekilde ayrildi.
