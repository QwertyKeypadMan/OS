# KayaOS

KayaOS, C ile yazilmis kucuk ama duzenli bir egitim isletim sistemi
cekirdegidir. GRUB uzerinden acilir, VGA terminale yazar, PS/2 klavyeden
girdi alir, kendi shell'ini calistirir ve RAM uzerinde yazilabilir bir
filesystem sunar.

## Ozellikler

- 32-bit x86 freestanding C kernel
- Multiboot uyumlu GRUB acilisi ve 1024x768x32 framebuffer istegi
- Framebuffer ustunde calisan grafik terminal
- Polling tabanli PS/2 klavye surucusu
- Polling tabanli PS/2 mouse surucusu
- Shell'den acilan bitmap tabanli GUI: `gui`
- Uncompressed 24/32-bit BMP asset yukleme
- Shell komutlari: `help`, `clear`, `ls`, `cd`, `pwd`, `cat`, `touch`,
  `mkdir`, `write`, `append`, `echo`, `run`, `gui`, `rm`, `tree`, `mem`,
  `version`, `halt`, `reboot`
- RAMFS: dizinler, dosyalar, yol cozme, yazma, ekleme ve silme
- `.op` program dosyalari: `run test.op` ile satir satir shell komutu calistirma
- Moduler kaynak agaci ve ISO uretebilen build sistemi

## Derleme

Gerekli araclar:

- `gcc` ve `gcc-multilib`
- `make`
- `grub-file` ve `grub-mkrescue`
- `xorriso`
- `qemu-system-i386`

Linux ya da Docker icinde:

```sh
make iso
make run
```

Docker ile:

```sh
docker build -t kayaos-build .
docker run --rm -it -v "$PWD:/workspace" kayaos-build make iso
docker run --rm -it -v "$PWD:/workspace" kayaos-build make run
```

Uretilen ISO:

```text
build/kayaos.iso
```

## Shell ornekleri

```text
help
ls /
cat /README.txt
mkdir /tmp/demo
write /tmp/demo/note.txt merhaba kayaos
append /tmp/demo/note.txt  - ikinci satir
cat /tmp/demo/note.txt
tree /
cat /test.op
run test.op
gui
cat /tmp/gui-note.txt
```

## .op programlari

`.op` dosyalari basit KayaOS program dosyalaridir. Her satir bir shell komutu
olarak calisir. Bos satirlar ve `#` ile baslayan yorum satirlari atlanir.

Sistem acilista `/test.op` dosyasini hazir getirir:

```text
run test.op
cat /tmp/test-output.txt
```

## GUI

`gui` komutu KayaOS'un bitmap tabanli GUI katmanini acar. GRUB framebuffer
verdiyse masaustu, dock, pencere, mouse cursor ve tiklanabilir demo butonu
piksel olarak cizilir. GUI icinde `q` veya `Esc` ile shell'e geri donulur.

Demo butonuna tiklaninca `/tmp/gui-note.txt` olusturulur:

```text
gui
cat /tmp/gui-note.txt
```

Bir sonraki dogal adim VESA/framebuffer grafik moda gecip bitmap cursor, ikon ve
piksel tabanli pencere cizimi eklemektir.

## BMP asset isimleri

BMP'leri `assets/` klasorune koy, sonra:

```sh
make assets
make iso
```

Kullanilan isimler:

- `wallpaper.bmp` - 1024x768 masaustu arka plani
- `cursor_arrow.bmp` - 32x32 mouse cursor
- `logo.bmp` - 96x96 sistem logosu
- `icon_terminal.bmp` - 48x48 terminal ikonu
- `icon_files.bmp` - 48x48 dosya ikonu
- `icon_settings.bmp` - 48x48 ayar ikonu
- `button_primary.bmp` - 220x54 ana buton kaplamasi
- `window_close.bmp` - 24x24 pencere kapatma butonu
- `dock_tile.bmp` - 72x72 dock kutusu

Cursor ve ikonlarda `#FF00FF` magenta pikseller transparan sayilir. Eksik BMP
olursa GUI kendi procedural fallback cizimlerini kullanir.

## Not

Filesystem RAM uzerindedir; makine yeniden baslayinca degisiklikler kaybolur.
Kalici disk destegi icin bir sonraki asama blok aygiti surucusu ve FAT benzeri
bir disk filesystem katmani eklemektir.
