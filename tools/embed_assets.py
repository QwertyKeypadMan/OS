#!/usr/bin/env python3
from pathlib import Path
import re
import sys


def symbol_for(name: str) -> str:
    safe = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if not safe or safe[0].isdigit():
        safe = "_" + safe
    return safe


def bytes_to_c(data: bytes) -> str:
    chunks = []
    for index in range(0, len(data), 12):
        chunk = ", ".join(f"0x{byte:02X}" for byte in data[index:index + 12])
        chunks.append("    " + chunk + ",")
    return "\n".join(chunks)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: embed_assets.py <assets-dir> <generated-dir>")
        return 2

    assets_dir = Path(sys.argv[1])
    generated_dir = Path(sys.argv[2])
    generated_dir.mkdir(parents=True, exist_ok=True)

    # Desteklenen dosya uzantıları listesi
    ALLOWED_EXTENSIONS = {".bmp", ".ttf", ".h"}

    # Klasördeki hedef uzantılı tüm dosyaları topluyoruz
    asset_files = [
        p for p in assets_dir.glob("*")
        if p.is_file() and p.suffix.lower() in ALLOWED_EXTENSIONS
    ]
    
    # İsimlerine göre sıralıyoruz ki her derlemede sıralama sabit kalsın
    asset_files = sorted(asset_files)

    output = [
        '#include "kernel/assets.h"',
        "",
    ]

    entries = []
    for path in asset_files:
        data = path.read_bytes()
        # Çakışmayı önlemek için uzantıyı da içeren tam dosya adından sembol üretiyoruz (örn: tccdefs.h -> asset_tccdefs_h)
        symbol = "asset_" + symbol_for(path.name)
        output.append(f"static const uint8_t {symbol}[] = {{")
        output.append(bytes_to_c(data))
        output.append("};")
        output.append("")
        # C tarafında (asset_find) aratırken uzantısıyla beraber ("tccdefs.h" gibi) aratabilmek için path.name kullanıyoruz
        entries.append((path.name, symbol))

    if entries:
        output.append(f"const asset_t kernel_assets[{len(entries)}] = {{")
        for name, symbol in entries:
            output.append(f'    {{ "{name}", {symbol}, sizeof({symbol}) }},')
        output.append("};")
        output.append("")
        output.append(f"const size_t kernel_asset_count = {len(entries)};")
    else:
        output.append("const asset_t kernel_assets[1] = {")
        output.append("    { 0, 0, 0 },")
        output.append("};")
        output.append("")
        output.append("const size_t kernel_asset_count = 0;")

    (generated_dir / "assets.c").write_text("\n".join(output) + "\n", encoding="ascii")
    print(f"embedded {len(entries)} asset(s) (BMP/TTF/H)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())