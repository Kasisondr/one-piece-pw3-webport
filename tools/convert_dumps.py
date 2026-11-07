"""Convert dumped PPM frames to PNG."""
import os
import sys
from pathlib import Path
from PIL import Image

dir_name = sys.argv[1] if len(sys.argv) > 1 else "generated/opw3/dumps"
dumps_dir = Path(dir_name)
if not dumps_dir.exists():
    print(f"Directory {dumps_dir} not found.")
    sys.exit(0)

ppms = sorted(dumps_dir.glob("*.ppm"))
print(f"Found {len(ppms)} PPM files in {dumps_dir}.")
for ppm in ppms:
    png = ppm.with_suffix(".png")
    if not png.exists() or png.stat().st_mtime < ppm.stat().st_mtime:
        try:
            with Image.open(ppm) as im:
                im.save(png)
            print(f"Converted {ppm.name} -> {png.name}")
        except Exception as e:
            print(f"Failed to convert {ppm.name}: {e}")
