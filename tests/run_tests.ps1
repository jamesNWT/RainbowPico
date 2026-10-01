# Builds and runs the host-side controller tests with a native gcc (not the ARM toolchain).
# Usage: .\tests\run_tests.ps1
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
$gcc = if ($gcc) { $gcc.Source } else { "$env:USERPROFILE\scoop\apps\gcc\current\bin\gcc.exe" }

$outDir = Join-Path $root 'build\host-tests'
New-Item -ItemType Directory -Force $outDir | Out-Null
$exe = Join-Path $outDir 'test_controller.exe'

& $gcc -std=c11 -Wall -Wextra -g -O0 `
    -I (Join-Path $root 'source') `
    (Join-Path $PSScriptRoot 'test_controller.c') `
    (Join-Path $root 'source\Controller.c') `
    -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $exe
exit $LASTEXITCODE
