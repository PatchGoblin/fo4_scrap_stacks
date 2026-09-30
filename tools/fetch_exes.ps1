<#
Downloads every F4SE-supported Steam Fallout4.exe into re/ for offline testing, and
unpacks each with Steamless. Needs a Steam account that owns Fallout 4. DepotDownloader
asks for the password and Steam Guard code on the first run, then remembers the login.

    .\tools\fetch_exes.ps1 -Username <steam account name>

Manifest IDs come from community downgrade guides. Each
download's file version is checked, so a wrong ID is reported, never trusted.
Your game install is not touched; everything lands in re/ (gitignored).
#>
param(
    [Parameter(Mandatory)] [string] $Username
)

$ErrorActionPreference = 'Stop'
$root      = Split-Path -Parent $PSScriptRoot
$re        = Join-Path $root 're'
$depotDl   = Join-Path $root '.tools\depotdownloader\DepotDownloader.exe'
$steamless = Join-Path $root '.tools\steamless\Steamless.CLI.exe'

# version -> depot 377162 manifest ('' = current public build, $null = not known)
$builds = [ordered]@{
    '1.10.163' = '5847529232406005096'
    '1.10.980' = $null
    '1.10.984' = '5698952341602575696'
    '1.11.137' = '7771770220637696673'
    '1.11.159' = '1553582618043813733'
    '1.11.169' = '1314777104987018390'
    '1.11.191' = '5433405173062582852'
    '1.11.221' = '387388833281246371'
    '1.11.240' = ''
}

$fileList = Join-Path $re 'fo4exe.txt'
New-Item -ItemType Directory -Force $re | Out-Null
Set-Content -Path $fileList -Value 'Fallout4.exe' -Encoding ascii

$results = @()
foreach ($version in $builds.Keys) {
    $name     = 'Fallout4_' + $version.Replace('.', '_') + '.exe'
    $packed   = Join-Path $re $name
    $unpacked = "$packed.unpacked.exe"

    if (Test-Path $unpacked) {
        $results += [pscustomobject]@{ Version = $version; Result = 'already present' }
        continue
    }
    $manifest = $builds[$version]
    if ($null -eq $manifest) {
        $results += [pscustomobject]@{ Version = $version; Result = 'SKIPPED: no confirmed manifest ID' }
        continue
    }

    $out  = Join-Path $re "depots\$version"
    $ddArgs = @('-app', '377160', '-depot', '377162', '-username', $Username, '-remember-password',
              '-filelist', $fileList, '-dir', $out)
    if ($manifest) { $ddArgs += @('-manifest', $manifest) }
    Write-Host "== $version ==" -ForegroundColor Cyan
    & $depotDl @ddArgs
    $downloaded = Join-Path $out 'Fallout4.exe'
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $downloaded)) {
        $results += [pscustomobject]@{ Version = $version; Result = "FAILED: DepotDownloader exit $LASTEXITCODE" }
        continue
    }

    $actual = (Get-Item $downloaded).VersionInfo.FileVersion
    if ($actual -ne "$version.0") {
        $results += [pscustomobject]@{ Version = $version; Result = "WRONG BUILD: manifest gave $actual (kept in $out, not installed)" }
        continue
    }

    Copy-Item $downloaded $packed -Force
    & $steamless --quiet $packed | Out-Null
    if (-not (Test-Path $unpacked)) {
        # Not SteamStub-wrapped: the exe is already readable as-is.
        Copy-Item $packed $unpacked
    }
    $results += [pscustomobject]@{ Version = $version; Result = "ok ($actual)" }
}

$results | Format-Table -AutoSize
