[CmdletBinding()]
param(
    [switch]$DryRun,
    [string]$RegistryRoot = 'HKCU:\Control Panel\NotifyIconSettings'
)

$ErrorActionPreference = 'Stop'
$appFileName = 'CodexWeekUsageTray.exe'

try {
    if (-not (Test-Path -LiteralPath $RegistryRoot)) {
        Write-Host 'No saved tray settings were found.'
        exit 0
    }

    $targets = @()
    foreach ($key in Get-ChildItem -LiteralPath $RegistryRoot) {
        $executablePath = (Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction Stop).ExecutablePath
        if ($executablePath -and [IO.Path]::GetFileName($executablePath) -ieq $appFileName) {
            $targets += [pscustomobject]@{
                KeyPath = $key.PSPath
                ExecutablePath = $executablePath
            }
        }
    }

    if ($targets.Count -eq 0) {
        Write-Host 'No CodexWeekUsageTray tray settings were found.'
        exit 0
    }

    Write-Host "Found $($targets.Count) CodexWeekUsageTray tray setting(s):"
    foreach ($target in $targets) {
        Write-Host "  $($target.ExecutablePath)"
    }

    if ($DryRun) {
        Write-Host 'Dry run only. Nothing was changed.'
        exit 0
    }

    $targetPaths = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($target in $targets) {
        [void]$targetPaths.Add($target.ExecutablePath)
    }

    $stopped = 0
    foreach ($process in Get-CimInstance Win32_Process -Filter "Name='$appFileName'") {
        if ($process.ExecutablePath -and $targetPaths.Contains($process.ExecutablePath)) {
            Stop-Process -Id $process.ProcessId -Force
            $stopped++
        }
    }

    foreach ($target in $targets) {
        Remove-Item -LiteralPath $target.KeyPath -Force
    }

    Write-Host "Removed $($targets.Count) tray setting(s). Stopped $stopped matching app process(es)."
    Write-Host 'Files were not deleted.'
}
catch {
    Write-Error $_
    exit 1
}
