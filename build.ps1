param(
    [switch]$NoInstall
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $ProjectRoot)
$BuildDir = Join-Path $ProjectRoot "build\Release"
$GameScriptsDir = Join-Path $RepoRoot "Hitman 2 Silent Assassin\scripts"
$OutputAsi = Join-Path $BuildDir "h2_stats_overlay.asi"
$InstalledAsi = Join-Path $GameScriptsDir "h2_stats_overlay.asi"
$InstalledIni = Join-Path $GameScriptsDir "h2_stats_overlay.ini"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$VcVars = Join-Path ${env:ProgramFiles} "Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path -LiteralPath $VcVars)) {
    $VcVars = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
}
if (-not (Test-Path -LiteralPath $VcVars)) {
    throw "Could not find Visual Studio vcvarsall.bat."
}

$Sources = Get-ChildItem -LiteralPath (Join-Path $ProjectRoot "src") -Filter "*.cpp" | Sort-Object Name
$SourceArgs = $Sources | ForEach-Object { '"' + $_.FullName + '"' }
$CompilerArgs = @(
    "/nologo",
    "/std:c++17",
    "/EHsc",
    "/MT",
    "/O2",
    "/W4",
    "/DWIN32",
    "/D_WINDOWS",
    "/DNDEBUG",
    "/LD",
    ('/I' + (Join-Path $ProjectRoot "src")),
    ('/Fo' + $BuildDir + '\'),
    ('/Fe' + $OutputAsi)
) + $SourceArgs + @(
    "/link",
    "/NOLOGO",
    "/DLL",
    "/SUBSYSTEM:WINDOWS",
    "/MACHINE:X86"
)

$Command = 'call "' + $VcVars + '" x86 >nul && cl ' + ($CompilerArgs -join " ")
cmd.exe /c $Command
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

if (-not $NoInstall) {
    New-Item -ItemType Directory -Force -Path $GameScriptsDir | Out-Null
    Copy-Item -LiteralPath $OutputAsi -Destination $InstalledAsi -Force
    if (-not (Test-Path -LiteralPath $InstalledIni)) {
        Copy-Item -LiteralPath (Join-Path $ProjectRoot "h2_stats_overlay.ini") -Destination $InstalledIni
    }
    Write-Host "Installed:"
    Write-Host "  $InstalledAsi"
    Write-Host "  $InstalledIni"
} else {
    Write-Host "Built:"
    Write-Host "  $OutputAsi"
}
