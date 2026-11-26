# Workshop environment setup

Do this before the day. The ESP32 toolchain and Arduino framework come to about **5 GB** on disk, and venue wifi will not cope with 15 people downloading that at once. Most of it only downloads on your first build, so the setup ends with a test build.

When you are done, run the check script for your OS. Every line should be OK or an explained WARN.

## Everyone

- A USB **data** cable for the CoreS3 (many C-to-C cables are charge only; if no serial port appears, swap the cable first).
- If you bring your own micro SD card, it must be **FAT32**, not exFAT. Cards over 32 GB come as exFAT and the ESP32 cannot read them. On a Mac: Disk Utility, Show All Devices, select the card, Erase as MS-DOS (FAT) with Master Boot Record. On Windows, use a FAT32 formatting tool, because Windows will not offer FAT32 for large cards. Formatting erases the card.
- A phone hotspot on **2.4 GHz**. The ESP32 cannot see 5 GHz networks.
  - iPhone: Settings > Personal Hotspot > turn on **Maximise Compatibility**.
  - Android: Hotspot > Band > **2.4 GHz**.
  - Keep the hotspot name and password plain ASCII with no spaces.
- Clone the workshop repo to a **short local path with no spaces**: `C:\dev\esp32-workshop` or `~/dev/esp32-workshop`. Not OneDrive, not iCloud Drive, not Desktop or Documents if those are synced.
- Claude Code signed in on a **Pro plan or higher**. The free Claude plan does not include Claude Code.
- In the repo root, copy `secrets.example.h` to `secrets.h` and fill in your hotspot name and password. It is gitignored: never commit it, and never paste your keys into a public fork.

## Windows

Use **PowerShell** for everything. Windows Terminal is nicer but not required. You do not need WSL, and WSL makes USB access to the board harder, so avoid it.

1. Install the basics (winget ships with Windows 10 2004+ and 11):

   ```powershell
   winget install --id Git.Git -e
   winget install --id Microsoft.VisualStudioCode -e
   ```

   Close and reopen PowerShell afterwards so PATH updates.

2. Install Claude Code. It needs Git for Windows from step 1, because it runs its shell commands through Git Bash.

   ```powershell
   irm https://claude.ai/install.ps1 | iex
   ```

3. Install PlatformIO: open VS Code, install the **PlatformIO IDE** extension, wait for it to finish installing PlatformIO Core (watch the bottom right), then restart VS Code.

4. Put `pio` on your PATH so Claude Code can build and upload from the terminal:

   ```powershell
   [Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path','User') + ";$env:USERPROFILE\.platformio\penv\Scripts", 'User')
   ```

   Open a new PowerShell and check `pio --version` works.

5. Pre-download the ESP32 toolchain by building the test project in the workshop repo (first build takes a while):

   ```powershell
   cd C:\dev\esp32-workshop\hello-cores3
   pio run
   ```

   It should finish with `[SUCCESS]`.

6. Turn on long path support (needs an **Administrator** PowerShell, once per machine). ESP32 builds produce very deep paths and fail in confusing ways without it.

   ```powershell
   New-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force
   ```

7. Run the check from your workshop folder:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\check-windows.ps1
   ```

   Add `-Install` to have it winget install anything missing.

Windows notes:

- The CoreS3 uses the ESP32-S3's built-in USB, so **no driver is needed** on Windows 10/11. Ignore guides that tell you to install CP210x or CH9102 drivers; those are for older M5Stack boards.
- New laptops often redirect Desktop and Documents to OneDrive. OneDrive locks files during builds and causes random "permission denied" errors. Use `C:\dev`.
- Windows Defender scanning `.platformio` makes builds noticeably slower. Optional: add `%USERPROFILE%\.platformio` and `C:\dev` as exclusions in Windows Security if you are allowed to on your machine.
- Only one program can hold the COM port. Close the PlatformIO serial monitor (and Arduino IDE, and other terminals) before uploading.

## macOS

1. `xcode-select --install` (gives you git).
2. Install VS Code, then in VS Code run **Shell Command: Install 'code' command in PATH** from the command palette.
3. Install Claude Code: `curl -fsSL https://claude.ai/install.sh | bash`
4. Install the **PlatformIO IDE** extension in VS Code and let it finish.
5. Add pio to PATH: `echo 'export PATH="$PATH:$HOME/.platformio/penv/bin"' >> ~/.zshrc && exec zsh`
6. Pre-download the toolchain with a test build: `cd ~/dev/esp32-workshop/hello-cores3 && pio run` (should end with `[SUCCESS]`).
7. Run `bash check-mac.sh` from your workshop folder.

Mac notes:

- If you used PlatformIO years ago, the old install may be Intel or Python 2 and will not run on Apple Silicon (`Bad CPU type in executable`). Delete `~/.platformio/penv` and reopen VS Code to rebuild it.
- The board shows up as `/dev/cu.usbmodem...`. Through a USB hub or dongle it can be flaky; plug straight into the laptop if you can.

## Board will not upload

1. Try another cable, then another port.
2. Put the CoreS3 in download mode: hold the reset button on the side for about 3 seconds until the green LED lights, then upload again.
3. Close anything holding the serial port.
4. `pio device list` should show the board (USB VID 303A).
