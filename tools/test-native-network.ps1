param(
    [string]$Preset = 'windows-mingw-debug',
    [ValidateRange(1024,65535)][int]$Port = 27031,
    [ValidateRange(0,1000)][int]$LagMs = 60,
    [ValidateRange(0,50)][int]$LossPercent = 10
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$binaryDirectory = Join-Path $projectRoot "out/build/$Preset/bin"
$serverExe = Join-Path $binaryDirectory 'tchips_server.exe'
$clientExe = Join-Path $binaryDirectory 'tchips_client.exe'
if (-not (Test-Path -LiteralPath $serverExe) -or -not (Test-Path -LiteralPath $clientExe)) {
    throw 'Compile client and server before running the native network scenario'
}
$logDirectory = Join-Path $projectRoot 'out/native-network'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$ownedProcesses = @()
try {
    $server = Start-Process -FilePath $serverExe -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru `
        -ArgumentList @('--port', "$Port", '--ticks', '600', '--lag-ms', "$LagMs", '--loss-percent', "$LossPercent") `
        -RedirectStandardOutput (Join-Path $logDirectory 'server.stdout.log') `
        -RedirectStandardError (Join-Path $logDirectory 'server.stderr.log')
    $ownedProcesses += $server
    $null = $server.Handle
    $readyTimer = [Diagnostics.Stopwatch]::StartNew()
    do {
        if ($server.HasExited) { throw 'Server exited before listening; inspect out/native-network/server.stderr.log' }
        $ready = Test-Path -LiteralPath (Join-Path $logDirectory 'server.stdout.log')
        if ($ready) { $ready = (Get-Content -Raw -LiteralPath (Join-Path $logDirectory 'server.stdout.log')) -match 'UDP port=' }
        if (-not $ready) { Start-Sleep -Milliseconds 100 }
    } while (-not $ready -and $readyTimer.ElapsedMilliseconds -lt 10000)
    if (-not $ready) { throw 'Server readiness timeout' }
    $clients = @()
    foreach ($index in 1,2) {
        $client = Start-Process -FilePath $clientExe -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru `
            -ArgumentList @('--connect', "127.0.0.1:$Port", '--network-smoke', '--lag-ms', "$LagMs", '--loss-percent', "$LossPercent") `
            -RedirectStandardOutput (Join-Path $logDirectory "client$index.stdout.log") `
            -RedirectStandardError (Join-Path $logDirectory "client$index.stderr.log")
        $clients += $client
        $ownedProcesses += $client
        $null = $client.Handle
    }
    foreach ($client in $clients) {
        if (-not $client.WaitForExit(20000)) { throw 'Native client timeout' }
        if ($client.ExitCode -ne 0) { throw "Native client failed with code $($client.ExitCode); inspect out/native-network" }
    }
    foreach ($index in 1,2) {
        $log = Get-Content -Raw -LiteralPath (Join-Path $logDirectory "client$index.stdout.log")
        if ($log -notmatch 'network smoke passed: visible=2, remote-motion=yes, gap=0(?:,|\s|$)') { throw "Client $index did not validate two-player convergence" }
        Write-Output "Client $index : $($log.Trim())"
    }
    if (-not $server.WaitForExit(25000)) { throw 'Bounded server timeout' }
    if ($server.ExitCode -ne 0) { throw "Server failed with code $($server.ExitCode)" }
    $serverLog = Get-Content -Raw -LiteralPath (Join-Path $logDirectory 'server.stdout.log')
    if ($serverLog -notmatch 'active=2' -or $serverLog -notmatch 'active=0') { throw 'Server did not create and clean both players' }
    Write-Output "PASS: two native clients, shared map, interpolation, convergence, clean disconnect; outgoing lag=$LagMs ms, packet loss=$LossPercent percent per process."
} finally {
    # Only processes created by this test are eligible for forced cleanup.
    foreach ($process in $ownedProcesses) {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
    }
}
