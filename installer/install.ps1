#Requires -Version 5.1
# Awesome-OS CAS pnpm (12.23.0) — Stamped by @awesome-os/installer
# Usage: irm https://github.com/awesome-os/pnpm/releases/latest/download/install.ps1 | iex
param(
  [string]$InstallDir = "$env:LOCALAPPDATA\AwesomeOS\pnpm",
  [string]$BaseUrl = "https://github.com/awesome-os/pnpm/releases/latest/download"
)

$ErrorActionPreference = 'Stop'
Write-Host "[Awesome-OS CAS pnpm] Installing 12.23.0 -> $InstallDir ..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null

$exePath = Join-Path $InstallDir "pnpm.exe"
$sumsPath = Join-Path $InstallDir "SHA256SUMS.txt"

Invoke-WebRequest -UseBasicParsing -Uri "$BaseUrl/pnpm.exe" -OutFile $exePath
try {
  Invoke-WebRequest -UseBasicParsing -Uri "$BaseUrl/SHA256SUMS.txt" -OutFile $sumsPath
  $expectedLine = Get-Content $sumsPath | Where-Object { $_ -match '\s+pnpm\.exe$' } | Select-Object -First 1
  if ($expectedLine) {
    $expected = ($expectedLine -split '\s+')[0].ToLowerInvariant()
    $actual = (Get-FileHash -Algorithm SHA256 $exePath).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
      Remove-Item -Force $exePath -ErrorAction SilentlyContinue
      throw "SHA-256 mismatch for pnpm.exe! expected=$expected actual=$actual"
    }
    Write-Host "[Awesome-OS CAS pnpm] SHA-256 verified: $actual" -ForegroundColor Green
  }
} catch {
  Write-Warning "SHA256SUMS verification skipped: $($_.Exception.Message)"
}


Write-Host "[Awesome-OS CAS pnpm] Ready! Run: & `"$exePath`"" -ForegroundColor Green
