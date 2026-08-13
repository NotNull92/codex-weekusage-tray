$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$uninstaller = Join-Path $repoRoot 'uninstall.cmd'
$testParent = 'HKCU:\Software\CodexWeekUsageTray\Tests'
$testRoot = "HKCU:\Software\CodexWeekUsageTray\Tests\$([guid]::NewGuid().ToString('N'))"
$matchingOne = Join-Path $testRoot 'matching-one'
$matchingTwo = Join-Path $testRoot 'matching-two'
$otherApp = Join-Path $testRoot 'other-app'

try {
    New-Item -Path $matchingOne -Force | Out-Null
    New-Item -Path $matchingTwo -Force | Out-Null
    New-Item -Path $otherApp -Force | Out-Null
    New-ItemProperty -Path $matchingOne -Name ExecutablePath -Value 'C:\Test\One\CodexWeekUsageTray.exe' -Force | Out-Null
    New-ItemProperty -Path $matchingTwo -Name ExecutablePath -Value 'C:\Test\Two\CodexWeekUsageTray.exe' -Force | Out-Null
    New-ItemProperty -Path $otherApp -Name ExecutablePath -Value 'C:\Test\Other\OtherTrayApp.exe' -Force | Out-Null

    & $uninstaller -Yes -RegistryRoot $testRoot
    if ($LASTEXITCODE -ne 0) {
        throw "uninstall.cmd exited with $LASTEXITCODE."
    }

    if ((Test-Path -LiteralPath $matchingOne) -or (Test-Path -LiteralPath $matchingTwo)) {
        throw 'uninstall.cmd must remove every CodexWeekUsageTray registry entry.'
    }

    if (-not (Test-Path -LiteralPath $otherApp)) {
        throw 'uninstall.cmd must not remove another tray app registry entry.'
    }
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }

    if ((Test-Path -LiteralPath $testParent) -and -not (Get-ChildItem -LiteralPath $testParent)) {
        Remove-Item -LiteralPath $testParent -Force
    }
}
