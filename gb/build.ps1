<#
    build.ps1 - build and test the emulator with gcc and nothing else.

        .\gb\build.ps1                  build build\gbemu.exe and build\gbemu_tests.exe
        .\gb\build.ps1 -Test            build everything, then run every test
        .\gb\build.ps1 -Test m03        run only tests whose name contains "m03"
        .\gb\build.ps1 -Clean -Test     rebuild from scratch
        .\gb\build.ps1 -Release         -O2 instead of -g -O0
        .\gb\build.ps1 -Sanitize -Test  try -fsanitize=address,undefined (may not
                                        work on MinGW; if it fails, drop the flag)

    Exit code is 0 when all tests pass, 1 when any fails.
    There is no make or cmake dependency, on purpose: this machine has neither.
#>
[CmdletBinding()]
param(
    [switch]$Test,
    [switch]$Clean,
    [switch]$Release,
    [switch]$Sanitize,
    [string]$Filter = ''
)

$ErrorActionPreference = 'Stop'

$Root  = $PSScriptRoot
$Build = Join-Path $Root 'build'

# ---- find a compiler -------------------------------------------------------
$gcc = $null
$cmd = Get-Command gcc -ErrorAction SilentlyContinue
if ($cmd) { $gcc = $cmd.Source }
if (-not $gcc) {
    foreach ($candidate in @('C:\msys64\ucrt64\bin\gcc.exe',
                             'C:\msys64\mingw64\bin\gcc.exe',
                             'C:\MinGW\bin\gcc.exe')) {
        if (Test-Path $candidate) { $gcc = $candidate; break }
    }
}
if (-not $gcc) {
    Write-Host 'gcc not found.' -ForegroundColor Red
    Write-Host 'Install MSYS2, then: pacman -S mingw-w64-ucrt-x86_64-gcc'
    exit 1
}

if ($Clean -and (Test-Path $Build)) { Remove-Item -Recurse -Force $Build }
if (-not (Test-Path $Build)) { New-Item -ItemType Directory -Path $Build | Out-Null }

# ---- flags -----------------------------------------------------------------
$optFlags  = if ($Release) { @('-O2', '-DNDEBUG') } else { @('-g', '-O0') }
$warnFlags = @('-Wall', '-Wextra', '-Wshadow', '-Wvla',
               '-Wno-unused-parameter', '-Wno-unused-function')
$sanFlags  = @()
if ($Sanitize) { $sanFlags = @('-fsanitize=address,undefined', '-fno-omit-frame-pointer') }

$cflags = @('-std=c17') + $optFlags + $warnFlags + $sanFlags +
          @("-I$(Join-Path $Root 'include')")

function Invoke-Compiler {
    param([string[]]$CompilerArgs, [string]$What)
    Write-Host "==> $What"
    & $gcc @CompilerArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "$What FAILED" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

$srcAll    = @(Get-ChildItem -Path (Join-Path $Root 'src')   -Filter '*.c' |
               ForEach-Object { $_.FullName })
$srcNoMain = @($srcAll | Where-Object { (Split-Path $_ -Leaf) -ne 'main.c' })
$testSrc   = @(Get-ChildItem -Path (Join-Path $Root 'tests') -Filter '*.c' |
               ForEach-Object { $_.FullName })

$gbemu = Join-Path $Build 'gbemu.exe'
$tests = Join-Path $Build 'gbemu_tests.exe'

Invoke-Compiler -What 'building gbemu.exe' `
    -CompilerArgs (@('-o', $gbemu) + $srcAll + $cflags)

Invoke-Compiler -What 'building gbemu_tests.exe' `
    -CompilerArgs (@('-o', $tests) + $srcNoMain + $testSrc + $cflags)

if ($Test) {
    Write-Host ''
    if ($Filter) { Write-Host "==> running tests [filter=$Filter]" }
    else         { Write-Host '==> running tests' }
    Write-Host ''
    if ($Filter) { & $tests $Filter } else { & $tests }
    exit $LASTEXITCODE
}

Write-Host ''
Write-Host "built: $gbemu"
Write-Host "built: $tests"
Write-Host 'run the tests with:  .\gb\build.ps1 -Test'
