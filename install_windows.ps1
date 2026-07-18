# install_windows.ps1
# ============================================================
# Screenplay Editor — Automated Windows 11 Setup Script
# Run from PowerShell as Administrator:
#   Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
#   .\install_windows.ps1
# ============================================================

$ErrorActionPreference = "Stop"

function Write-Step($msg) {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  $msg" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Test-Command($cmd) {
    return [bool](Get-Command $cmd -ErrorAction SilentlyContinue)
}

# ── 1. Check Windows version ─────────────────────────────────────────────────
Write-Step "Checking system"
$os = [System.Environment]::OSVersion.Version
Write-Host "Windows version: $($os.Major).$($os.Minor) (Build $($os.Build))"
if ($os.Major -lt 10) {
    Write-Error "Windows 10 or 11 required."
    exit 1
}
Write-Host "OK" -ForegroundColor Green

# ── 2. Install winget if missing ─────────────────────────────────────────────
Write-Step "Checking winget"
if (-not (Test-Command "winget")) {
    Write-Host "winget not found. Please install it from the Microsoft Store." -ForegroundColor Yellow
    Write-Host "Search for 'App Installer' in the Microsoft Store and install it."
    Pause
}

# ── 3. Install Git ───────────────────────────────────────────────────────────
Write-Step "Git"
if (-not (Test-Command "git")) {
    Write-Host "Installing Git..."
    winget install --id Git.Git -e --source winget --silent
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path","User")
} else {
    Write-Host "Git already installed: $(git --version)" -ForegroundColor Green
}

# ── 4. Install CMake ─────────────────────────────────────────────────────────
Write-Step "CMake"
if (-not (Test-Command "cmake")) {
    Write-Host "Installing CMake..."
    winget install --id Kitware.CMake -e --source winget --silent
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path","User")
} else {
    Write-Host "CMake already installed: $(cmake --version | Select-Object -First 1)" -ForegroundColor Green
}

# ── 5. Install Visual Studio Build Tools ────────────────────────────────────
Write-Step "Visual Studio 2022 Build Tools"
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$hasVS = (Test-Path $vsWhere) -and (& $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null)

if (-not $hasVS) {
    Write-Host "Installing Visual Studio 2022 Build Tools..."
    Write-Host "(This may take 10-20 minutes)" -ForegroundColor Yellow
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --source winget --silent `
        --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.22621"
} else {
    Write-Host "Visual Studio Build Tools already installed." -ForegroundColor Green
}

# ── 6. Install vcpkg ─────────────────────────────────────────────────────────
Write-Step "vcpkg"
$vcpkgDir = "C:\vcpkg"
if (-not (Test-Path "$vcpkgDir\vcpkg.exe")) {
    Write-Host "Cloning vcpkg to C:\vcpkg ..."
    git clone https://github.com/microsoft/vcpkg.git $vcpkgDir
    & "$vcpkgDir\bootstrap-vcpkg.bat" -disableMetrics
} else {
    Write-Host "vcpkg already at $vcpkgDir" -ForegroundColor Green
}

# Add vcpkg to PATH for this session
$env:Path += ";$vcpkgDir"

# ── 7. Install Qt6 + FreeType via vcpkg ──────────────────────────────────────
Write-Step "Qt6 + FreeType (via vcpkg — this takes 20-60 min first time)"
Write-Host "Triplet: x64-windows"

& "$vcpkgDir\vcpkg.exe" install `
    "qt6-base:x64-windows" `
    "freetype:x64-windows"

if ($LASTEXITCODE -ne 0) {
    Write-Error "vcpkg install failed. Check output above."
    exit 1
}

# ── 8. Build the project ─────────────────────────────────────────────────────
Write-Step "Building Screenplay Editor"
$projectDir = $PSScriptRoot
$buildDir   = "$projectDir\build"

Write-Host "Project dir : $projectDir"
Write-Host "Build dir   : $buildDir"

cmake -B $buildDir `
      -DCMAKE_TOOLCHAIN_FILE="$vcpkgDir\scripts\buildsystems\vcpkg.cmake" `
      -DCMAKE_BUILD_TYPE=Release `
      -A x64 `
      $projectDir

if ($LASTEXITCODE -ne 0) { Write-Error "CMake configuration failed."; exit 1 }

cmake --build $buildDir --config Release --parallel

if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit 1 }

# ── 9. Deploy Qt DLLs ────────────────────────────────────────────────────────
Write-Step "Deploying Qt runtime DLLs"
$exePath = "$buildDir\Release\ScreenplayEditor.exe"
$qtBin   = "$vcpkgDir\installed\x64-windows\tools\Qt6\bin"
$windeployqt = "$qtBin\windeployqt.exe"

if (Test-Path $windeployqt) {
    & $windeployqt --release --no-translations $exePath
    Write-Host "Qt DLLs deployed." -ForegroundColor Green
} else {
    Write-Host "windeployqt not found at $windeployqt" -ForegroundColor Yellow
    Write-Host "You may need to copy Qt DLLs manually or adjust PATH."
}

# ── 10. Done ──────────────────────────────────────────────────────────────────
Write-Step "Done!"
Write-Host ""
Write-Host "Executable: $exePath" -ForegroundColor Green
Write-Host ""
Write-Host "To run:  .\build\Release\ScreenplayEditor.exe"
Write-Host ""
Write-Host "Keyboard shortcuts:"
Write-Host "  Enter        — advance block type"
Write-Host "  Tab          — cycle block type"
Write-Host "  Ctrl+Z/Y     — undo / redo"
Write-Host "  Ctrl+S       — save"
Write-Host "  Ctrl+O       — open"
Write-Host "  Ctrl+= / -   — zoom in / out"
Write-Host "  Ctrl+0       — reset zoom"
Write-Host "  Ctrl+Wheel   — zoom with mouse"
