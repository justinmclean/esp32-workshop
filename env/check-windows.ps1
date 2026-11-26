# ESP32 AI Workshop: Windows environment check
# Run in PowerShell (Windows PowerShell 5.1 or PowerShell 7):
#   powershell -ExecutionPolicy Bypass -File .\check-windows.ps1
#   powershell -ExecutionPolicy Bypass -File .\check-windows.ps1 -Install
# -Install uses winget to install anything missing that winget can install.
# Without it, the script only reports. It never changes system settings
# except where it prints the command for you to run yourself.

param([switch]$Install)

$ErrorActionPreference = 'Continue'
$script:fails = 0
$script:warns = 0

function Ok($msg)   { Write-Host "  [ OK ] $msg" -ForegroundColor Green }
function Warn($msg) { Write-Host "  [WARN] $msg" -ForegroundColor Yellow; $script:warns++ }
function Bad($msg)  { Write-Host "  [FAIL] $msg" -ForegroundColor Red; $script:fails++ }
function Fix($msg)  { Write-Host "         fix: $msg" -ForegroundColor Cyan }

function Refresh-Path {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' +
                [Environment]::GetEnvironmentVariable('Path', 'User')
}

function Has($cmd) { [bool](Get-Command $cmd -ErrorAction SilentlyContinue) }

function Try-Winget($id, $name) {
    if (-not $Install) { Fix "winget install --id $id -e"; return }
    if (-not (Has 'winget')) { Fix "winget not available. Install $name manually."; return }
    Write-Host "         installing $name via winget..." -ForegroundColor Cyan
    winget install --id $id -e --accept-source-agreements --accept-package-agreements
    Refresh-Path
}

Write-Host "`nESP32 AI Workshop environment check (Windows)`n"

# --- OS -------------------------------------------------------------------
Write-Host "System"
$os = Get-CimInstance Win32_OperatingSystem
$build = [int]$os.BuildNumber
if ($build -ge 19041) { Ok "$($os.Caption) build $build" }
else { Bad "Windows build $build is old. Windows 10 2004+ or Windows 11 recommended." }

$lp = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -ErrorAction SilentlyContinue).LongPathsEnabled
if ($lp -eq 1) { Ok "Long paths enabled" }
else {
    Warn "Long paths not enabled. ESP32 toolchains can hit the 260 character limit."
    Fix "In an ADMIN PowerShell: New-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force"
}

$cwd = (Get-Location).Path
if ($cwd -match 'OneDrive') {
    Bad "Current folder is inside OneDrive ($cwd). Sync locks break PlatformIO builds."
    Fix "Clone the workshop repo to C:\dev\ instead."
} elseif ($cwd -match ' ') {
    Warn "Current folder path has spaces ($cwd). Some toolchain steps dislike this."
    Fix "Use a short path such as C:\dev\esp32-workshop"
} else { Ok "Working folder path looks safe: $cwd" }

$docs = [Environment]::GetFolderPath('MyDocuments')
if ($docs -match 'OneDrive') {
    Warn "Documents is redirected to OneDrive ($docs). Do not put the project there."
}

# --- Git ------------------------------------------------------------------
Write-Host "`nGit"
if (-not (Has 'git')) { Bad "git not found"; Try-Winget 'Git.Git' 'Git for Windows' }
if (Has 'git') {
    Ok (git --version)
    $bash = @("$env:ProgramFiles\Git\bin\bash.exe", "$env:LOCALAPPDATA\Programs\Git\bin\bash.exe") |
            Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($bash) { Ok "Git Bash found (Claude Code uses it): $bash" }
    else {
        Warn "Git Bash not found in the usual places. Claude Code on Windows needs it."
        Fix "Set CLAUDE_CODE_GIT_BASH_PATH to your bash.exe, or reinstall Git for Windows."
    }
    $name = git config --global user.name
    if ($name) { Ok "git user.name set" } else {
        Warn "git user.name not set"
        Fix 'git config --global user.name "Your Name"; git config --global user.email "you@example.com"'
    }
}

