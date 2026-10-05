$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
Set-StrictMode -Version Latest
$root=Join-Path $env:TEMP ('Sunny Transfer '+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$passed=[Collections.Generic.List[string]]::new()
function Check([bool]$Value,[string]$Message) { if (-not $Value) { throw $Message } }
function Fails($Run,[string]$Pattern) {
    $failed=$false
    try { & $Run | Out-Null } catch { if ($_.Exception.Message -notmatch $Pattern) { throw };$failed=$true }
    Check $failed ('Expected refusal: '+$Pattern)
}
function WriteText([string]$Path,[string]$Text) { [IO.File]::WriteAllText($Path,$Text,[Text.UTF8Encoding]::new($false)) }
function Frame([IO.Stream]$Stream,[byte[]]$Bytes) { $header=[BitConverter]::GetBytes([uint32]$Bytes.Length);$Stream.Write($header,0,4);$Stream.Write($Bytes,0,$Bytes.Length) }
function RequestStream($Plan,[long]$Maximum=-1) {
    $stream=[IO.MemoryStream]::new()
    Frame $stream ([Text.Encoding]::ASCII.GetBytes([Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(($Plan | ConvertTo-Json -Depth 8 -Compress)))))
    [long]$written=0
    foreach ($file in $Plan.files) {
        $bytes=[IO.File]::ReadAllBytes((Join-Path $source $file.path.Replace('/','\')))
        $count=$bytes.Length
        if ($Maximum -ge 0) { $count=[int][Math]::Min($count,[Math]::Max(0,$Maximum-$written)) }
        $stream.Write($bytes,0,$count);$written += $count
    }
    $stream.Position=0
    return $stream
}
function Receiver($Plan,[long]$Maximum=-1) {
    $stream=RequestStream $Plan $Maximum
    try { Invoke-SunnyTransferReceiver $stream } finally { $stream.Dispose() }
}
function NewPlan([string]$Name,[string]$Operation='Transfer') {
    return New-SunnyTransferPlan $source $manifestHash (Join-Path $root $Name) $Operation
}
try {
    $installerPath=Join-Path $root 'Sunny.ps1'
    $transferPath=Join-Path $root 'SunnyTransfer.ps1'
    $remotePath=Join-Path $root 'SunnyRemote.ps1'
    [IO.File]::WriteAllBytes($installerPath,[Convert]::FromBase64String('__INSTALLER_SOURCE__'))
    [IO.File]::WriteAllBytes($transferPath,[Convert]::FromBase64String('__TRANSFER_SOURCE__'))
    [IO.File]::WriteAllBytes($remotePath,[Convert]::FromBase64String('__REMOTE_SOURCE__'))
    . $installerPath; . $remotePath; . $transferPath
    $source=Join-Path $root 'source release with spaces'
    [IO.Directory]::CreateDirectory((Join-Path $source 'native\Sunny')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $source 'installer\windows')) | Out-Null
    WriteText (Join-Path $source 'native\Sunny\__init__.py') '# literal expected source'
    WriteText (Join-Path $source 'native\Sunny\bridge_contract.json') '{"bridge_protocol_version":47,"target_snapshot_schema_version":35}'
    WriteText (Join-Path $source 'native\Sunny\source.sha256') ('a'*64)
    [byte[]]$binary=[byte[]]::new(2097285)
    for ($i=0;$i -lt $binary.Length;$i++) { $binary[$i]=[byte](($i*37+129)%256) }
    [IO.File]::WriteAllBytes((Join-Path $source 'image.tar'),$binary)
    foreach ($name in @('Sunny.ps1','SunnyRemote.ps1','SunnyTransfer.ps1')) { [IO.File]::Copy((Join-Path $root $name),(Join-Path $source ('installer\windows\'+$name))) }
    $source=(Get-Item -LiteralPath $source).FullName
    $files=@();$bridge=@{}
    foreach ($item in Get-ChildItem -LiteralPath $source -File -Recurse) {
        $name=$item.FullName.Substring($source.Length+1).Replace('\','/')
        $record=@{path=$name;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
        $files += $record
        if ($name.StartsWith('native/Sunny/')) { $bridge[$item.Name]=@{bytes=$item.Length;sha256=$record.sha256} }
    }
    $manifest=@{release_manifest_schema_version=1;product=@{name='Sunny';version='fixture'};source=@{revision=('a'*40)};
        image=@{platform='linux/amd64';local_immutable_id=('sha256:'+('a'*64))};
        bridge=@{source_sha256=('a'*64);contract=@{bridge_protocol_version=47;target_snapshot_schema_version=35};files=$bridge};files=$files}
    WriteText (Join-Path $source 'release.json') ($manifest | ConvertTo-Json -Depth 8)
    $manifestHash=(Get-FileHash -LiteralPath (Join-Path $source 'release.json') -Algorithm SHA256).Hash.ToLowerInvariant()
    $plan=NewPlan 'fresh destination Café with spaces'
    Receiver $plan
    Check ((Get-FileHash -LiteralPath (Join-Path $plan.destination 'image.tar')).Hash.ToLowerInvariant() -ceq (Get-SunnyTransferHash $binary)) 'Binary content changed'
    $journal=$plan.destination+'.sunny-transfer.json'
    $state=Read-SunnyJson $journal
    Check ($state.owner_sid -ceq [Security.Principal.WindowsIdentity]::GetCurrent().User.Value -and $state.phase -ceq 'committed') 'Ownership/commit state missing'
    $before=[IO.File]::ReadAllText($journal)
    $status=NewPlan 'fresh destination Café with spaces' 'Status'
    Receiver $status 0
    Check ([IO.File]::ReadAllText($journal) -ceq $before) 'Status mutated durable state'
    Receiver $plan 0
    Check ([IO.File]::ReadAllText($journal) -ceq $before) 'Same-release rerun mutated state'
    $passed.Add('full_binary_unicode_repeat_read_only_status')

    $partial=NewPlan 'interrupted'
    # End mid-second image chunk: only the first authenticated chunk can be appended.
    Fails { Receiver $partial 1048600 } 'Interrupted transfer'
    Check (Test-Path -LiteralPath ($partial.destination+'.sunny-transfer.json')) 'Interrupted transfer lost ownership journal'
    Check (-not (Test-Path -LiteralPath $partial.destination)) 'Interrupted transfer published final release'
    $partial.operation='Recover'
    Receiver $partial
    Check ((Get-FileHash -LiteralPath (Join-Path $partial.destination 'image.tar')).Hash.ToLowerInvariant() -ceq (Get-SunnyTransferHash $binary)) 'Recovery mismatched literal binary'
    $passed.Add('truncated_chunk_resume_source_prefix')

    $changed=NewPlan 'changed-prefix'
    Fails { Receiver $changed 1048600 } 'Interrupted transfer'
    $changedImage=Join-Path ($changed.destination+'.sunny-stage') 'image.tar'
    $stream=[IO.File]::OpenWrite($changedImage);$stream.WriteByte(7);$stream.Dispose()
    $changedBefore=[IO.File]::ReadAllBytes($changedImage)
    Fails { Receiver $changed } 'Changed staging prefix'
    Check ((Get-SunnyTransferHash ([IO.File]::ReadAllBytes($changedImage))) -ceq (Get-SunnyTransferHash $changedBefore)) 'Changed user bytes overwritten'
    $changed.operation='Clean'
    Fails { Receiver $changed } 'Changed staging prefix'
    Check (Test-Path -LiteralPath $changedImage) 'Changed bytes removed by Clean'
    $passed.Add('changed_prefix_preserved_by_recovery_and_clean')

    $clean=NewPlan 'partial-clean'
    Fails { Receiver $clean 1048600 } 'Interrupted transfer'
    $clean.operation='Clean'; Receiver $clean
    Check (-not (Test-Path -LiteralPath ($clean.destination+'.sunny-stage')) -and -not (Test-Path -LiteralPath ($clean.destination+'.sunny-transfer.json'))) 'Owned partial cleanup incomplete'
    $passed.Add('authenticated_partial_cleanup')

    $foreign=NewPlan 'foreign-destination'
    [IO.Directory]::CreateDirectory($foreign.destination) | Out-Null
    WriteText (Join-Path $foreign.destination 'user.als') 'literal user data'
    Fails { Receiver $foreign } 'unowned'
    Check ([IO.File]::ReadAllText((Join-Path $foreign.destination 'user.als')) -ceq 'literal user data') 'Foreign destination changed'
    $foreignStage=NewPlan 'foreign-stage'
    Fails { Receiver $foreignStage 1048600 } 'Interrupted transfer'
    WriteText (Join-Path ($foreignStage.destination+'.sunny-stage') 'user.als') 'preserve foreign addition'
    Fails { Receiver $foreignStage } 'Foreign file'
    $foreignStage.operation='Clean';Fails { Receiver $foreignStage } 'Foreign file'
    $passed.Add('foreign_destination_and_stage_preserved')

    $move=NewPlan 'move-gap'
    Receiver $move
    $moveState=Read-SunnyJson ($move.destination+'.sunny-transfer.json');$moveState.phase='verified'
    Write-SunnyTransferJournal ($move.destination+'.sunny-transfer.json') $moveState
    $move.operation='Recover';Receiver $move 0
    Check ((Read-SunnyJson ($move.destination+'.sunny-transfer.json')).phase -ceq 'committed') 'Post-move recovery failed'
    $moveState.owner_sid='S-1-0-0';Write-SunnyTransferJournal ($move.destination+'.sunny-transfer.json') $moveState
    Fails { Receiver $move 0 } 'ownership'
    $passed.Add('post_move_recovery_and_owner_mismatch')

    # Invalid control data must be rejected before an ownership journal is created.
    foreach ($mutation in @('future','boolean','unknown')) {
        $invalid=NewPlan ('invalid-'+$mutation)
        if ($mutation -eq 'future') { $invalid.transfer_schema_version=2 }
        elseif ($mutation -eq 'boolean') { $invalid.transfer_schema_version=$true }
        else { $invalid.unrecognized='refuse' }
        Fails { Receiver $invalid 0 } 'integer|unknown'
        Check (-not (Test-Path -LiteralPath ($invalid.destination+'.sunny-transfer.json'))) 'Invalid request created managed state'
    }
    Fails { ConvertFrom-SunnyTransferJson ([Text.Encoding]::UTF8.GetBytes('{"x":1,"\u0078":2}')) } 'Duplicate'
    Fails { ConvertFrom-SunnyTransferJson ([Text.Encoding]::UTF8.GetBytes('{"x":1,"X":2}')) } 'Duplicate'
    $oversized=NewPlan 'over-budget';$oversized.files[0].bytes=[long]4294967297
    Fails { Receiver $oversized 0 } 'bounded JSON integer'
    Check (-not (Test-Path -LiteralPath ($oversized.destination+'.sunny-transfer.json'))) 'Over-budget request wrote ownership state'
    $tooMany=@(for ($entry=0;$entry -lt 513;$entry++) { @{path='file'+$entry;bytes=0;sha256=('a'*64);chunks=@()} })
    Fails { Assert-SunnyTransferFiles $tooMany } '2..512'
    $corrupt=NewPlan 'bad-chunk'
    $corruptStream=RequestStream $corrupt
    $corruptStream.Position=0;$null=Read-SunnyTransferFrame $corruptStream
    $corruptStream.WriteByte(0);$corruptStream.Position=0
    try { Fails { Invoke-SunnyTransferReceiver $corruptStream } 'chunk checksum' } finally { $corruptStream.Dispose() }
    Check ((Get-Item -LiteralPath (Join-Path ($corrupt.destination+'.sunny-stage') 'image.tar')).Length -eq 0) 'Unauthenticated chunk was written'
    $passed.Add('strict_control_data_and_corrupt_chunks_fail_before_payload_writes')

    # The exact real NTFS hardlink and reparse cases are refused without changing targets.
    $linked=NewPlan 'hardlink-stage';Fails { Receiver $linked 1048600 } 'Interrupted transfer'
    $linkedImage=Join-Path ($linked.destination+'.sunny-stage') 'image.tar'
    $foreignLink=Join-Path $root 'hardlink-target.bin'
    New-Item -ItemType HardLink -Path $foreignLink -Target $linkedImage | Out-Null
    $linkedHash=(Get-FileHash -LiteralPath $foreignLink).Hash
    Fails { Receiver $linked } 'hardlinked'
    $linked.operation='Clean';Fails { Receiver $linked } 'hardlinked'
    Check ((Get-FileHash -LiteralPath $foreignLink).Hash -ceq $linkedHash) 'Hardlink target changed'
    $junction=NewPlan 'junction-stage';Fails { Receiver $junction 1048600 } 'Interrupted transfer'
    $junctionTarget=Join-Path $root 'junction-user-data';[IO.Directory]::CreateDirectory($junctionTarget) | Out-Null
    WriteText (Join-Path $junctionTarget 'user.als') 'junction literal untouched'
    New-Item -ItemType Junction -Path (Join-Path ($junction.destination+'.sunny-stage') 'native') -Target $junctionTarget | Out-Null
    Fails { Receiver $junction } 'physical path'
    $junction.operation='Clean';Fails { Receiver $junction } 'physical path'
    Check ([IO.File]::ReadAllText((Join-Path $junctionTarget 'user.als')) -ceq 'junction literal untouched') 'Junction target changed'
    $passed.Add('actual_ntfs_hardlinks_and_junctions_preserved')

    $denied=NewPlan 'actual-readonly-denial';Fails { Receiver $denied 1048600 } 'Interrupted transfer'
    $deniedImage=Join-Path ($denied.destination+'.sunny-stage') 'image.tar'
    [IO.File]::SetAttributes($deniedImage,[IO.FileAttributes]::ReadOnly)
    try { Fails { Receiver $denied } 'denied' }
    finally { [IO.File]::SetAttributes($deniedImage,[IO.FileAttributes]::Normal) }
    $denied.operation='Recover';Receiver $denied
    # A production chunk write is interrupted after literal bytes hit NTFS. The
    # IOException is injected, not a claim of saturating the physical disk.
    $writeOriginal=(Get-Item Function:\Write-SunnyTransferChunk).ScriptBlock
    $disk=NewPlan 'injected-disk-full'
    Set-Item Function:\Write-SunnyTransferChunk {
        param($Stream,$Chunk,$Offset,$Count)
        $Stream.Write($Chunk,$Offset,[int][Math]::Min(97,$Count));$Stream.Flush($true)
        throw [IO.IOException]::new('Injected disk-full write after 97 durable bytes')
    }
    try { Fails { Receiver $disk } 'Injected disk-full' }
    finally { Set-Item Function:\Write-SunnyTransferChunk $writeOriginal }
    Check ((Get-Item -LiteralPath (Join-Path ($disk.destination+'.sunny-stage') 'image.tar')).Length -eq 97) 'Injected partial write was not real'
    $disk.operation='Recover';Receiver $disk
    Check ((Get-FileHash -LiteralPath (Join-Path $disk.destination 'image.tar')).Hash.ToLowerInvariant() -ceq (Get-SunnyTransferHash $binary)) 'Partial disk-write recovery failed'
    $moveOriginal=(Get-Item Function:\Move-SunnyTransferRelease).ScriptBlock
    $rename=NewPlan 'injected-rename-denial'
    Set-Item Function:\Move-SunnyTransferRelease { param($Stage,$Final) throw [IO.IOException]::new('Injected final rename denial') }
    try { Fails { Receiver $rename } 'rename denial' }
    finally { Set-Item Function:\Move-SunnyTransferRelease $moveOriginal }
    Check ((Read-SunnyJson ($rename.destination+'.sunny-transfer.json')).phase -ceq 'verified') 'Rename failure lost verified phase'
    $rename.operation='Recover';Receiver $rename
    $passed.Add('actual_readonly_denial_and_injected_disk_write_rename_recovery')

    $drift=NewPlan 'cleanup-drift';Fails { Receiver $drift 1048600 } 'Interrupted transfer';$drift.operation='Clean'
    $deleteOriginal=(Get-Item Function:\Remove-SunnyTransferProvenFile).ScriptBlock
    Set-Item Function:\Remove-SunnyTransferProvenFile {
        param($Path,$Proof)
        $changedStream=[IO.File]::OpenWrite($Path);try { $changedStream.WriteByte(11) } finally { $changedStream.Dispose() }
        & $deleteOriginal $Path $Proof
    }
    try { Fails { Receiver $drift } 'after source proof' }
    finally { Set-Item Function:\Remove-SunnyTransferProvenFile $deleteOriginal }
    Check (Test-Path -LiteralPath (Join-Path ($drift.destination+'.sunny-stage') 'image.tar')) 'Post-proof drift was deleted'
    $passed.Add('post_proof_cleanup_drift_preserved')

    # Inject expiry after real filesystem work. Physical disk-stall cancellation
    # is unqualified, but later phases must observe the exhausted budget.
    $budgetChunk=NewPlan 'budget-after-chunk'
    Set-Item Function:\Write-SunnyTransferChunk {
        param($Stream,$Chunk,$Offset,$Count)
        & $writeOriginal $Stream $Chunk $Offset $Count
        $script:SunnyTransferReadSeconds=0
    }
    try { Fails { Receiver $budgetChunk } 'Receiver transfer deadline expired' }
    finally { Set-Item Function:\Write-SunnyTransferChunk $writeOriginal }
    Check ((Get-Item -LiteralPath (Join-Path ($budgetChunk.destination+'.sunny-stage') 'image.tar')).Length -eq 1048576) 'Budget-expired partial write was not retained'
    $budgetChunk.operation='Recover';Receiver $budgetChunk
    $readOriginal=(Get-Item Function:\Read-SunnyTransferRelease).ScriptBlock
    $budgetHash=NewPlan 'budget-after-verification'
    Set-Item Function:\Read-SunnyTransferRelease {
        param($Root,$Hash)
        $result=& $readOriginal $Root $Hash
        $script:SunnyTransferReadSeconds=0
        return $result
    }
    try { Fails { Receiver $budgetHash } 'Receiver transfer deadline expired' }
    finally { Set-Item Function:\Read-SunnyTransferRelease $readOriginal }
    Check (-not (Test-Path -LiteralPath $budgetHash.destination) -and
        (Read-SunnyJson ($budgetHash.destination+'.sunny-transfer.json')).phase -ceq 'receiving') 'Expired verification published a release'
    $budgetHash.operation='Recover';Receiver $budgetHash
    $budgetMove=NewPlan 'budget-after-move'
    Set-Item Function:\Move-SunnyTransferRelease {
        param($Stage,$Final)
        & $moveOriginal $Stage $Final
        $script:SunnyTransferReadSeconds=0
    }
    try { Fails { Receiver $budgetMove } 'Receiver transfer deadline expired' }
    finally { Set-Item Function:\Move-SunnyTransferRelease $moveOriginal }
    Check ((Test-Path -LiteralPath $budgetMove.destination) -and
        (Read-SunnyJson ($budgetMove.destination+'.sunny-transfer.json')).phase -ceq 'verified') 'Post-move expiry lost its durable recovery state'
    $budgetMove.operation='Recover';Receiver $budgetMove 0
    Check ((Read-SunnyJson ($budgetMove.destination+'.sunny-transfer.json')).phase -ceq 'committed') 'Post-move budget recovery failed'
    $passed.Add('expired_filesystem_phase_budgets_preserve_recoverable_states')

    # The exact bootstrap + raw STDIO is exercised using an actual child powershell.
    # This proves Windows pipe/framing; it makes no SSH connection or OCI claim.
    $pipe=NewPlan 'actual pipe destination'
    $result=Invoke-SunnyTransferExchange (Get-Process -Id $PID).Path @('-NoProfile','-NonInteractive','-EncodedCommand',(Get-SunnyTransferStarter)) $source $pipe ([IO.File]::ReadAllText($installerPath)) ([IO.File]::ReadAllText($transferPath)) 30
    Check ($result.status -ceq 'committed') 'Actual binary pipe exchange failed'
    Check ((Get-FileHash -LiteralPath (Join-Path $pipe.destination 'image.tar')).Hash.ToLowerInvariant() -ceq (Get-SunnyTransferHash $binary)) 'Actual pipe altered bytes'
    $repeat=Invoke-SunnyTransferExchange (Get-Process -Id $PID).Path @('-NoProfile','-NonInteractive','-EncodedCommand',(Get-SunnyTransferStarter)) $source $pipe ([IO.File]::ReadAllText($installerPath)) ([IO.File]::ReadAllText($transferPath)) 30
    Check (-not $repeat.payload_required -and $repeat.status -ceq 'committed') 'Idempotent pipe sent payload'
    $passed.Add('real_windows_binary_pipe_bootstrap_and_zero_payload_repeat')

    # Fault peers use the real bootstrap and preserve ownership, but emit invalid
    # acknowledgments or too much output. No artifact payload may follow.
    foreach ($fault in @('boolean-schema','unknown-field','wrong-request','array-manifest-hash','stdout-flood','stderr-flood')) {
        $bad=NewPlan ('fault-peer-'+$fault)
        $pidPath=Join-Path $root ($fault+'.pid')
        $faultData=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes((@{pid_path=$pidPath;fault=$fault;manifest_sha256=$manifestHash} | ConvertTo-Json -Compress)))
        $faultSource=[IO.File]::ReadAllText($transferPath)+"`n"+@'
$script:SunnyTransferFixtureData=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String('__FAULT_DATA__'))|ConvertFrom-Json
function Send-SunnyTransferAck($Value) {
 [IO.File]::WriteAllText($script:SunnyTransferFixtureData.pid_path,[string]$PID)
 switch ($script:SunnyTransferFixtureData.fault) {
  'boolean-schema' { $Value.transfer_schema_version=$true }
  'unknown-field' { $Value.unexpected='reject' }
  'wrong-request' { $Value.request_id='00000000000000000000000000000000' }
  'array-manifest-hash' { $Value.status='committed';$Value.payload_required=$false;$Value.manifest_sha256=@($script:SunnyTransferFixtureData.manifest_sha256) }
  'stdout-flood' { [Console]::Out.Write(('x'*70000)+"`n");[Console]::Out.Flush();Start-Sleep -Seconds 90;return }
  'stderr-flood' { [Console]::Error.Write('x'*70000);[Console]::Error.Flush();Start-Sleep -Seconds 90;return }
 }
 [Console]::Out.WriteLine(($Value | ConvertTo-Json -Depth 5 -Compress));[Console]::Out.Flush();Start-Sleep -Seconds 90
}
'@
        $faultSource=$faultSource.Replace('__FAULT_DATA__',$faultData)
        Fails { Invoke-SunnyTransferExchange (Get-Process -Id $PID).Path @('-NoProfile','-NonInteractive','-EncodedCommand',(Get-SunnyTransferStarter)) $source $bad ([IO.File]::ReadAllText($installerPath)) $faultSource 30 } 'integer|unknown|uncorrelated|source mismatch|64 KiB'
        Check ($null -eq (Get-Process -Id ([int][IO.File]::ReadAllText($pidPath)) -ErrorAction SilentlyContinue)) 'Fault peer process survived'
        Check ((Get-Item -LiteralPath ($bad.destination+'.sunny-stage')).PSIsContainer) 'Fault peer lost owned stage'
        Check (-not (Test-Path -LiteralPath $bad.destination)) 'Fault peer published a release'
        Check (@(Get-ChildItem -LiteralPath ($bad.destination+'.sunny-stage') -File -Recurse).Count -eq 0) 'Payload was sent after invalid acknowledgment'
    }
    $passed.Add('actual_invalid_acknowledgments_and_output_bounds_reap_owned_peer')

    # Run both independent timeout witnesses concurrently: the receiver's own
    # deadline bounds an open, silent input pipe, and the sender reaps exactly its
    # launched child when no readiness acknowledgment arrives. No SSH is used.
    $silent=NewPlan 'receiver-timeout';$silent.timeout_seconds=30
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=(Get-Process -Id $PID).Path
    $start.Arguments=(@(@('-NoProfile','-NonInteractive','-EncodedCommand',(Get-SunnyTransferStarter)) | ForEach-Object { ConvertTo-SunnyWindowsArgument $_ }) -join ' ')
    $start.UseShellExecute=$false;$start.RedirectStandardInput=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    $child=[Diagnostics.Process]::Start($start);$childPID=$child.Id
    try {
        $outTask=$child.StandardOutput.ReadToEndAsync();$errTask=$child.StandardError.ReadToEndAsync()
        $bootstrap=@{installer=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes([IO.File]::ReadAllText($installerPath)));
            transfer=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes([IO.File]::ReadAllText($transferPath)))}
        Frame $child.StandardInput.BaseStream ([Text.Encoding]::UTF8.GetBytes(($bootstrap | ConvertTo-Json -Compress)))
        Frame $child.StandardInput.BaseStream ([Text.Encoding]::ASCII.GetBytes([Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(($silent | ConvertTo-Json -Depth 8 -Compress)))))
        $child.StandardInput.BaseStream.Flush()
        $hangPath=Join-Path $root 'owned-hung-child.pid'
        $data=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes((@{pid_path=$hangPath} | ConvertTo-Json -Compress)))
        $hangCode='$d=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("'+$data+'"))|ConvertFrom-Json;[IO.File]::WriteAllText($d.pid_path,[string]$PID);Start-Sleep -Seconds 90'
        $hangEncoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($hangCode))
        $hangPlan=NewPlan 'sender-timeout';$elapsed=[Diagnostics.Stopwatch]::StartNew()
        Fails { Invoke-SunnyTransferExchange (Get-Process -Id $PID).Path @('-NoProfile','-NonInteractive','-EncodedCommand',$hangEncoded) $source $hangPlan ([IO.File]::ReadAllText($installerPath)) ([IO.File]::ReadAllText($transferPath)) 30 } 'deadline expired'
        Check ($elapsed.Elapsed.TotalSeconds -ge 30 -and $elapsed.Elapsed.TotalSeconds -lt 36) 'Sender deadline was not finite'
        Check ($child.WaitForExit(6000)) 'Receiver did not terminate its own silent-input deadline'
        Check ($child.ExitCode -ne 0 -and $errTask.GetAwaiter().GetResult() -match 'Receiver transfer deadline expired') 'Receiver timeout was misreported'
        Check ($outTask.GetAwaiter().GetResult() -match '"status":"ready"') 'Receiver timed out before admitting the authenticated plan'
        Check ((Read-SunnyJson ($silent.destination+'.sunny-transfer.json')).phase -ceq 'receiving') 'Receiver timeout lost durable receiving state'
        $hungPID=[int][IO.File]::ReadAllText($hangPath)
        Check ($null -eq (Get-Process -Id $hungPID -ErrorAction SilentlyContinue)) 'Owned sender child survived timeout'
        Check ($null -eq (Get-Process -Id $childPID -ErrorAction SilentlyContinue)) 'Owned receiver child survived timeout'
        $silent.operation='Recover';Receiver $silent
        Check ((Get-FileHash -LiteralPath (Join-Path $silent.destination 'image.tar')).Hash.ToLowerInvariant() -ceq (Get-SunnyTransferHash $binary)) 'Timeout recovery failed'
    } finally {
        if (-not $child.HasExited) { $child.Kill();$null=$child.WaitForExit(1000) }
        $child.Dispose()
    }
    $passed.Add('actual_sender_receiver_deadlines_pid_absence_and_recovery')
    @{passed=$passed.ToArray();ssh_executed=$false;native_live_executed=$false;literal_disk_saturation_tested=$false} | ConvertTo-Json -Depth 8 -Compress
} finally { if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force } }
