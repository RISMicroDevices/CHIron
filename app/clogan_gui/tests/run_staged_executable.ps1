param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath
)

$sourceDir = Split-Path -Parent $ExecutablePath
$executableName = Split-Path -Leaf $ExecutablePath
$runDir = Join-Path $env:TEMP ("clogan_gui_test_" + [Guid]::NewGuid().ToString("N"))

New-Item -ItemType Directory -Path $runDir | Out-Null

try {
    Copy-Item -LiteralPath $ExecutablePath -Destination $runDir

    Get-ChildItem -LiteralPath $sourceDir -File -Filter *.dll | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $runDir
    }

    foreach ($pluginDir in @("platforms", "imageformats", "iconengines", "styles", "networkinformation", "tls", "generic", "translations")) {
        $pluginPath = Join-Path $sourceDir $pluginDir
        if (Test-Path -LiteralPath $pluginPath) {
            Copy-Item -LiteralPath $pluginPath -Destination (Join-Path $runDir $pluginDir) -Recurse
        }
    }

    Push-Location $runDir
    & (Join-Path $runDir $executableName)
    $exitCode = $LASTEXITCODE
    Pop-Location

    exit $exitCode
} finally {
    Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue
}
