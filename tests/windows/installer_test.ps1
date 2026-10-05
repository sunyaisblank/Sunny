$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Join-Path $env:TEMP ('Sunny Installer Test ' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$passed = [Collections.Generic.List[string]]::new()
function Check([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Fails($Run, [string]$Pattern) {
    $failed = $false
    try { & $Run | Out-Null } catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        $failed = $true
    }
    Check $failed ('Expected failure: ' + $Pattern)
}
function WriteText([string]$Path, [string]$Value) { [IO.File]::WriteAllText($Path, $Value, [Text.UTF8Encoding]::new($false)) }
function NewRelease([string]$Name, [string]$Body, [string]$Digit) {
    $directory = Join-Path $root $Name
    $bridge = Join-Path $directory 'native\Sunny'
    [IO.Directory]::CreateDirectory($bridge) | Out-Null
    WriteText (Join-Path $bridge '__init__.py') $Body
    WriteText (Join-Path $bridge 'bridge_contract.json') '{"bridge_protocol_version":47,"target_snapshot_schema_version":35}'
    WriteText (Join-Path $bridge 'source.sha256') ($Digit * 64)
    WriteText (Join-Path $directory 'image.tar') ('fixture archive ' + $Name)
    $files = [ordered]@{}
    $payload = @()
    foreach ($file in Get-ChildItem -LiteralPath $bridge -File) {
        $entry = [ordered]@{ sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant(); bytes = $file.Length }
        $files[$file.Name] = $entry
        $payload += [ordered]@{ path = ('native/Sunny/' + $file.Name); sha256 = $entry.sha256; bytes = $entry.bytes }
    }
    $archive = Get-Item -LiteralPath (Join-Path $directory 'image.tar')
    $payload += [ordered]@{ path = 'image.tar'; sha256 = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash.ToLowerInvariant(); bytes = $archive.Length }
    $operators = Join-Path $directory 'installer\windows'
    [IO.Directory]::CreateDirectory($operators) | Out-Null
    foreach ($name in @('Sunny.ps1', 'SunnyHost.ps1', 'SunnyRemote.ps1')) {
        Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $operators $name)
        $operatorFile = Get-Item -LiteralPath (Join-Path $operators $name)
        $payload += [ordered]@{path=('installer/windows/' + $name);sha256=(Get-FileHash -LiteralPath $operatorFile.FullName -Algorithm SHA256).Hash.ToLowerInvariant();bytes=$operatorFile.Length}
    }
    $manifest = [ordered]@{
        release_manifest_schema_version = 1; product = @{ name = 'Sunny'; version = 'fixture' }
        source = @{ revision = ($Digit * 40); source_date_epoch = 1 }
        image = @{ platform = 'linux/amd64'; local_immutable_id = ('sha256:' + ($Digit * 64)); archive = 'image.tar' }
        bridge = @{ source_sha256 = ($Digit * 64); contract = @{ bridge_protocol_version = 47; target_snapshot_schema_version = 35 }; files = $files }
        files = $payload
    }
    $manifestPath = Join-Path $directory 'release.json'
    WriteText $manifestPath ($manifest | ConvertTo-Json -Depth 20)
    return @{ root = $directory; hash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant() }
}
function HealthFixture($Paths, [string]$Kind = 'valid') {
    $state = Read-SunnyState $Paths
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0
    $request = [Guid]::NewGuid().ToString('N')
    $identity = @{ protocol_version = 47; source_sha256 = $state.active.bridge_source_sha256 }
    $doctor = @{
        schema_version = 1; request_id = $request; observed_at = $now
        success = $true; read_only_ready = $true
        expected_bridge = $identity; observed_bridge = $identity
        session = @{ schema_version = 1; bridge_instance = ('d' * 32); document_token = ('e' * 32) }
        native_state = @{ is_playing = $false; session_record = $false; record_mode = $false }
        checks = @(@('bridge_connection', 'release_pairing', 'native_session', 'native_readiness') | ForEach-Object { @{layer = $_; status = 'pass'} })
    }
    $report = @{
        schema_version = 1; request_id = $request; observed_at = $now
        success = $true; read_only_ready = $true; doctor = $doctor
        cleanup = @{ success = $true }
        checks = @(@('client_launch', 'stdio', 'mcp_tools') | ForEach-Object { @{layer = $_; status = 'pass'} })
    }
    if ($Kind -eq 'stale') { $doctor.observed_at = $now - 600 }
    if ($Kind -eq 'future') { $doctor.observed_at = $now + 600 }
    if ($Kind -eq 'mismatch') { $doctor.observed_bridge = @{ protocol_version = 47; source_sha256 = ('f' * 64) } }
    if ($Kind -eq 'uncorrelated') { $doctor.request_id = ('f' * 32) }
    if ($Kind -eq 'incomplete') { $doctor.checks = @($doctor.checks[0]) }
    if ($Kind -eq 'boolean') { $doctor.native_state.record_mode = 'false' }
    if ($Kind -eq 'cleanup_failed') { $report.cleanup.success = $false }
    if ($Kind -eq 'cleanup_missing') { $report.Remove('cleanup') }
    if ($Kind -eq 'cleanup_boolean') { $report.cleanup.success = 'true' }
    $path = Join-Path $root ('doctor-fixture-' + $Kind + '.json')
    WriteText $path ($report | ConvertTo-Json -Depth 20)
    return $path
}
try {
    $script = Join-Path $root 'Sunny.ps1'
    [IO.File]::WriteAllBytes($script, [Convert]::FromBase64String('__SOURCE__'))
    . $script
    $hostScript = Join-Path $root 'SunnyHost.ps1'
    [IO.File]::WriteAllBytes($hostScript, [Convert]::FromBase64String('__HOST_SOURCE__'))
    . $hostScript
    $remoteScript = Join-Path $root 'SunnyRemote.ps1'
    [IO.File]::WriteAllBytes($remoteScript, [Convert]::FromBase64String('__REMOTE_SOURCE__'))
    . $remoteScript
    $library = Join-Path $root 'Different User Library with spaces'
    [IO.Directory]::CreateDirectory((Join-Path $library 'Remote Scripts\Unrelated')) | Out-Null
    WriteText (Join-Path $library 'user-set.als') 'unsaved-work-fixture'
    WriteText (Join-Path $library 'Remote Scripts\Unrelated\__init__.py') '# unrelated'
    $a = NewRelease 'release-a' '# original' 'a'
    $b = NewRelease 'release-b' '# updated' 'b'
    $same = NewRelease 'release-same-bridge' '# original' 'a'
    $c = NewRelease 'release-c' '# third' 'c'
    $plan = Invoke-SunnyLifecycle 'Plan' $library $a.root $a.hash
    Check ($plan.active_files_valid -and -not (Test-Path -LiteralPath $plan.managed_directory)) 'Plan changed the installation'
    Fails { Invoke-SunnyLifecycle 'Install' $library $a.root ('0' * 64) } 'manifest checksum'
    Check (-not (Test-Path -LiteralPath $plan.managed_directory)) 'Failed verification changed the installation'
    $null = Invoke-SunnyLifecycle 'Install' $library $a.root $a.hash
    $paths = Get-SunnyPaths $library
    $state = Read-SunnyState $paths
    Check ($state.active.manifest_sha256 -ceq $a.hash -and $null -eq $state.previous -and $null -eq $state.pending) 'Fresh install state'
    $repeat = Invoke-SunnyLifecycle 'Install' $library $a.root $a.hash
    Check (-not $repeat.changed -and $null -eq (Read-SunnyState $paths).previous) 'Repeat installation made a backup'
    $passed.Add('verified_plan_fresh_repeat')

    # Corrupt a transfer source after manifest creation: it cannot replace the active release.
    WriteText (Join-Path $b.root 'image.tar') 'changed'
    Fails { Invoke-SunnyLifecycle 'Install' $library $b.root $b.hash } 'payload checksum'
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# original') 'Corrupt release replaced active'
    $b = NewRelease 'release-b' '# updated' 'b'
    $passed.Add('interrupted_transfer_checksum')

    $script:copyCount = 0
    function Copy-Item([string]$LiteralPath, [string]$Destination) {
        $script:copyCount++
        if ($script:copyCount -eq 2) {
            [IO.File]::WriteAllBytes($Destination, [byte[]]@(35))
            throw [IO.IOException]::new('injected copy failure: no space')
        }
        Microsoft.PowerShell.Management\Copy-Item -LiteralPath $LiteralPath -Destination $Destination
    }
    Fails { Invoke-SunnyLifecycle 'Install' $library $b.root $b.hash } 'injected copy failure'
    Remove-Item Function:\Copy-Item
    Check ((Read-SunnyState $paths).pending.phase -ceq 'prepared') 'Partial copy was not journalled'
    $null = Invoke-SunnyLifecycle 'Recover' $library $null $null
    Check (-not (Test-Path -LiteralPath $paths.stage)) 'Partial stage was not removed'
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# original') 'Copy failure damaged active'
    $passed.Add('partial_copy_recovery')

    # Same native bytes but a different release manifest: recovery must not delete the old active folder.
    $originalMove = ${function:Move-SunnyDirectory}
    function Move-SunnyDirectory([string]$From, [string]$To) { throw 'injected before rename' }
    Fails { Invoke-SunnyLifecycle 'Install' $library $same.root $same.hash } 'injected before rename'
    Set-Item Function:\Move-SunnyDirectory $originalMove
    $null = Invoke-SunnyLifecycle 'Recover' $library $null $null
    Check ((Read-SunnyState $paths).active.manifest_sha256 -ceq $a.hash) 'Identical-byte recovery lost original release identity'
    $passed.Add('identical_bridge_pre_activation_recovery')

    $script:moveCount = 0
    function Move-SunnyDirectory([string]$From, [string]$To) {
        $script:moveCount++
        if ($script:moveCount -eq 2) { throw 'injected after old active rename' }
        & $originalMove $From $To
    }
    Fails { Invoke-SunnyLifecycle 'Install' $library $b.root $b.hash } 'injected after old active rename'
    Set-Item Function:\Move-SunnyDirectory $originalMove
    Check ((Test-Path -LiteralPath $paths.before_active) -and -not (Test-Path -LiteralPath $paths.active)) 'Rename fault did not hit the real gap'
    $null = Invoke-SunnyLifecycle 'Recover' $library $null $null
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# original') 'Old active was not restored'
    $passed.Add('rename_gap_recovery')

    $originalWrite = ${function:Write-SunnyState}
    function Write-SunnyState([string]$Path, $State) {
        if ($State.pending -and $State.pending.phase -ceq 'committed') { throw 'injected before state commit' }
        & $originalWrite $Path $State
    }
    Fails { Invoke-SunnyLifecycle 'Install' $library $b.root $b.hash } 'injected before state commit'
    Set-Item Function:\Write-SunnyState $originalWrite
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# updated') 'Commit fault did not activate fixture'
    $null = Invoke-SunnyLifecycle 'Recover' $library $null $null
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# original') 'Pre-commit recovery did not restore old bytes'
    $passed.Add('activated_uncommitted_recovery')

    # A committed update interrupted during backup finalization must roll forward.
    $script:moveCount = 0
    function Move-SunnyDirectory([string]$From, [string]$To) {
        $script:moveCount++
        if ($script:moveCount -eq 3) { throw 'injected after state commit' }
        & $originalMove $From $To
    }
    Fails { Invoke-SunnyLifecycle 'Install' $library $b.root $b.hash } 'injected after state commit'
    Set-Item Function:\Move-SunnyDirectory $originalMove
    Check ((Read-SunnyState $paths).pending.phase -ceq 'committed') 'Post-commit fault did not persist its phase'
    $null = Invoke-SunnyLifecycle 'Recover' $library $null $null
    Check ((Read-SunnyState $paths).active.manifest_sha256 -ceq $b.hash) 'Committed update was rolled back'
    Check ((Get-Content -LiteralPath (Join-Path $paths.previous '__init__.py') -Raw) -ceq '# original') 'Original backup unavailable'
    $passed.Add('committed_update_recovery')

    $null = Invoke-SunnyLifecycle 'Rollback' $library $null $null
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# original') 'Rollback failed'
    $null = Invoke-SunnyLifecycle 'Rollback' $library $null $null
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# updated') 'Rollback of rollback failed'
    Fails { Invoke-SunnyLifecycle 'Install' $library $c.root $c.hash } 'Confirm the active release'
    $unchanged = (Get-FileHash -LiteralPath $paths.state -Algorithm SHA256).Hash
    foreach ($kind in @('stale', 'future', 'mismatch', 'uncorrelated', 'incomplete', 'boolean', 'cleanup_failed', 'cleanup_missing', 'cleanup_boolean')) {
        $report = HealthFixture $paths $kind
        Fails { Invoke-SunnyLifecycle 'Confirm' $library $null $null $report } 'Doctor'
        Check ((Get-FileHash -LiteralPath $paths.state -Algorithm SHA256).Hash -ceq $unchanged) 'Invalid health evidence changed managed state'
    }
    $report = HealthFixture $paths
    $originalClosed = ${function:Assert-SunnyLiveClosed}
    function Assert-SunnyLiveClosed { throw 'Model Live is open during health confirmation' }
    $null = Invoke-SunnyLifecycle 'Confirm' $library $null $null $report
    Set-Item Function:\Assert-SunnyLiveClosed $originalClosed
    Check ((Read-SunnyState $paths).native_health -ceq 'paired_read_only_ready') 'Valid modeled doctor not recorded'
    $passed.Add('backup_retirement_requires_fresh_paired_health')

    # Interrupt deletion during prepared rollback after retiring the new public tree.
    function Write-SunnyState([string]$Path, $State) {
        if ($State.pending -and $State.pending.phase -ceq 'committed') { throw 'injected before state commit' }
        & $originalWrite $Path $State
    }
    Fails { Invoke-SunnyLifecycle 'Install' $library $c.root $c.hash } 'injected before state commit'
    Set-Item Function:\Write-SunnyState $originalWrite
    $originalRemove = ${function:Remove-SunnyTree}
    function Remove-SunnyTree([string]$Path, $Metadata, [switch]$Partial) {
        if ($Path -eq $paths.stage) {
            Remove-Item -LiteralPath (Join-Path $Path '__init__.py')
            throw 'injected partial cleanup'
        }
        & $originalRemove $Path $Metadata -Partial:$Partial
    }
    Fails { Invoke-SunnyLifecycle 'Recover' $library $null $null } 'injected partial cleanup'
    Set-Item Function:\Remove-SunnyTree $originalRemove
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# updated') 'Cleanup failure left public active absent'
    $null = Invoke-SunnyLifecycle 'Recover' $library $null $null
    Check ($null -eq (Read-SunnyState $paths).pending -and -not (Test-Path -LiteralPath $paths.stage)) 'Prepared cleanup did not resume'
    $passed.Add('partial_prepared_cleanup_resumes')

    # Interrupt retirement of the older backup after state commit, then roll forward.
    function Remove-SunnyTree([string]$Path, $Metadata, [switch]$Partial) {
        if ($Path -eq $paths.cleanup_old) {
            Remove-Item -LiteralPath (Join-Path $Path '__init__.py')
            throw 'injected partial retirement'
        }
        & $originalRemove $Path $Metadata -Partial:$Partial
    }
    Fails { Invoke-SunnyLifecycle 'Install' $library $c.root $c.hash } 'injected partial retirement'
    Set-Item Function:\Remove-SunnyTree $originalRemove
    Check ((Read-SunnyState $paths).pending.phase -ceq 'committed') 'Retirement fault lost committed phase'
    $null = Invoke-SunnyLifecycle 'Recover' $library $null $null
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# third') 'Committed cleanup did not retain new active'
    Check ($null -eq (Read-SunnyState $paths).pending -and -not (Test-Path -LiteralPath $paths.cleanup_old)) 'Committed cleanup did not resume'
    $null = Invoke-SunnyLifecycle 'Rollback' $library $null $null
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# updated') 'Previous release lost in cleanup'
    $passed.Add('partial_committed_retirement_resumes')

    $previousBody = Get-Content -LiteralPath (Join-Path $paths.previous '__init__.py') -Raw
    WriteText (Join-Path $paths.previous '__init__.py') '# modified backup'
    Fails { Invoke-SunnyLifecycle 'Rollback' $library $null $null } 'Managed file changed'
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# updated') 'Failed rollback damaged active'
    WriteText (Join-Path $paths.previous '__init__.py') $previousBody
    $passed.Add('rollback_and_failed_rollback')

    WriteText (Join-Path $paths.active 'user-file.txt') 'retain me'
    Fails { Invoke-SunnyLifecycle 'Uninstall' $library $null $null } 'Unmanaged file'
    Check ((Get-Content -LiteralPath (Join-Path $paths.active 'user-file.txt') -Raw) -ceq 'retain me') 'Unmanaged content lost'
    Remove-Item -LiteralPath (Join-Path $paths.active 'user-file.txt')
    $held = [IO.File]::Open($paths.lock, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { Fails { Invoke-SunnyLifecycle 'Uninstall' $library $null $null } 'being used by another process' }
    finally { $held.Dispose() }
    $passed.Add('unmanaged_content_and_concurrent_operation_refused')

    $null = Invoke-SunnyLifecycle 'Uninstall' $library $null $null
    $repeat = Invoke-SunnyLifecycle 'Uninstall' $library $null $null
    Check (-not $repeat.changed -and -not (Test-Path -LiteralPath $paths.active)) 'Repeated uninstall failed'
    Check (Test-Path -LiteralPath $repeat.retained_backup) 'Uninstall failed to report retained backup'
    Check ((Get-Content -LiteralPath (Join-Path $library 'user-set.als') -Raw) -ceq 'unsaved-work-fixture') 'User Set changed'
    Check ((Get-Content -LiteralPath (Join-Path $library 'Remote Scripts\Unrelated\__init__.py') -Raw) -ceq '# unrelated') 'Unrelated script changed'
    $passed.Add('uninstall_retains_backup_and_user_data')

    $null = Invoke-SunnyLifecycle 'Install' $library $a.root $a.hash
    Check ((Get-Content -LiteralPath (Join-Path $paths.previous '__init__.py') -Raw) -ceq '# updated') 'Reinstall deleted the retained backup'
    $null = Invoke-SunnyLifecycle 'Rollback' $library $null $null
    Check ((Get-Content -LiteralPath (Join-Path $paths.active '__init__.py') -Raw) -ceq '# updated') 'Reinstalled release could not roll back'
    [IO.Directory]::CreateDirectory($paths.stage) | Out-Null
    WriteText (Join-Path $paths.stage '__init__.py') '# original'
    Fails { Invoke-SunnyLifecycle 'Install' $library $a.root $a.hash } 'unrecorded staging'
    Check ((Get-Content -LiteralPath (Join-Path $paths.stage '__init__.py') -Raw) -ceq '# original') 'Unrecorded stage was adopted or deleted'
    Check ($null -eq (Read-SunnyState $paths).pending) 'Unrecorded stage acquired journal ownership'
    Remove-Item -LiteralPath $paths.stage -Recurse
    $passed.Add('reinstall_preserves_backup_and_refuses_unrecorded_stage')

    $state = Read-SunnyState $paths
    $state.owner_sid = 'S-1-0-0'
    Write-SunnyState $paths.state $state
    Fails { Invoke-SunnyLifecycle 'Status' $library $null $null } 'another user'
    $passed.Add('owner_mismatch_refused')
    $foreign = Join-Path $root 'Other Library'
    $foreignManaged = Join-Path $foreign 'Remote Scripts\.sunny-managed'
    [IO.Directory]::CreateDirectory($foreignManaged) | Out-Null
    $foreignFile = Join-Path $foreignManaged 'state.json.writing'
    WriteText $foreignFile 'unrelated content'
    Fails { Invoke-SunnyLifecycle 'Install' $foreign $a.root $a.hash } 'ownership record was preserved'
    Check ((Get-Content -LiteralPath $foreignFile -Raw) -ceq 'unrelated content') 'Unregistered ownership file was overwritten'
    $passed.Add('unregistered_initial_content_preserved')

    $logDirectory = Join-Path $root 'Independent host logs with spaces'
    [IO.Directory]::CreateDirectory($logDirectory) | Out-Null
    $log = Join-Path $logDirectory 'Log.txt'
    WriteText $log "Project PRIVATE-SET`nSunny error password=DO-NOT-EXPORT C:\private\PRIVATE-SET.als`nTraceback native script loading`n"
    $hash = (Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash
    $request = ('a' * 32)
    $metadata = Invoke-SunnyHost 'Log' $log 1024 $false $request
    Check ($metadata.request_id -ceq $request -and $metadata.read_only -and -not $metadata.native_bridge_required) 'Host log correlation or effect classification failed'
    Check (-not $metadata.log.Contains('messages') -and $metadata.log.matching_line_counts.sunny -eq 1 -and $metadata.log.matching_line_counts.tracebacks -eq 1) 'Default host log collected messages or lost independent counts'
    Check (-not $metadata.log.incomplete -and -not $metadata.log.truncated) 'Complete host log was labeled incomplete'
    $content = Invoke-SunnyHost 'Log' $log 1024 $true $request
    $json = $content | ConvertTo-Json -Depth 12
    Check ($content.log.project_content_may_remain -and $content.log.messages.Count -eq 2) 'Explicit host log content scope failed'
    Check (-not $json.Contains('DO-NOT-EXPORT') -and -not $json.Contains('PRIVATE-SET')) 'Host log failed credential/path minimization'
    Check ((Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash -ceq $hash) 'Read-only host log changed file bytes'
    WriteText $log (('Old Sunny entry' + "`n") * 150 + "Sunny final marker`n")
    $tail = Invoke-SunnyHost 'Log' $log 1024 $true $request
    Check ($tail.log.truncated -and $tail.log.read_bytes -le 1024 -and $tail.log.range_start -gt 0) 'Host log tail was not bounded'
    Check ($tail.log.messages[-1] -ceq 'Sunny final marker') 'Host log tail lost newest independent marker'
    [IO.File]::AppendAllText($log, 'Sunny unfinished')
    $partial = Invoke-SunnyHost 'Log' $log 1024 $true $request
    Check ($partial.log.incomplete -and $partial.log.messages[-1] -ceq 'Sunny final marker') 'Incomplete host log line was emitted as complete'
    $completeBytes = [Text.UTF8Encoding]::new($false).GetBytes("Sunny complete evidence`n")
    [IO.File]::WriteAllBytes($log, [byte[]]($completeBytes + [byte[]]@(226, 130)))
    $partialCodepoint = Invoke-SunnyHost 'Log' $log 1024 $true $request
    Check ($partialCodepoint.log.incomplete -and $partialCodepoint.log.messages.Count -eq 1 -and $partialCodepoint.log.messages[0] -ceq 'Sunny complete evidence') 'Partial UTF-8 codepoint discarded complete preceding evidence'
    Check ($partialCodepoint.log.range_end -eq $completeBytes.Length -and $partialCodepoint.log.read_bytes -eq $completeBytes.Length + 2) 'Partial UTF-8 bytes were reported as complete evidence'
    Fails { Invoke-SunnyHost 'Log' $log 65537 $false $request } 'byte limit'
    Fails { Invoke-SunnyHost 'Log' $log 1024 $false 'invalid' } 'RequestId'
    Fails { Invoke-SunnyHost 'Log' (Join-Path $logDirectory 'other.txt') 1024 $false $request } 'other filenames'
    [IO.File]::WriteAllBytes($log, [byte[]]@(255, 10))
    Fails { Invoke-SunnyHost 'Log' $log 1024 $false $request } 'UTF-8'
    $originalLog = Join-Path $logDirectory 'Original.txt'
    [IO.File]::Move($log, $originalLog)
    New-Item -ItemType HardLink -Path $log -Target $originalLog | Out-Null
    Fails { Invoke-SunnyHost 'Log' $log 1024 $false $request } 'hard links'
    $passed.Add('independent_bounded_read_only_host_log')

    $identity = Join-Path $root 'external identity file'
    $knownHosts = Join-Path $root 'external known hosts'
    WriteText $identity 'FIXTURE ONLY - NOT A KEY'
    WriteText $knownHosts 'FIXTURE ONLY - NOT APPROVED TRUST'
    $connection = @{connection_schema_version=1;host='localhost';port=22222;user='fixture';identity_file=$identity;known_hosts_file=$knownHosts}
    $connectionFile = Join-Path $root 'connection.json'
    $connectionText = $connection | ConvertTo-Json -Compress
    WriteText $connectionFile $connectionText
    $keyHash = (Get-FileHash -LiteralPath $identity -Algorithm SHA256).Hash
    $trustHash = (Get-FileHash -LiteralPath $knownHosts -Algorithm SHA256).Hash
    $plan = Invoke-SunnyRemote 'Plan' $connectionFile $a.root $a.hash $null $null $null $null $false 5 $request
    Check (-not $plan.remote_action_executed -and -not $plan.security_settings_changed -and $plan.host -ceq 'localhost') 'Remote plan performed an action or changed endpoint'
    foreach ($key in @('host', 'h\u006fst')) {
        WriteText $connectionFile ('{"' + $key + '":"localhost",' + $connectionText.Substring(1))
        Fails { Read-SunnyConnection $connectionFile } 'duplicate'
    }
    WriteText $connectionFile ($connectionText -replace '"connection_schema_version":1', '"connection_schema_version":2')
    Fails { Read-SunnyConnection $connectionFile } 'Unsupported connection schema'
    WriteText $connectionFile ($connectionText.Substring(0, $connectionText.Length - 1) + ',}')
    Fails { Read-SunnyConnection $connectionFile } 'trailing comma'
    WriteText $connectionFile $connectionText
    Check ((Get-FileHash -LiteralPath $identity -Algorithm SHA256).Hash -ceq $keyHash -and (Get-FileHash -LiteralPath $knownHosts -Algorithm SHA256).Hash -ceq $trustHash) 'Remote plan changed keys or trust'
    Fails { New-SunnyRemotePayload 'ArbitraryCommand' @{} '' '' } 'Unsupported remote operation'
    $safeDirectory = Join-Path $root "Quoted' host log Ω"
    [IO.Directory]::CreateDirectory($safeDirectory) | Out-Null
    $safeLog = Join-Path $safeDirectory 'Log.txt'
    WriteText $safeLog "Sunny error literal Ω`n"
    $inputData = @{operation='Log';request_id=$request;log_path=$safeLog;include_messages=$true}
    $payload = New-SunnyRemotePayload 'Log' $inputData ([IO.File]::ReadAllText($script)) ([IO.File]::ReadAllText($hostScript))
    Check (-not ($payload -match '[^\x00-\x7f]')) 'Remote source transport is not ASCII-only'
    $starter = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes('[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); Invoke-Expression ([Console]::In.ReadToEnd())'))
    $originalInputEncoding = [Console]::InputEncoding
    try {
        [Console]::InputEncoding = [Text.UTF8Encoding]::new($true)
        $bomInputEncoding = [Console]::InputEncoding
        $exchange = Invoke-SunnyBoundedProcess (Get-Command powershell.exe).Source @('-NoProfile', '-NonInteractive', '-EncodedCommand', $starter) $payload 10
        Check ($bomInputEncoding.Equals([Console]::InputEncoding)) 'Remote startup changed the caller input Encoding'

        # Use an independent raw-byte peer to observe the actual child pipe,
        # rather than trusting the writer's encoding metadata alone.
        $peer = @'
$s=[Console]::OpenStandardInput();$b=[byte[]]::new(8);$p=0
while($p-lt8){$t=$s.ReadAsync($b,$p,8-$p);if(-not$t.Wait(5000)){throw 'Prefix deadline'};$n=$t.GetAwaiter().GetResult();if($n-le0){throw 'Prefix ended'};$p+=$n}
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
[Console]::Out.WriteLine([BitConverter]::ToString($b))
'@
        $peerEncoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($peer))
        foreach ($encoding in @([Text.Encoding]::GetEncoding(437), [Text.UTF8Encoding]::new($true), [Text.UTF8Encoding]::new($false))) {
            [Console]::InputEncoding = $encoding
            $expectedInputEncoding = [Console]::InputEncoding
            $pipeStart = [Diagnostics.ProcessStartInfo]::new()
            $pipeStart.FileName = (Get-Command powershell.exe).Source
            $pipeStart.Arguments = '-NoProfile -NonInteractive -EncodedCommand ' + $peerEncoded
            $pipeStart.UseShellExecute = $false
            $pipeStart.RedirectStandardInput = $true; $pipeStart.RedirectStandardOutput = $true; $pipeStart.RedirectStandardError = $true
            $pipeStart.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
            $pipeChild = Start-SunnyUtf8PipeProcess $pipeStart
            try {
                Check ($expectedInputEncoding.Equals([Console]::InputEncoding)) 'Successful spawn changed the caller input Encoding'
                Check ($pipeChild.StandardInput.Encoding.CodePage -eq 65001 -and $pipeChild.StandardInput.Encoding.GetPreamble().Length -eq 0) 'Producer writer can prepend a BOM'
                $prefix = [byte[]]@(0x7a, 0x68, 1, 0, 0x7b, 0x22, 0x61, 0x22)
                $write = $pipeChild.StandardInput.BaseStream.WriteAsync($prefix, 0, $prefix.Length)
                Check ($write.Wait(5000)) 'Independent prefix write exceeded its deadline'
                $null = $write.GetAwaiter().GetResult(); $pipeChild.StandardInput.Close()
                Check ($pipeChild.WaitForExit(7000)) 'Independent prefix peer did not exit'
                Check ($pipeChild.ExitCode -eq 0 -and $pipeChild.StandardOutput.ReadToEnd().Trim() -ceq '7A-68-01-00-7B-22-61-22') 'Producer altered the actual first frame bytes'
            } finally {
                if (-not $pipeChild.HasExited) { $pipeChild.Kill(); $null = $pipeChild.WaitForExit(1000) }
                $pipeChild.Dispose()
            }
        }
        [Console]::InputEncoding = [Text.UTF8Encoding]::new($true)
        $failedStartEncoding = [Console]::InputEncoding
        $missingStart = [Diagnostics.ProcessStartInfo]::new()
        $missingStart.FileName = Join-Path $root 'no-such-owned-pipe-peer.exe'
        $missingStart.UseShellExecute = $false; $missingStart.RedirectStandardInput = $true
        $failedStart = $false
        try { $null = Start-SunnyUtf8PipeProcess $missingStart } catch { $failedStart = $true }
        Check ($failedStart -and $failedStartEncoding.Equals([Console]::InputEncoding)) 'Failed startup did not restore the caller input Encoding'

        # Force an actual strict UTF-8 input write failure at the shared GetResult
        # boundary; local Framework pipes may silently accept writes after exit.
        # The literal peer independently emits its bootstrap refusal on stderr.
        $declineCode = '[Console]::Error.WriteLine("SUNNY_PIPE_BOOTSTRAP_DECLINED"); exit 19'
        $declineEncoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($declineCode))
        $invalidInput = ('x' * 1048576) + [char]0xd800 + 'x'
        $declined = $false
        try {
            $null = Invoke-SunnyBoundedProcess (Get-Command powershell.exe).Source @('-NoProfile', '-NonInteractive', '-EncodedCommand', $declineEncoded) $invalidInput 5
        } catch {
            $declined = $true
            Check ($_.Exception.Message -match '(?s)outcome unknown.*Owned child PID=.*exit=19.*input_codepage=65001.*SUNNY_PIPE_BOOTSTRAP_DECLINED') 'Failed input concealed the owned child bootstrap refusal'
            Check ($_.Exception.Message.Length -lt 10000) 'Failed input diagnostic is unbounded'
        }
        Check $declined 'Declining child unexpectedly accepted input'
    } finally { [Console]::InputEncoding = $originalInputEncoding }
    Check ($originalInputEncoding.Equals([Console]::InputEncoding)) 'Pipe witnesses did not restore the original input Encoding'
    Check ($exchange.exit_code -eq 0) ('Local literal remote payload failed: ' + $exchange.stderr)
    $reply = $exchange.stdout | ConvertFrom-Json
    Check ($reply.request_id -ceq $request -and $reply.log.messages[0] -ceq 'Sunny error literal Ω') 'Remote data quoting, Unicode, or correlation failed'
    Assert-SunnyRemoteReport $reply 'Log' $request 15
    $observed = $reply.observed_at
    $reply.operation = 'install'
    Fails { Assert-SunnyRemoteReport $reply 'Log' $request 15 } 'uncorrelated'
    $reply.operation = 'log'
    $reply.observed_at = $observed - 30
    Fails { Assert-SunnyRemoteReport $reply 'Log' $request 15 } 'stale'
    $reply.observed_at = $observed
    $reply.schema_version = $true
    Fails { Assert-SunnyRemoteReport $reply 'Log' $request 15 } 'malformed'
    $reply.schema_version = 1
    $reply.read_only = 'true'
    Fails { Assert-SunnyRemoteReport $reply 'Log' $request 15 } 'effect classification'
    $argumentScript = Join-Path $root 'Arguments with spaces.ps1'
    WriteText $argumentScript 'param([string]$One,[string]$Two) [Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); @{one=$One;two=$Two} | ConvertTo-Json -Compress'
    $one = 'spaces and a"quote\'
    $two = "other' value Ω\\"
    $quoted = Invoke-SunnyBoundedProcess (Get-Command powershell.exe).Source @('-NoProfile', '-NonInteractive', '-File', $argumentScript, '-One', $one, '-Two', $two) '' 10
    Check ($quoted.exit_code -eq 0) ('Windows argument parser failed: ' + $quoted.stderr)
    $parsed = $quoted.stdout | ConvertFrom-Json
    Check ($parsed.one -ceq $one -and $parsed.two -ceq $two) 'Windows argument roundtrip changed caller data'
    $passed.Add('strict_remote_plan_and_literal_local_payload')
    @{ passed = @($passed); native_live_executed = $false; root = $root } | ConvertTo-Json -Compress
} finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
