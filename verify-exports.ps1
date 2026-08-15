param(
    [string]$DllPath = "dist\PalworldKeyInjector.dll"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Verifier = Join-Path $ProjectRoot "tools\verify_pe.py"

$Python = Get-Command py -ErrorAction SilentlyContinue
if ($null -ne $Python) {
    & $Python.Source -3 $Verifier $DllPath --root $ProjectRoot
} else {
    $Python = Get-Command python -ErrorAction SilentlyContinue
    if ($null -eq $Python) {
        throw "Python 3 is required to run the strict PE verifier."
    }
    & $Python.Source $Verifier $DllPath --root $ProjectRoot
}

if ($LASTEXITCODE -ne 0) {
    throw "PalworldKeyInjector verification failed."
}

Write-Host "Export, import, architecture, mitigation, host, and version checks passed."
