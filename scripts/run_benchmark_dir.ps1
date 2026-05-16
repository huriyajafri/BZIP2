# Run bz.exe on every file in benchmarks/ (compress + decompress + verify).
# Usage:
#   .\scripts\run_benchmark_dir.ps1
#   .\scripts\run_benchmark_dir.ps1 -SaveStages

param(
    [string]$BenchDir = "benchmarks",
    [string]$ResultsDir = "results",
    [string]$StagesDir = "stages",
    [switch]$SaveStages
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path "bz.exe")) { throw "Run 'make cli' first (bz.exe missing)." }
if (-not (Test-Path $BenchDir)) { throw "Directory not found: $BenchDir" }

New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
if ($SaveStages) {
    (Get-Content config.ini) -replace 'save_stages\s*=\s*\w+', 'save_stages = true' |
        Set-Content config.ini
    (Get-Content config.ini) -replace 'stages_directory\s*=\s*.*', "stages_directory = ./$StagesDir" |
        Set-Content config.ini
}

Get-ChildItem -Path $BenchDir -File | Sort-Object Name | ForEach-Object {
    $src = $_.FullName
    $name = $_.Name
    $bz2 = Join-Path $ResultsDir "$name.bz2"
    $out = Join-Path $ResultsDir "$name.restored"

    Write-Host "`n=== $name ===" -ForegroundColor Cyan
    & .\bz.exe $src -c $bz2
    if ($LASTEXITCODE -ne 0) { Write-Host "  compress FAILED"; return }
    & .\bz.exe $bz2 -d $out
    if ($LASTEXITCODE -ne 0) { Write-Host "  decompress FAILED"; return }

    $same = (Get-FileHash $src).Hash -eq (Get-FileHash $out).Hash
    if ($same) { Write-Host "  roundtrip OK" -ForegroundColor Green }
    else { Write-Host "  roundtrip FAIL" -ForegroundColor Red }
}

Write-Host "`nDone. Outputs in $ResultsDir\"
