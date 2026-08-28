# Stages built binaries and engine data into the output\ layout used by the
# installer and for local testing.
#
#   output\                     binaries (plus x64\OutloudSAPI.dll)
#   output\eci.template.ini     full engine configuration; setup filters it
#                               down to the languages the user selected
#   output\engine\              complete flat engine directory, for running the
#                               host and the tests straight from the build tree
#   output\install\core\        engine files shared by every language
#   output\install\lang\<code>\ the data files for one language
#   output\install\help\        the legacy IBM *.HLP / *.cnt documentation
#   output\install\dicts\chs\   the Mandarin parser dictionaries
#
# The per-component folders are what [Files] in outloud.iss installs, so the
# component split is decided here, in one place, rather than in wildcards.
param(
    [string]$Root = (Split-Path $PSScriptRoot -Parent)
)

$ErrorActionPreference = "Stop"

$out = Join-Path $Root "output"
$engineOut = Join-Path $out "engine"
$installOut = Join-Path $out "install"

if (Test-Path $installOut) { Remove-Item $installOut -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
New-Item -ItemType Directory -Force (Join-Path $out "x64") | Out-Null
New-Item -ItemType Directory -Force $engineOut | Out-Null

# ---- binaries ----
Copy-Item (Join-Path $Root "build_x86\bin\Release\OutloudSAPI.dll") $out -Force
Copy-Item (Join-Path $Root "build_x86\bin\Release\outloud_host.exe") $out -Force
Copy-Item (Join-Path $Root "build_x86\bin\Release\OutloudConfig.exe") $out -Force
Copy-Item (Join-Path $Root "build_x86\bin\Release\client_test.exe") (Join-Path $out "client_test32.exe") -Force
Copy-Item (Join-Path $Root "build_x64\bin\Release\OutloudSAPI.dll") (Join-Path $out "x64") -Force
Copy-Item (Join-Path $Root "build_x64\bin\Release\client_test.exe") (Join-Path $out "client_test64.exe") -Force

# ---- engine data ----
# The engine payload lives in bin\engine (the layout this project ships) or,
# for a raw IBM runtime drop, directly in bin.
$bin = Join-Path $Root "bin"
$engineSrc = Join-Path $bin "engine"
if (-not (Test-Path (Join-Path $engineSrc "ibmeci.dll"))) {
    $engineSrc = $bin
}
if (-not (Test-Path (Join-Path $engineSrc "ibmeci.dll"))) {
    throw "Engine files not found. Put the ViaVoice Outloud runtime in bin\engine (or bin)."
}

# Never staged: SDK leftovers, IBM's registry-based tools, NVDA add-on glue,
# and anything a previous installation left behind.
$exclude = @(
    "*.py", "*.pyc", "__pycache__", "joshk", "unins000.dat", "unins000.exe",
    "tts.log", "outloud_test.exe", "ibmeci.lib", "eci.h", "ibmtts_host32.dll",
    "inicache.exe", "inifilter.exe", "inilog.exe", "initrace.exe",
    "inivoice.exe", "ssinivoice.exe", "ttsclean.exe", "cmmcmd.exe", "ibmcmm.exe",
    "eci.ini", "voices.ini"
)

$engineFiles = Get-ChildItem $engineSrc -File | Where-Object {
    $name = $_.Name
    -not ($exclude | Where-Object { $name -like $_ })
}

# Flat engine directory for local testing, with the unfiltered eci.ini.
Get-ChildItem $engineOut -File -ErrorAction SilentlyContinue | Remove-Item -Force
$engineFiles | ForEach-Object { Copy-Item $_.FullName $engineOut -Force }
if (Test-Path (Join-Path $engineSrc "Dicts")) {
    Copy-Item (Join-Path $engineSrc "Dicts") $engineOut -Recurse -Force
}
Copy-Item (Join-Path $Root "installer\eci.template.ini") (Join-Path $engineOut "eci.ini") -Force
Copy-Item (Join-Path $Root "installer\eci.template.ini") $out -Force

# ---- component split ----
# Every engine data file is named after its language: ENU50.syn, enulang.dll,
# CHSrom.dll and so on. Anything that does not start with a language code is
# shared by all of them.
$codes = @("ENU","ENG","ESP","ESM","FRA","FRC","DEU","ITA","CHS","PTB","JPN","FIN","KOR","CTT","NOR","SWE","DAN")

New-Item -ItemType Directory -Force (Join-Path $installOut "core") | Out-Null
New-Item -ItemType Directory -Force (Join-Path $installOut "help") | Out-Null
foreach ($code in $codes) {
    New-Item -ItemType Directory -Force (Join-Path $installOut "lang\$code") | Out-Null
}

foreach ($file in $engineFiles) {
    $name = $file.Name
    if ($name -like "*.HLP" -or $name -like "*.cnt") {
        $dest = Join-Path $installOut "help"
    } else {
        $code = $codes | Where-Object { $name.ToUpperInvariant().StartsWith($_) } | Select-Object -First 1
        if ($code) { $dest = Join-Path $installOut "lang\$code" } else { $dest = Join-Path $installOut "core" }
    }
    Copy-Item $file.FullName $dest -Force
}

# accfilter.dll is referenced only by the Japanese section of eci.ini.
$acc = Join-Path $installOut "core\accfilter.dll"
if (Test-Path $acc) { Move-Item $acc (Join-Path $installOut "lang\JPN") -Force }

# The Mandarin parser dictionaries are the only per-language subdirectory.
if (Test-Path (Join-Path $engineSrc "Dicts")) {
    New-Item -ItemType Directory -Force (Join-Path $installOut "dicts\chs") | Out-Null
    Copy-Item (Join-Path $engineSrc "Dicts") (Join-Path $installOut "dicts\chs") -Recurse -Force
}

# ---- report ----
function Report($label, $path) {
    if (-not (Test-Path $path)) { return }
    $files = @(Get-ChildItem $path -Recurse -File)
    if ($files.Count -eq 0) { return }
    $mb = [math]::Round((($files | Measure-Object Length -Sum).Sum) / 1MB, 1)
    Write-Host ("  {0,-10} {1,3} files  {2,6} MB" -f $label, $files.Count, $mb)
}

Write-Host "Staged into $out"
Report "core" (Join-Path $installOut "core")
foreach ($code in $codes) { Report $code (Join-Path $installOut "lang\$code") }
Report "help" (Join-Path $installOut "help")
Report "dicts" (Join-Path $installOut "dicts")
