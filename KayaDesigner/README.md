# Kaya Designer

Kaya Designer, KayaOS arayuzleri icin C++ ile yazilan native bir tasarim aracidir.
SDL3 + Dear ImGui kullanir. Qt, Electron, .NET veya WinForms kullanmaz.

Bu arac KayaOS'un icine gomulmez; Windows uzerinde calisan ayri bir developer
uygulamasidir. Amaci canvas uzerinde widget tasarlayip KayaOS icin `.c` dosyasi
uretmek.

## Bagimliliklar

- CMake 3.22+
- C++20 compiler
- SDL3 development package
- Dear ImGui kaynak kodu

Dear ImGui kaynaklarini su klasore koy:

```text
KayaDesigner/third_party/imgui/
```

Beklenen dosyalar:

```text
imgui.cpp
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp
backends/imgui_impl_sdl3.cpp
backends/imgui_impl_sdlrenderer3.cpp
```

## Derleme

```sh
cmake -S KayaDesigner -B KayaDesigner/build
cmake --build KayaDesigner/build --config Release
```

## Ilk Surum Ozellikleri

- Menu bar
- Toolbar
- Widget palette
- Canvas
- Properties panel
- Hierarchy panel
- Output panel
- Drag & drop ile widget ekleme
- Widget secme, tasima, resize
- Grid, snap, zoom, pan
- Delete, copy, paste, undo, redo
- KayaOS icin `.c` export

## KayaOS'a export

Toolbar'daki `Export C` butonu varsayilan olarak su dosyayi yazar:

```text
../src/generated/kaya_designer_ui.c
```

Bu dosya KayaOS Makefile tarafinda `src/generated/*.c` ile otomatik derlenir.
Mevcut `src/kernel/gui.c` icinde `Designer UI` baslat menusu girdisi bu export
dosyasindaki fonksiyonu cagirir:

```c
void kaya_designer_ui_open(void);
```

Akis:

```sh
./KayaDesigner/build/KayaDesigner
# UI tasarla, Export C'ye bas
make iso
```

KayaOS acilinca Start menusunden `Designer UI` sec.
