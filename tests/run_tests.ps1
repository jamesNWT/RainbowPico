# Builds and runs the host-side controller tests with a native gcc (not the ARM toolchain).
# Usage: .\tests\run_tests.ps1
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$gcc = Get-Command gcc -ErrorAction SilentlyContinue
$gcc = if ($gcc) { $gcc.Source } else { "$env:USERPROFILE\scoop\apps\gcc\current\bin\gcc.exe" }

$outDir = Join-Path $root 'build\host-tests'
New-Item -ItemType Directory -Force $outDir | Out-Null
$exe = Join-Path $outDir 'test_controller.exe'
$testSource = Join-Path $PSScriptRoot 'test_controller.c'
$flags = @('-std=c11', '-Wall', '-Wextra', '-g', '-O0', '-I', (Join-Path $root 'source'))

# Lets VS Code IntelliSense treat the test file as a host build (see .vscode/c_cpp_properties.json).
# Controller.c is left out on purpose so it keeps using the firmware's compile_commands.json.
ConvertTo-Json -Depth 3 -InputObject @(@{
    directory = $root
    file      = $testSource
    arguments = @($gcc) + $flags + @('-c', $testSource)
}) | Set-Content -Encoding utf8 (Join-Path $outDir 'compile_commands.json')

& $gcc @flags $testSource (Join-Path $root 'source\Controller.c') -o $exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $exe
exit $LASTEXITCODE
