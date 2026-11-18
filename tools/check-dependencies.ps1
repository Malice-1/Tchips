param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$registry = Join-Path $projectRoot '.tools/vcpkg'
$manifest = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'vcpkg.json') | ConvertFrom-Json
$actual = & git -C $registry rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $manifest.'builtin-baseline') { throw 'Run tools/bootstrap.ps1 to select the pinned registry' }
$rows = foreach ($port in @('sdl3','entt','gamenetworkingsockets','protobuf','rmlui','miniaudio','libpqxx','gtest','tracy')) {
    $file = Join-Path $registry "ports/$port/vcpkg.json"
    if (-not (Test-Path -LiteralPath $file)) { throw "Missing port: $port" }
    $definition = Get-Content -Raw -LiteralPath $file | ConvertFrom-Json
    $version = @($definition.version, $definition.'version-semver', $definition.'version-string', $definition.'version-date') | Where-Object { $_ } | Select-Object -First 1
    [PSCustomObject]@{ Port=$port; Version=$version; Supports=$definition.supports; Status='Present in registry; not necessarily built' }
}
$rows | Format-Table -AutoSize
Write-Host 'SDL_GPU is part of SDL3, not a separate port.'
Write-Host 'PostgreSQL server is an external runtime prerequisite for a future persistence phase.'
