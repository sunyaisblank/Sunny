$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Join-Path ([IO.Path]::GetTempPath()) ('Sunny-client-fixture-' + [Guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($root)
function Check($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Refuses($Body, [string]$Message) {
    try { & $Body | Out-Null } catch { Check ($_.Exception.Message.Contains($Message)) $_.Exception.Message; return }
    throw ('Expected refusal: ' + $Message)
}
try {
    foreach ($pair in @(@('Sunny.ps1', '__INSTALLER__'), @('SunnyRemote.ps1', '__REMOTE__'), @('SunnyClient.ps1', '__CLIENT__'))) {
        [IO.File]::WriteAllBytes((Join-Path $root $pair[0]), [Convert]::FromBase64String($pair[1]))
    }
    . (Join-Path $root 'Sunny.ps1')
    . (Join-Path $root 'SunnyRemote.ps1')
    . (Join-Path $root 'SunnyClient.ps1')
    $docker = (Get-Command docker.exe).Source
    $context = 'desktop-linux'
    $daemon = Get-SunnyClientDaemon $docker $context
    Check ([bool]$daemon) 'Actual local Docker Desktop identity unavailable'
    $image = '__IMAGE__'
    $owner = [Guid]::NewGuid().ToString('N')
    $otherOwner = [Guid]::NewGuid().ToString('N')
    $owned = ''; $unrelated = ''; $wrongName = ''; $job = $null; $stdio = $null
    try {
        $owned = Invoke-SunnyClientDocker $docker $context @('create', '--network', 'none', '--name', ('sunny-client-' + $owner + '-validate'), '--label', ('org.sunny.client.owner=' + $owner), $image)
        $unrelated = Invoke-SunnyClientDocker $docker $context @('create', '--network', 'none', '--name', ('sunny-client-' + $otherOwner + '-validate'), '--label', ('org.sunny.client.owner=' + $otherOwner), $image)
        Remove-SunnyClientContainers $docker $context $daemon $owner 5
        $left = Invoke-SunnyClientDocker $docker $context @('ps', '-aq', '--no-trunc', '--filter', ('id=' + $owned))
        Check (-not $left) 'Owned stopped container survived cleanup'
        $retained = Invoke-SunnyClientDocker $docker $context @('container', 'inspect', '--format', '{{.Id}}', $unrelated)
        Check ($retained -ceq $unrelated) 'Cleanup removed an unrelated container'
        $wrongName = Invoke-SunnyClientDocker $docker $context @('create', '--network', 'none', '--name', ('sunny-client-' + $owner + '-foreign-name'), '--label', ('org.sunny.client.owner=' + $owner), $image)
        Refuses { Remove-SunnyClientContainers $docker $context $daemon $owner 5 } 'ownership could not be confirmed'
        Check ((Invoke-SunnyClientDocker $docker $context @('container', 'inspect', '--format', '{{.Id}}', $wrongName)) -ceq $wrongName) 'Wrong-name container was removed'
        Refuses { Remove-SunnyClientContainers $docker $context 'another-daemon' $owner 5 } 'daemon identity changed'
        $null = Invoke-SunnyClientDocker $docker $context @('container', 'rm', $wrongName)
        $wrongName = ''
        $job = New-SunnyClientJob
        $stdio = Start-SunnyClientStdio $docker @('--context', $context, 'run', '-i', '--rm', '--pull=never', '--name', ('sunny-client-' + $owner + '-run'), '--label', ('org.sunny.client.owner=' + $owner), $image) $job
        Check ($stdio.WaitForExit(15000)) 'Actual inherited MCP stdio process did not finish'
        Check ($stdio.ExitCode -eq 0) 'Actual inherited MCP stdio process failed'
        $stdio.Dispose(); $stdio = $null
        $job.Dispose(); $job = $null
        Remove-SunnyClientContainers $docker $context $daemon $owner 5
        $job = New-SunnyClientJob
        $child = Start-SunnyClientStdio (Get-Command powershell.exe).Source @('-NoProfile', '-NonInteractive', '-Command', 'Start-Sleep -Seconds 30') $job
        try {
            Check (-not $child.HasExited) 'Windows job fixture child did not start'
            $job.Dispose(); $job = $null
            Check ($child.WaitForExit(2000)) 'Closing the private Windows job left its owned child alive'
        } finally { if (-not $child.HasExited) { $child.Kill() }; $child.Dispose() }
        $session = Join-Path ([IO.Path]::GetTempPath()) ('Sunny-client-' + $owner)
        $null = [IO.Directory]::CreateDirectory($session)
        try {
            $parent = Get-Process -Id $PID
            $record = [ordered]@{schema_version=1;owner_sid=(Get-SunnyOwner);owner=$owner;context=$context;daemon_id=$daemon;parent_pid=$PID;parent_started=$parent.StartTime.ToUniversalTime().Ticks.ToString()}
            Write-SunnyState (Join-Path $session 'session.json') $record
            Refuses { Recover-SunnyClient $session 5 } 'still running'
            $record.parent_started = '0'
            Write-SunnyState (Join-Path $session 'session.json') $record
            [IO.File]::WriteAllText((Join-Path $session 'foreign.txt'), 'Preserve this unrelated content')
            Refuses { Recover-SunnyClient $session 5 } 'Unexpected session recovery content'
            Check ([IO.File]::ReadAllText((Join-Path $session 'foreign.txt')) -ceq 'Preserve this unrelated content') 'Recovery changed unregistered data'
            Remove-Item -LiteralPath (Join-Path $session 'foreign.txt')
            $recovered = Recover-SunnyClient $session 5
            Check ($recovered.containers_absent -and -not $recovered.project_volume_removed -and -not (Test-Path -LiteralPath $session)) 'Scoped client recovery failed'
        } finally { if (Test-Path -LiteralPath $session) { Remove-Item -LiteralPath $session -Recurse -Force } }
    } finally {
        if ($null -ne $job) { $job.Dispose() }
        if ($null -ne $stdio) { if (-not $stdio.HasExited) { $stdio.Kill(); $null = $stdio.WaitForExit(1000) }; $stdio.Dispose() }
        foreach ($id in @($wrongName, $unrelated, $owned)) {
            if ($id -cmatch '^[0-9a-f]{64}$') {
                $present = Invoke-SunnyClientDocker $docker $context @('ps', '-aq', '--no-trunc', '--filter', ('id=' + $id))
                if ($present -ceq $id) { $null = Invoke-SunnyClientDocker $docker $context @('container', 'rm', '--force', $id) }
            }
        }
        Remove-SunnyClientContainers $docker $context $daemon $owner 5
    }
    [Console]::Error.WriteLine('SUNNY_CLIENT_FIXTURE_PASS')
} finally { if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force } }
