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
    WriteText (Join-Path $bridge 'bridge_contract.json') '{"bridge_protocol_version":46,"target_snapshot_schema_version":35}'
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
    $manifest = [ordered]@{
        release_manifest_schema_version = 1; product = @{ name = 'Sunny'; version = 'fixture' }
        source = @{ revision = ($Digit * 40); source_date_epoch = 1 }
        image = @{ platform = 'linux/amd64'; local_immutable_id = ('sha256:' + ($Digit * 64)); archive = 'image.tar' }
        bridge = @{ source_sha256 = ($Digit * 64); contract = @{ bridge_protocol_version = 46; target_snapshot_schema_version = 35 }; files = $files }
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
    $identity = @{ protocol_version = 46; source_sha256 = $state.active.bridge_source_sha256 }
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
    if ($Kind -eq 'mismatch') { $doctor.observed_bridge = @{ protocol_version = 46; source_sha256 = ('f' * 64) } }
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
    @{ passed = @($passed); native_live_executed = $false; root = $root } | ConvertTo-Json -Compress
} finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
