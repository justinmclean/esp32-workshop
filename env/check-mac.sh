#!/bin/bash
# ESP32 AI Workshop: macOS environment check. Report only, changes nothing.
#   bash check-mac.sh

fails=0; warns=0
ok()   { printf '  \033[32m[ OK ]\033[0m %s\n' "$1"; }
warn() { printf '  \033[33m[WARN]\033[0m %s\n' "$1"; warns=$((warns+1)); }
bad()  { printf '  \033[31m[FAIL]\033[0m %s\n' "$1"; fails=$((fails+1)); }
fix()  { printf '         \033[36mfix:\033[0m %s\n' "$1"; }
has()  { command -v "$1" >/dev/null 2>&1; }

printf '\nESP32 AI Workshop environment check (macOS)\n\n'

echo "System"
ok "macOS $(sw_vers -productVersion) on $(uname -m)"
case "$PWD" in
  *" "*) warn "Current folder path has spaces ($PWD)"; fix "Use a short path such as ~/dev/esp32-workshop" ;;
  *"Mobile Documents"*|*iCloud*) bad "Current folder is in iCloud Drive. Sync breaks builds."; fix "Clone to ~/dev/ instead" ;;
  *) ok "Working folder path looks safe: $PWD" ;;
esac

printf '\nGit\n'
if xcode-select -p >/dev/null 2>&1 && has git; then
  ok "$(git --version)"
  if git config --global user.name >/dev/null; then ok "git user.name set"
  else warn "git user.name not set"; fix 'git config --global user.name "Your Name"'; fi
else
  bad "git / Xcode Command Line Tools missing"; fix "xcode-select --install"
fi

printf '\nVS Code\n'
vsc="/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code"
if [ -x "$vsc" ]; then
  ok "VS Code installed"
  if ! has code; then
    warn "'code' not on PATH (optional)"
    fix "In VS Code: Cmd+Shift+P > 'Shell Command: Install code command in PATH'"
  elif ! code --version 2>/dev/null | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    warn "'code' on PATH is not VS Code ($(command -v code)). Another tool has taken the name."
  fi
  if "$vsc" --list-extensions 2>/dev/null | grep -qi '^platformio.platformio-ide$'; then
    ok "PlatformIO IDE extension installed"
  else
    warn "PlatformIO IDE extension not installed"
    fix "Install 'PlatformIO IDE' from the VS Code Extensions panel, then restart VS Code"
  fi
else
  warn "VS Code not found"; fix "brew install --cask visual-studio-code, or download from code.visualstudio.com"
fi

printf '\nPlatformIO\n'
piodir="$HOME/.platformio/penv/bin"
if has pio; then
  ok "pio on PATH: $(command -v pio)"
elif [ -x "$piodir/pio" ]; then
  warn "PlatformIO installed but pio not on PATH. Claude Code needs it on PATH."
  fix "echo 'export PATH=\"\$PATH:$piodir\"' >> ~/.zshrc && exec zsh"
  PATH="$PATH:$piodir"
else
  bad "PlatformIO Core not found"
  fix "Install the VS Code PlatformIO IDE extension and open VS Code once, or: brew install platformio"
fi
if has pio && ! piover=$(pio --version 2>&1); then
  bad "pio is installed but will not run (often a stale Intel or Python 2 install)"
  printf '         %s\n' "$(echo "$piover" | tail -1)"
  fix "rm -rf ~/.platformio/penv  then reopen VS Code so the PlatformIO extension rebuilds it"
elif has pio; then
  ok "$piover"
  if pio pkg list -g --only-platforms 2>/dev/null | grep -q espressif32; then
    ok "espressif32 platform installed"
  else
    warn "ESP32 toolchain not downloaded yet (about 5 GB, do it BEFORE the workshop)"
    fix "cd hello-cores3 in the workshop repo and run: pio run"
  fi
fi

printf '\nClaude Code\n'
if has claude; then ok "claude: $(claude --version 2>/dev/null)"
elif [ -x "$HOME/.local/bin/claude" ]; then
  warn "Claude Code installed but not on PATH"; fix "Add ~/.local/bin to PATH in ~/.zshrc"
else
  bad "Claude Code not found"; fix "curl -fsSL https://claude.ai/install.sh | bash"
fi

printf '\nM5Stack CoreS3 (plug it in with a DATA cable first)\n'
ports=$(ls /dev/cu.usbmodem* 2>/dev/null)
if [ -n "$ports" ]; then
  for p in $ports; do ok "USB serial device: $p"; done
else
  warn "No /dev/cu.usbmodem* device. Check it is a data cable, try another port or adapter."
  fix "If stuck, hold the side reset button about 3 seconds until the green LED lights (download mode)."
fi

printf '\nNetwork\n'
for h in api.elevenlabs.io api.anthropic.com github.com api.registry.platformio.org; do
  if nc -z -G 5 "$h" 443 >/dev/null 2>&1; then ok "Reachable: $h"; else bad "Cannot reach $h on 443"; fi
done

echo
if [ $fails -eq 0 ] && [ $warns -eq 0 ]; then printf '\033[32mAll good.\033[0m\n'
else printf '\033[33m%d failure(s), %d warning(s). Fix the FAIL lines first.\033[0m\n' $fails $warns; fi
echo
