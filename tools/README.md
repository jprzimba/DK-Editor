# Tools

These scripts are optional local smoke tests. They are not needed to build or
run the editor.

- `screenshot.ps1` launches `build\Release\DKEditor.exe` with a ROM, captures
  the window with `PrintWindow` and saves a PNG. Example:

  ```powershell
  powershell -ExecutionPolicy Bypass -File tools\screenshot.ps1 `
    -Rom "Donkey Kong Country (U) (V1.2) [!].smc" `
    -Out "docs/images/dkc1-editor.png" `
    -Level "0x02"
  ```

- `gui_paint_test.ps1` copies the ROM to a temporary file, opens the editor,
  simulates paint/erase strokes, clicks Save, closes the editor, and leaves a
  screenshot. Compare the temp ROM against the original to confirm the tilemap
  changed. It never modifies the source ROM.
- `generate_icon.py` and `make_icon.py` build the application icon. The source
  art is `src/resources/DKEditor.png`; the Windows resource is
  `src/resources/DKEditor.ico`; the README preview is `docs/images/icon.png`.

These scripts assume the repository root is the current directory and that the
Release build already exists.
