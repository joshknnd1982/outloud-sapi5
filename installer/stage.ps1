# Stages built binaries and engine data into the output\ layout used by the
# installer and for local testing.
param(
    [string]$Root = (Split-Path $PSScriptRoot -Parent)
)

$ErrorActionPreference = "Stop"

$out = Join-Path $Root "output"
$engineOut = Join-Path $out "engine"
New-Item -ItemType Directory -Force $out | Out-Null
New-Item -ItemType Directory -Force (Join-Path $out "x64") | Out-Null
New-Item -ItemType Directory -Force $engineOut | Out-Null

# Binaries
Copy-Item (Join-Path $Root "build_x86\bin\Release\OutloudSAPI.dll") $out -Force
Copy-Item (Join-Path $Root "build_x86\bin\Release\outloud_host.exe") $out -Force
Copy-Item (Join-Path $Root "build_x86\bin\Release\OutloudConfig.exe") $out -Force
Copy-Item (Join-Path $Root "build_x86\bin\Release\client_test.exe") (Join-Path $out "client_test32.exe") -Force
Copy-Item (Join-Path $Root "build_x64\bin\Release\OutloudSAPI.dll") (Join-Path $out "x64") -Force
Copy-Item (Join-Path $Root "build_x64\bin\Release\client_test.exe") (Join-Path $out "client_test64.exe") -Force

# Engine data: everything from bin except NVDA addon glue and leftovers.
$bin = Join-Path $Root "bin"
$exclude = @(
    "*.py", "*.pyc", "__pycache__", "joshk", "unins000.dat", "unins000.exe",
    "tts.log", "outloud_test.exe", "ibmeci.lib", "eci.h", "ibmtts_host32.dll",
    "inicache.exe", "inifilter.exe", "inilog.exe", "initrace.exe",
    "inivoice.exe", "ssinivoice.exe", "ttsclean.exe", "cmmcmd.exe", "ibmcmm.exe",
    "eci.ini"
)
Get-ChildItem $bin -File | Where-Object {
    $name = $_.Name
    -not ($exclude | Where-Object { $name -like $_ })
} | ForEach-Object { Copy-Item $_.FullName $engineOut -Force }

# Dictionaries folder
if (Test-Path (Join-Path $bin "Dicts")) {
    Copy-Item (Join-Path $bin "Dicts") $engineOut -Recurse -Force
}

# Relative-path eci.ini (resolved against the engine dir at runtime).
Copy-Item (Join-Path $Root "installer\eci.template.ini") (Join-Path $engineOut "eci.ini") -Force

$count = (Get-ChildItem $engineOut -Recurse -File).Count
$size = [math]::Round(((Get-ChildItem $engineOut -Recurse -File | Measure-Object Length -Sum).Sum) / 1MB, 1)
Write-Host "Staged: $count engine files ($size MB) plus binaries in $out"