# --- VS Code --------------------------------------------------------------
Write-Host "`nVS Code"
if (-not (Has 'code')) { Warn "VS Code 'code' command not found"; Try-Winget 'Microsoft.VisualStudioCode' 'VS Code' }
if (Has 'code') {
    Ok "VS Code installed"
    $ext = code --list-extensions 2>$null
    if ($ext -contains 'platformio.platformio-ide') { Ok "PlatformIO IDE extension installed" }
    else {
        Warn "PlatformIO IDE extension not installed"
        Fix "code --install-extension platformio.platformio-ide   (then open VS Code once and let it finish)"
    }
}

# --- PlatformIO -----------------------------------------------------------
Write-Host "`nPlatformIO"
$pioDir = Join-Path $env:USERPROFILE '.platformio\penv\Scripts'
$pioExe = Join-Path $pioDir 'pio.exe'
if (Has 'pio') { Ok "pio on PATH: $((Get-Command pio).Source)" }
elseif (Test-Path $pioExe) {
    Warn "PlatformIO is installed but pio is not on PATH. Claude Code needs it on PATH."
    Fix "[Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path','User') + ';$pioDir', 'User')   then open a NEW terminal"
    $env:Path += ";$pioDir"
} else {
    Bad "PlatformIO Core not found"
    Fix "Install the VS Code PlatformIO IDE extension and open VS Code once, or: python -m pip install -U platformio"
}
$pioVer = $null
if (Has 'pio') { $pioVer = pio --version 2>&1 | Out-String; if ($LASTEXITCODE -ne 0) {
    Bad "pio is installed but will not run: $($pioVer.Trim())"
    Fix "Remove-Item -Recurse -Force `$env:USERPROFILE\.platformio\penv   then reopen VS Code so the PlatformIO extension rebuilds it"
    $pioVer = $null } }
if ($pioVer) {
    Ok $pioVer.Trim()
    $plat = pio pkg list -g --only-platforms 2>$null | Out-String
    if ($plat -match 'espressif32') { Ok "espressif32 platform installed" }
    else {
        Warn "ESP32 toolchain not downloaded yet (about 5 GB, do it BEFORE the workshop)"
        Fix "cd hello-cores3 in the workshop repo and run: pio run"
    }
}

# --- Claude Code ----------------------------------------------------------
Write-Host "`nClaude Code"
if (-not (Has 'claude')) {
    $local = Join-Path $env:USERPROFILE '.local\bin\claude.exe'
    if (Test-Path $local) {
        Warn "Claude Code installed but not on PATH"
        Fix "Add $env:USERPROFILE\.local\bin to your user PATH, then open a new terminal"
    } else {
        Bad "Claude Code not found"
        Fix "irm https://claude.ai/install.ps1 | iex"
    }
} else { Ok "claude: $(claude --version 2>$null)" }

# --- Board ----------------------------------------------------------------
Write-Host "`nM5Stack CoreS3 (plug it in with a DATA cable before running this)"
$ports = Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
         Where-Object { $_.Name -match '\(COM\d+\)' }
$esp = $ports | Where-Object { $_.DeviceID -match 'VID_303A' }
if ($esp) { $esp | ForEach-Object { Ok "ESP32-S3 USB found: $($_.Name)" } }
elseif ($ports) {
    Warn "Serial ports found but none look like an ESP32-S3 (USB VID 303A):"
    $ports | ForEach-Object { Write-Host "         $($_.Name)" }
} else {
    Warn "No serial ports found. Check the cable is a data cable, try another port."
    Fix "If the board is stuck, hold the side reset button about 3 seconds until the green LED lights (download mode)."
}

# --- Network --------------------------------------------------------------
Write-Host "`nNetwork"
foreach ($h in 'api.elevenlabs.io', 'api.anthropic.com', 'github.com', 'api.registry.platformio.org') {
    try {
        $r = Test-NetConnection $h -Port 443 -WarningAction SilentlyContinue
        if ($r.TcpTestSucceeded) { Ok "Reachable: $h" } else { Bad "Cannot reach $h on 443" }
    } catch { Warn "Could not test $h" }
}

Write-Host ""
if ($script:fails -eq 0 -and $script:warns -eq 0) { Write-Host "All good." -ForegroundColor Green }
else { Write-Host "$($script:fails) failure(s), $($script:warns) warning(s). Fix the FAIL lines first." -ForegroundColor Yellow }
Write-Host ""
