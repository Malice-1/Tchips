param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'vcpkg.json') | ConvertFrom-Json
$baseline = $manifest.'builtin-baseline'
if ($baseline -notmatch '^[a-f0-9]{40}$') { throw 'Invalid pinned vcpkg baseline' }
$vcpkgDirectory = Join-Path $projectRoot '.tools/vcpkg'
foreach ($directory in @('.tools', '.tools/binary-cache', '.tools/downloads')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot $directory) | Out-Null
}
function Invoke-CheckedNative {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}
if (-not (Test-Path -LiteralPath (Join-Path $vcpkgDirectory '.git'))) {
    Invoke-CheckedNative -Executable git -Arguments @('clone', '--no-checkout', '--depth', '1', 'https://github.com/microsoft/vcpkg.git', $vcpkgDirectory)
}
$current = & git -C $vcpkgDirectory rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect vcpkg checkout' }
if ($current -ne $baseline) {
    $changes = & git -C $vcpkgDirectory status --porcelain
    if ($changes) { throw 'vcpkg checkout contains changes; refusing to overwrite them' }
    Invoke-CheckedNative -Executable git -Arguments @('-C', $vcpkgDirectory, 'fetch', '--depth', '1', 'origin', $baseline)
}
Invoke-CheckedNative -Executable git -Arguments @('-C', $vcpkgDirectory, 'checkout', '--detach', $baseline)
Invoke-CheckedNative -Executable (Join-Path $vcpkgDirectory 'bootstrap-vcpkg.bat') -Arguments @('-disableMetrics')
Write-Host "vcpkg ready at $baseline"
