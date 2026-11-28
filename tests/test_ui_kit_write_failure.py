"""The real-asset UI test must reject a failed contact-sheet write."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable, data = sys.argv[1:]
    if not (Path(data) / "a").is_dir():
        print("SKIPPED: no game archive tree")
        return 77
    with tempfile.TemporaryDirectory(prefix="eidos-ui-write-verdict-") as directory:
        root = Path(directory)
        env = os.environ.copy()
        env.update({
            "EIDOS_HOME": str(root), "EIDOS_USER_DIR": str(root),
            "BW_SAVE_DIR": str(root / "save"),
            "EIDOS_SETTINGS": str(root / "settings.ini"),
            "EIDOS_MODS_DIR": str(root / "mods"),
            "UI_KIT_SHEET": str(root / "missing-parent" / "sheet.png"),
        })
        result = subprocess.run([executable, data, "--assets-only"], cwd=root,
                                env=env, capture_output=True, text=True, timeout=120)
        output = result.stdout + result.stderr
        if result.returncode != 1 or "FAIL write contact sheet" not in output:
            print(f"unexpected UI test verdict {result.returncode}:\n{output}")
            return 1
        print("PASS: failed contact-sheet write was rejected")
        return 0


if __name__ == "__main__":
    sys.exit(main())
