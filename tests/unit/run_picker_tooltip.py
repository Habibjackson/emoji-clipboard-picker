#!/usr/bin/env python3
"""Run the tooltip's QML source in an isolated Qt Quick Test module."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="emoji-qml-test-") as directory:
        module = Path(directory) / "EmojiKeyboard"
        module.mkdir()
        shutil.copy2(root / "qml/Theme.qml", module / "Theme.qml")
        shutil.copy2(root / "qml/components/PickerToolTip.qml", module / "PickerToolTip.qml")
        (module / "qmldir").write_text(
            "module EmojiKeyboard\n"
            "singleton Theme 1.0 Theme.qml\n"
            "PickerToolTip 1.0 PickerToolTip.qml\n"
        )
        result = subprocess.run(
            [sys.argv[1], "-input", str(root / "tests/unit/tst_PickerToolTip.qml"),
             "-import", directory], check=False
        )
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
