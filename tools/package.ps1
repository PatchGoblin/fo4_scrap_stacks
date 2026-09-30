<#
Builds ScrapStacks, runs every test, and packages a Nexus/MO2-ready zip in dist/.

    .\tools\package.ps1

The zip installs as a normal mod (F4SE\Plugins\ScrapStacks.dll + .ini) and carries
the license files GPL-3.0 requires. Refuses to package if any test fails.
#>
$ErrorActionPreference = 'Stop'
$root   = Split-Path -Parent $PSScriptRoot
$plugin = Join-Path $root 'plugin'
$xmake  = if ($env:XMAKE) { $env:XMAKE } elseif (Get-Command xmake -ErrorAction SilentlyContinue) { 'xmake' } else { throw 'xmake not found: put it on PATH or set $env:XMAKE' }

function Invoke-Step([string] $what, [scriptblock] $action) {
    Write-Host "== $what" -ForegroundColor Cyan
    & $action
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit $LASTEXITCODE)" }
}

Push-Location $plugin
try {
    # Uses the existing xmake configuration (so a developer's deploy_dir is kept);
    # on a fresh clone xmake configures itself with defaults.
    Invoke-Step 'build'     { & $xmake build ScrapStacks }
    Invoke-Step 'unit and exe tests' { & $xmake build tests; if ($LASTEXITCODE -eq 0) { & $xmake run tests } }
} finally {
    Pop-Location
}
Invoke-Step 'tool tests' { & (Join-Path $root '.venv\Scripts\python.exe') -m pytest -q (Join-Path $root 'tests') }

$version = (Select-String -Path (Join-Path $plugin 'xmake.lua') -Pattern 'set_version\("([^"]+)"\)').Matches[0].Groups[1].Value
$stage   = Join-Path $root "dist\stage"
$zip     = Join-Path $root "dist\ScrapStacks-$version.zip"
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
$plugins = New-Item -ItemType Directory -Force (Join-Path $stage 'F4SE\Plugins')

Copy-Item (Join-Path $plugin 'build\windows\x64\releasedbg\ScrapStacks.dll') $plugins
Copy-Item (Join-Path $plugin 'ScrapStacks.ini') $plugins
$docs = New-Item -ItemType Directory -Force (Join-Path $stage 'ScrapStacks')
foreach ($file in 'README.md', 'LICENSE', 'EXCEPTIONS', 'THIRD_PARTY_NOTICES.md') {
    Copy-Item (Join-Path $root $file) $docs
}

Remove-Item $zip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Remove-Item $stage -Recurse -Force
Write-Host "packaged $zip" -ForegroundColor Green
