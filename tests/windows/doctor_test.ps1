$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
Set-StrictMode -Version Latest
$root=Join-Path $env:TEMP ('Sunny Doctor Café '+[Guid]::NewGuid().ToString('N'))
$null=[IO.Directory]::CreateDirectory($root)
$passed=[Collections.Generic.List[string]]::new()
$fixturePhase='fixture_setup';$fixturePrimaryPhase=$null;$fixtureFailure=$null
function Check([bool]$Value,[string]$Message){if(-not$Value){throw $Message}}
function Fails($Run,[string]$Pattern){$failed=$false;try{&$Run|Out-Null}catch{if($_.Exception.Message-notmatch$Pattern){throw};$failed=$true};Check $failed ('Expected refusal: '+$Pattern)}
function Text([string]$Path,[string]$Value){[IO.File]::WriteAllText($Path,$Value,[Text.UTF8Encoding]::new($false))}
function FixtureFailureJson($Failure,[string]$Phase,[string]$Kind){
 # PositionMessage's first line and ScriptStackTrace contain locations, not
 # invocation source/arguments. No child output or project contents are logged.
 $position=if($null-ne$Failure.InvocationInfo){([string]$Failure.InvocationInfo.PositionMessage-split"`n",2)[0].TrimEnd("`r")}else{''}
 $message=[string]$Failure.Exception.Message
 $exception=if($message-ceq'Remote command deadline expired; inspect status before repeating a state change.'){$message}elseif($message.StartsWith('Remote input pipe failed;')){'Remote input pipe failed; code=input_pipe_failure; child diagnostics omitted.'}else{$Failure.Exception.GetType().FullName+'; code=unclassified_fixture_exception'}
 $record=[ordered]@{fixture_failure_schema_version=1;kind=$Kind;phase=$Phase;last_completed_group=$(if($passed.Count){$passed[$passed.Count-1]}else{'none'});exception=$exception;position_message=$position;script_stack_trace=[string]$Failure.ScriptStackTrace}
 foreach($field in @('kind','phase','last_completed_group','exception','position_message','script_stack_trace')){
  $limit=if($field-ceq'script_stack_trace'){1024}elseif($field-ceq'exception'){512}elseif($field-ceq'position_message'){256}else{160}
  if($record[$field].Length-gt$limit){$record[$field]=$record[$field].Substring(0,$limit)}
 }
 return ($record|ConvertTo-Json -Depth 3 -Compress)
}
function WriteFixtureFailure($Failure,[string]$Phase,[string]$Kind){[Console]::Error.WriteLine('SUNNY_DOCTOR_FIXTURE_FAILURE '+(FixtureFailureJson $Failure $Phase $Kind))}
try{
 foreach($pair in @(@('Sunny.ps1','__INSTALLER_SOURCE__'),@('SunnyRemote.ps1','__REMOTE_SOURCE__'),@('SunnyClient.ps1','__CLIENT_SOURCE__'),@('SunnyDoctor.ps1','__DOCTOR_SOURCE__'))){
  [IO.File]::WriteAllBytes((Join-Path $root $pair[0]),[Convert]::FromBase64String($pair[1]))
 }
 . (Join-Path $root 'Sunny.ps1');. (Join-Path $root 'SunnyRemote.ps1');. (Join-Path $root 'SunnyClient.ps1');. (Join-Path $root 'SunnyDoctor.ps1')
 $actualRemove=(Get-Command Remove-SunnyClientContainers).ScriptBlock
 Initialize-SunnyDoctorTypes
 $shell=Join-Path $PSHOME 'powershell.exe'
 $release=@{bridge=@{source_sha256=('a'*64);contract=@{bridge_protocol_version=47}}}
 $peer=Join-Path $root 'literal peer.ps1'
 Text $peer @'
param([string]$Mode,[string]$Marker)
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$owner=[Guid]::NewGuid().ToString('N')
$directory=Join-Path ([IO.Path]::GetTempPath()) ('Sunny-client-'+$owner)
$null=[IO.Directory]::CreateDirectory($directory)
$journal=@{schema_version=1;owner_sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value;owner=$owner;context='fixture';daemon_id='literal-daemon';parent_pid=$PID;parent_started=(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks.ToString()}
if($Mode-eq'docker_orphan'){$journal.context='desktop-linux';$journal.daemon_id=(& docker.exe --context desktop-linux info --format '{{.ID}}').Trim();if($LASTEXITCODE-ne0){exit 93}}
if($Mode-eq'wrong_owner'){$journal.owner_sid='S-1-0-0'}
[IO.File]::WriteAllText((Join-Path $directory 'session.json'),($journal|ConvertTo-Json -Compress),[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($Marker,($journal|ConvertTo-Json -Compress),[Text.UTF8Encoding]::new($false))
[Console]::Error.WriteLine('Sunny client recovery directory: '+$directory)
if($Mode-eq'docker_orphan'){
 $id=& docker.exe --context desktop-linux run -d --name ('sunny-client-'+$owner+'-run') --label ('org.sunny.client.owner='+$owner) --mount ('type=volume,source='+$env:SUNNY_DOCTOR_FIXTURE_VOLUME+',target=/data') busybox:1.36 sleep 90
 if($LASTEXITCODE-ne0){exit 94};[IO.File]::WriteAllText(($Marker+'.cid'),$id.Trim())
}
if($Mode-eq'overflow_err'){[Console]::Error.Write(('x'*70000))}
if($Mode-eq'noise'){[Console]::Out.WriteLine('stdout noise')}
if($Mode-eq'bad_utf8'){$out=[Console]::OpenStandardOutput();$out.WriteByte(255);$out.WriteByte(10);$out.Flush()}
if($Mode-eq'timeout'){Start-Sleep -Seconds 60;exit 0}
$pollCount=0
if($Mode-eq'descendant'){
 $data=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(($Marker+'.descendant')))
 $code='[IO.File]::WriteAllText([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("'+$data+'")),[string]$PID);Start-Sleep -Seconds 60'
 $command=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($code))
 $child=Start-Process (Join-Path $PSHOME 'powershell.exe') -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand',$command) -WindowStyle Hidden -PassThru
 $watch=[Diagnostics.Stopwatch]::StartNew();while(-not(Test-Path -LiteralPath ($Marker+'.descendant'))-and$watch.Elapsed.TotalSeconds-lt5){Start-Sleep -Milliseconds 10}
}
while($null-ne($line=[Console]::In.ReadLine())){
 $request=$line|ConvertFrom-Json
 [IO.File]::AppendAllText(($Marker+'.requests'),$request.method+':'+$(if($request.PSObject.Properties['params']-and$request.params.PSObject.Properties['name']){$request.params.name}else{''})+"`n")
 if($request.method-eq'notifications/initialized'){continue}
 if($request.method-eq'initialize'){$result=@{protocolVersion='2025-11-25';capabilities=@{};serverInfo=@{name='literal';version='1'}};if($Mode-eq'protocol'){$result.protocolVersion='2024-11-05'}}
 elseif($request.method-eq'ping'){$result=@{}}
 elseif($request.method-eq'tools/list'){$result=@{tools=@(@{name='doctor_ableton'},@{name='get_ableton_remote_log'})};if($Mode-eq'missing_tools'){$result.tools=@(@{name='doctor_ableton'})}}
 elseif($request.method-eq'tools/call'-and$request.params.name-eq'get_ableton_remote_log'){
  $pollCount++;[IO.File]::AppendAllText(($Marker+'.logrequests'),($request.params.arguments|ConvertTo-Json -Compress)+"`n")
  if($Mode-eq'log_timeout'){Start-Sleep -Seconds 60;exit 0}
  [long]$after=$request.params.arguments.after_sequence;$stream='d'*32;$oldest=1;$latest=$after;$entries=@();$reset=$false
  $indices=@()
  if($Mode-eq'log_pages'){$latest=3;if($pollCount-eq1){$indices=@(1)}elseif($pollCount-eq2){$indices=@(2,3)}}
  elseif($Mode-eq'log_gap'){$oldest=3;$latest=3;if($pollCount-eq1){$indices=@(3)}}
  elseif($Mode-in@('log_rotate','log_ahead','log_reset_hidden')){
   if($pollCount-eq1){$latest=3;$indices=@(1,2,3)}else{$latest=1;if($Mode-ne'log_ahead'){$stream='e'*32};if($pollCount-eq2){$reset=$true;$after=0;$indices=@(1)}}
  }
  elseif($Mode-eq'log_many'){$latest=300;if($pollCount-eq1){$indices=@(1..300)}}
  elseif($Mode-eq'log_pending'){$latest=$after+2;$indices=@($after+1)}
  elseif($Mode-in@('log_messages','log_unicode','log_unpaired','log_unordered','log_next_bad','log_gap_hidden')){$latest=1;if($pollCount-eq1){$indices=@(1)}}
  $entries=@(foreach($sequence in $indices){
   $message='literal-message';$source='literal-source'
   if($Mode-in@('log_messages','log_many')){$message='password="literal private" token=private-token ssh://user:private-pass@host Bearer private-bearer sk-abcdefghijk ghp_abcdefghijk';if($Mode-eq'log_many'){$message=$message+' '+('x'*(1999-$message.Length))}}
   if($Mode-eq'log_unicode'){$message=[char]::ConvertFromUtf32(0x1f600)*2000;$source=[char]::ConvertFromUtf32(0x1f600)*128}
   @{sequence=$sequence;time=([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0);level='INFO';source=$source;message=$message}
  })
  [long]$next=if($entries.Count){$indices[-1]}else{$after}
  $page=@{success=$true;entries=$entries;next_sequence=$next;latest_sequence=$latest;oldest_sequence=$oldest;stream_id=$stream;reset=$reset;truncated=($oldest-gt($after+1));has_more=($next-lt$latest);observed_at=([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0)}
  switch($Mode){
   'log_stale'{$page.observed_at-=60}
   'log_unknown'{$page.foreign='refuse'}
   'log_legacy'{$page.stream_id=$null;$page.cursor_metadata_available=$false}
   'log_bool'{$page.reset=0}
   'log_seqfloat'{$page.next_sequence=[double]0.5}
   'log_seq_array'{$page.latest_sequence=@(0)}
   'log_gap_hidden'{$page.oldest_sequence=3;$page.latest_sequence=3;$page.entries[0].sequence=3;$page.next_sequence=3;$page.truncated=$false}
   'log_reset_hidden'{if($pollCount-eq2){$page.reset=$false}}
   'log_hasmore_bad'{$page.latest_sequence=1;$page.has_more=$true}
   'log_next_bad'{if($pollCount-eq1){$page.next_sequence=2}}
   'log_unordered'{if($pollCount-eq1){$page.entries[0].sequence=2;$page.latest_sequence=2;$page.next_sequence=2}}
   'log_error'{$page=@{success=$false;error='password=raw-native-private-error'}}
  }
  $text=$page|ConvertTo-Json -Depth 8 -Compress
  if($Mode-eq'log_duplicate'){$text=$text.Replace('"success":true','"success":true,"\u0073uccess":true')}
  if($Mode-eq'log_unpaired'){$text=$text.Replace('literal-message','\ud800')}
  $result=@{isError=(-not$page.success);content=@(@{type='text';text=$text})}
  if($Mode-eq'log_structured'){$result.structuredContent=$page}
 }
 elseif($request.method-eq'tools/call'){
  if($request.params.name-ne'doctor_ableton'){exit 90}
  $layers=@('bridge_connection','release_pairing','native_session','native_readiness')
  $checks=@(foreach($layer in $layers){@{layer=$layer;status='pass';code='literal';message='literal';next_step=''}})
  $doctor=@{schema_version=1;request_id=$request.params.arguments.request_id;success=$true;read_only_ready=$true;observed_at=([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0);
   expected_bridge=@{source_sha256=('a'*64);protocol_version=47};observed_bridge=@{source_sha256=('a'*64);protocol_version=47};
   session=@{schema_version=1;bridge_instance=('b'*32);document_token=('c'*32)};native_state=@{is_playing=$false;session_record=$false;record_mode=$false};
   capabilities=@{basis='version_floor_claims_not_host_qualification';managed_authoring_version_eligible=$true;live_version=@{major=12;minor=4;bugfix=5};reported=@{clip_add_new_notes='available';track_insert_device_native='available';automation_envelope_authoring='available';group_track_creation='unknown';arbitrary_browser_loading='unknown';structural_snapshot='available';max_for_live='unknown'}};
   checks=$checks;unverified=@('library','audio','licence','musical effects','two-host qualification')}
  switch($Mode){
   'wrong_source'{$doctor.observed_bridge.source_sha256='d'*64}
   'wrong_expected'{$doctor.expected_bridge.source_sha256='d'*64;$doctor.observed_bridge.source_sha256='d'*64}
   'stale'{$doctor.observed_at-=600}
   'session_array'{$doctor.session.document_token=@('c'*32)}
   'boolean_int'{$doctor.native_state.is_playing=0}
   'success_array'{$doctor.success=@($true)}
   'nonce'{$doctor.request_id='d'*32}
   'future'{$doctor.schema_version=2}
   'unknown'{$doctor.foreign='refuse'}
   'offline'{$doctor.success=$false;$doctor.read_only_ready=$false;$doctor.observed_bridge=$null;$doctor.session=$null;$doctor.native_state=$null;$doctor.capabilities=$null;$doctor.checks=@(@{layer='bridge_connection';status='fail';code='native_unreachable';message='Literal offline connection';next_step='Check host status'});$doctor.connection_failure='not_connected'}
  }
  $result=@{isError=(-not$doctor.success);content=@(@{type='text';text=($doctor|ConvertTo-Json -Depth 12 -Compress)})}
  if($Mode-eq'structured'){$result.structuredContent=[ordered]@{};foreach($key in @($doctor.Keys|Sort-Object)){$result.structuredContent[$key]=$doctor[$key]}}
  if($Mode-eq'unknown_tool_envelope'){$result.foreign='refuse'}
 }
 else{exit 91}
 $response=@{jsonrpc='2.0';id=$request.id;result=$result}
 if($Mode-eq'wrong_id'){$response.id+=1}
 $encoded=$response|ConvertTo-Json -Depth 15 -Compress
 if($Mode-eq'duplicate'){$encoded=$encoded.Replace('"jsonrpc":"2.0"','"jsonrpc":"2.0","\u006asonrpc":"2.0"')}
 if($Mode-eq'truncated'){[Console]::Out.Write($encoded);exit 0}
 if($Mode-eq'overflow_out'){[Console]::Out.Write(('x'*16780000));[Console]::Out.Flush();Start-Sleep -Seconds 60;exit 0}
 [Console]::Out.WriteLine($encoded);[Console]::Out.Flush()
}
if($Mode-eq'late_noise'){[Console]::Out.WriteLine('late stdout noise')}
if($Mode-eq'late_stderr'){[Console]::Error.Write(('x'*70000))}
if($Mode-eq'bad_exit'){exit 9}
exit 0
'@
 $script:removed=0;$script:denyCleanup=$false;$script:realDocker=$false
 function Remove-SunnyClientContainers([string]$Executable,[string]$Context,[string]$Daemon,[string]$Owner,[int]$Seconds){
  if($script:realDocker){&$actualRemove $Executable $Context $Daemon $Owner $Seconds;return}
  Check ($Context-ceq'fixture'-and$Daemon-ceq'literal-daemon'-and$Owner-cmatch'^[0-9a-f]{32}$'-and$Seconds-gt0) 'Unproved cleanup scope used'
  if($script:denyCleanup){throw 'Literal changed daemon refusal'};$script:removed++
 }
 function Run([string]$Mode,[int]$Seconds=8,[double]$Polling=0,[double]$Interval=0.05,[bool]$Messages=$false){
  $owner=[Guid]::NewGuid().ToString('N');$directory=Join-Path $root ('Sunny-doctor-'+$owner);$null=[IO.Directory]::CreateDirectory($directory)
  $path=Join-Path $directory 'doctor.json';$marker=Join-Path $directory 'peer.json'
  $state=@{schema_version=1;owner_sid=Get-SunnyOwner;owner=$owner;parent_pid=$PID;parent_started=(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks.ToString();child_pid=$null;child_started=$null;client_scope=$null;cleanup_confirmed=$false}
  Write-SunnyDoctorState $path $state -Initial
  $args=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$peer,'-Mode',$Mode,'-Marker',$marker)
  $context=if($Mode-eq'docker_orphan'){'desktop-linux'}else{'fixture'}
  $report=Invoke-SunnyDoctorExchange $shell $args $root $release $context $Seconds 12 $path $state $Polling $Interval $Messages
  $journal=ConvertFrom-SunnyDoctorJson (Read-SunnyDoctorFile $marker 4096)
  Check ($null-eq(Get-Process -Id $journal.parent_pid -ErrorAction SilentlyContinue)) ('Owned peer remains: '+$Mode)
  Check $report.cleanup.owned_job_absent ('Owned job absence unconfirmed: '+$Mode)
  return @{report=$report;state=$state;directory=$directory;marker=$marker;journal=$journal}
 }
 $success=Run 'success'
 Check ($success.report.success-and$success.report.read_only_ready-and$success.report.cleanup.success) ('Literal fresh doctor did not pass: '+($success.report|ConvertTo-Json -Depth 12 -Compress)+' errors '+($Error|ForEach-Object{$_.Exception.Message}))
 Check ($success.report.logs-eq$null) 'Default exported log content'
 $requests=[IO.File]::ReadAllLines($success.marker+'.requests')
 Check (($requests-join',')-ceq'initialize:,notifications/initialized:,ping:,tools/list:,tools/call:doctor_ableton') 'Unexpected protocol or mutation request'
 $passed.Add('fresh_closed_readonly_protocol_and_exact_owned_cleanup')
 foreach($mode in @('wrong_source','wrong_expected','stale','session_array','boolean_int','success_array','nonce','future','unknown','unknown_tool_envelope','protocol','missing_tools','wrong_id','duplicate','truncated','noise','bad_utf8')){
  $run=Run $mode;Check (-not$run.report.success-and-not$run.report.read_only_ready) ('Malformed evidence accepted: '+$mode)
 }
 $passed.Add('typed_pairing_session_freshness_and_correlated_closed_protocol_refusals')
 $offline=Run 'offline';Check (-not$offline.report.success-and$null-ne$offline.report.doctor-and$offline.report.doctor.connection_failure-ceq'not_connected'-and$offline.report.cleanup.success) 'Honest incomplete native evidence was lost or reported ready'
 $structured=Run 'structured';Check $structured.report.success 'Equivalent structured and text evidence rejected due to key ordering'
 $passed.Add('honest_offline_evidence_and_semantic_structured_content')
 foreach($mode in @('late_noise','late_stderr','bad_exit')){$run=Run $mode;Check (-not$run.report.success-and$run.report.cleanup.success) ('Invalid close accepted: '+$mode)}
 $passed.Add('normal_exit_and_trailing_output_are_part_of_readiness')
 foreach($mode in @('timeout','overflow_err','overflow_out')){$watch=[Diagnostics.Stopwatch]::StartNew();$run=Run $mode 2;Check (-not$run.report.success-and$watch.Elapsed.TotalSeconds-lt11) ('Unbounded failed peer: '+$mode)}
 # Exercise the exact diagnostic writer with a real owned helper timeout. Keep
 # its stderr private to this control; the successful fixture emits only JSON.
 $fixturePhase='forced_owned_process_timeout';$forcedFailure=$null
 $forcedDirectory=Join-Path $root 'forced bounded process';$null=[IO.Directory]::CreateDirectory($forcedDirectory)
 $forcedMarker=Join-Path $forcedDirectory 'peer.json';$diagnosticSink=[IO.StringWriter]::new();$originalErrorWriter=[Console]::Error
 try{
  [Console]::SetError($diagnosticSink)
  $forcedOriginal=$null
  try{
   try{$null=Invoke-SunnyBoundedProcess $shell @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$peer,'-Mode','timeout','-Marker',$forcedMarker) '' 2}
   catch{$forcedOriginal=$_;WriteFixtureFailure $_ $fixturePhase 'operation_failure';throw}
   finally{
    # An independent literal cleanup refusal cannot replace the real timeout.
    try{throw 'Literal fixture cleanup refusal'}
    catch{WriteFixtureFailure $_ 'forced_cleanup_refusal' 'cleanup_failure';if($null-eq$forcedOriginal){throw}}
   }
  }catch{$forcedFailure=$_}
 }finally{[Console]::SetError($originalErrorWriter)}
 $diagnostic=$diagnosticSink.ToString();$diagnosticSink.Dispose()
 Check ($null-ne$forcedFailure-and$forcedFailure.Exception.Message-match'deadline expired') 'Actual bounded-process timeout control did not fail'
 Check ($diagnostic.StartsWith('SUNNY_DOCTOR_FIXTURE_FAILURE ')-and[Text.Encoding]::UTF8.GetByteCount($diagnostic)-lt16384) 'Failure context is absent or unbounded'
 $diagnosticLines=$diagnostic.Trim()-split"`r?`n"
 Check ($diagnosticLines.Count-eq2) 'Original timeout and cleanup failure were not separately retained'
 $context=ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes($diagnosticLines[0].Substring(29)))
 $cleanupContext=ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes($diagnosticLines[1].Substring(29)))
 Check ($cleanupContext.kind-ceq'cleanup_failure'-and$cleanupContext.phase-ceq'forced_cleanup_refusal'-and$cleanupContext.exception-match'code=unclassified_fixture_exception$') 'Cleanup failure was lost or mislabeled'
 Check ($context.phase-ceq$fixturePhase-and$context.kind-ceq'operation_failure'-and$context.last_completed_group-ceq'normal_exit_and_trailing_output_are_part_of_readiness') 'Failure context lost its literal phase or completed group'
 Check ($context.exception-match'deadline expired'-and$context.position_message-match'SunnyRemote.ps1'-and$context.script_stack_trace-match'Invoke-SunnyBoundedProcess') 'Failure context lost the actual exception or source stack'
 $forcedPeer=ConvertFrom-SunnyDoctorJson (Read-SunnyDoctorFile $forcedMarker 4096)
 Check (($forcedPeer.parent_pid-is[int]-or$forcedPeer.parent_pid-is[long])-and$forcedPeer.parent_pid-gt0-and$forcedPeer.parent_pid-ne$PID-and$forcedPeer.parent_started-cmatch'^[0-9]{1,19}$') 'Forced timeout lacks its actual positive owned process identity'
 Check ($null-eq(Get-Process -Id $forcedPeer.parent_pid -ErrorAction SilentlyContinue)) 'Forced timeout retained its actual owned child'
 foreach($privateMessage in @('Remote input pipe failed; outcome unknown; child stderr=SUNNY_PRIVATE_CHILD; input error=SUNNY_PRIVATE_ARGUMENT','Unknown SUNNY_PRIVATE_CHILD SUNNY_PRIVATE_ARGUMENT')){
  $privateFailure=[Management.Automation.ErrorRecord]::new([IO.IOException]::new($privateMessage),'private-fixture',[Management.Automation.ErrorCategory]::NotSpecified,$null)
  $safeContext=FixtureFailureJson $privateFailure 'private_message_control' 'operation_failure'
  Check ($safeContext-notmatch'SUNNY_PRIVATE_CHILD|SUNNY_PRIVATE_ARGUMENT|child stderr|input error') 'Failure context exported child diagnostics or arguments'
 }
 $fixturePhase='protocol_fixture_checks'
 $passed.Add('deadline_and_stdout_stderr_bounds_reap_actual_owned_process')
 $descendant=Run 'descendant'
 $childId=[int][IO.File]::ReadAllText($descendant.marker+'.descendant')
 Check ($null-eq(Get-Process -Id $childId -ErrorAction SilentlyContinue)) 'Owned descendant survived job closure'
 $passed.Add('actual_descendant_cannot_escape_suspended_owned_job')
 $admission=Join-Path $root 'must not execute before durable identity.json'
 $data=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($admission))
 $code='[IO.File]::WriteAllText([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("'+$data+'")),[string]$PID);Start-Sleep -Seconds 60'
 $arguments=@($shell,'-NoProfile','-NonInteractive','-EncodedCommand',[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($code)))
 $line=($arguments|ForEach-Object{ConvertTo-SunnyWindowsArgument $_})-join' ';$suspended=$null
 try{
  $suspended=[SunnyDoctorProcess]::new($shell,$line,$root,$false);Start-Sleep -Milliseconds 100
  Check (-not(Test-Path -LiteralPath $admission)-and-not$suspended.Process.HasExited) 'Child code ran before durable process identity'
  $identityPath=Join-Path $root 'independent identity.json';Write-SunnyDoctorState $identityPath @{child_pid=$suspended.Process.Id;child_started=$suspended.Process.StartTime.ToUniversalTime().Ticks.ToString()} -Initial
  $suspended.Resume();$clock=[Diagnostics.Stopwatch]::StartNew();while(-not(Test-Path -LiteralPath $admission)-and$clock.Elapsed.TotalSeconds-lt3){Start-Sleep -Milliseconds 5}
  Check ([IO.File]::ReadAllText($admission)-ceq([string]$suspended.Process.Id)) 'Resumed exact child identity changed'
  $suspended.Terminate();$clock.Restart();while(-not$suspended.Empty-and$clock.Elapsed.TotalSeconds-lt2){Start-Sleep -Milliseconds 5}
  Check $suspended.Empty 'Suspended admission fixture process survived termination'
 }finally{if($null-ne$suspended){$suspended.Dispose()}}
 $passed.Add('durable_exact_child_identity_precedes_any_resumed_launcher_code')
 $leak=[IO.Pipes.AnonymousPipeServerStream]::new([IO.Pipes.PipeDirection]::In,[IO.HandleInheritability]::Inheritable)
 $isolated=$null
 try{
  $arguments=@($shell,'-NoProfile','-NonInteractive','-Command','Start-Sleep -Seconds 60')
  $line=($arguments|ForEach-Object{ConvertTo-SunnyWindowsArgument $_})-join' '
  $isolated=[SunnyDoctorProcess]::new($shell,$line,$root);$leak.DisposeLocalCopyOfClientHandle()
  $bytes=[byte[]]::new(1);$read=$leak.ReadAsync($bytes,0,1);$clock=[Diagnostics.Stopwatch]::StartNew()
  while(-not$read.IsCompleted-and$clock.Elapsed.TotalSeconds-lt2){Start-Sleep -Milliseconds 5}
  Check ($read.IsCompleted-and$read.GetAwaiter().GetResult()-eq0-and-not$isolated.Process.HasExited) 'Unrelated caller pipe inherited into the owned child'
  $isolated.Terminate();$clock.Restart();while(-not$isolated.Empty-and$clock.Elapsed.TotalSeconds-lt2){Start-Sleep -Milliseconds 5}
  Check $isolated.Empty 'Handle isolation fixture child survived exact job termination'
 }finally{if($null-ne$isolated){$isolated.Dispose()};$leak.Dispose()}
 $passed.Add('exclusive_standard_handle_list_preserves_unrelated_caller_pipes')
 $before=$script:removed;$unproved=Run 'wrong_owner'
 Check (-not$unproved.report.cleanup.success-and$script:removed-eq$before-and(Test-Path -LiteralPath (Join-Path $unproved.directory 'doctor.json'))) 'Unproved scope cleaned or recovery evidence discarded'
 $script:denyCleanup=$true;$denied=Run 'success';$script:denyCleanup=$false
 Check (-not$denied.report.success-and-not$denied.report.cleanup.success-and$null-ne$denied.state.client_scope) 'Changed daemon refusal lost captured scope'
 $passed.Add('unproved_ownership_and_uncertain_cleanup_preserve_durable_evidence')
 $denialPath=Join-Path $root 'readonly checkpoint.json';$denialState=@{child_pid=$null;child_started=$null;client_scope=$null;cleanup_confirmed=$false};Write-SunnyDoctorState $denialPath $denialState -Initial
 [IO.File]::SetAttributes($denialPath,[IO.FileAttributes]::ReadOnly)
 try{
  $notExecuted=Join-Path $root 'denied launcher must not execute.json'
  $args=@('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$peer,'-Mode','success','-Marker',$notExecuted)
  $denied=Invoke-SunnyDoctorExchange $shell $args $root $release 'fixture' 3 5 $denialPath $denialState
  Check (-not$denied.success-and$denied.cleanup.owned_job_absent-and-not(Test-Path -LiteralPath $notExecuted)) 'Failed durable admission executed launcher or leaked its owned job'
 }finally{[IO.File]::SetAttributes($denialPath,[IO.FileAttributes]::Normal)}
 $passed.Add('actual_readonly_checkpoint_denial_cannot_resume_or_leak_owned_child')
 $export=Join-Path $root 'report.json';Export-SunnyDoctorReport $export $success.report
 $read=ConvertFrom-SunnyDoctorJson (Read-SunnyDoctorFile $export 1048576)
 Check ($read.success-and$read.cleanup.success) 'Exclusive export invalid JSON'
 $hash=(Get-FileHash -LiteralPath $export).Hash
 Fails {Export-SunnyDoctorReport $export $success.report} 'exists|already'
 Check ((Get-FileHash -LiteralPath $export).Hash-ceq$hash) 'Existing export changed'
 $redacted=Protect-SunnyDoctorValues @{message='password="do not expose" api_key=literal-private authorization=Bearer-private'}
 Check ($redacted.message-notmatch'expose|literal-private|Bearer-private') 'Bounded text redaction failed'
 Fails {Export-SunnyDoctorReport (Join-Path $root 'oversized.json') @{message=('x'*1048577)}} '1 MiB'
 Check (-not(Test-Path -LiteralPath (Join-Path $root 'oversized.json'))) 'Oversized export created file'
 $passed.Add('optin_exclusive_bounded_valid_json_export_and_value_redaction')
 Fails {ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes('{"x":1,"\u0078":2}'))} 'Duplicate'
 $physical=Join-Path $root 'physical.json';Text $physical '{}'
 $hard=Join-Path $root 'linked.json';$null=New-Item -ItemType HardLink -Path $hard -Target $physical
 Fails {Read-SunnyDoctorFile $physical 100} 'Linked'
 $passed.Add('escaped_duplicate_keys_and_actual_ntfs_hardlinks_refused')

 # A trusted offline fixture contains the exact real operator scripts. Exercise
 # capture and refusal before any launcher/SSH process can be started.
 $artifact=Join-Path $root 'fixture release';$null=[IO.Directory]::CreateDirectory((Join-Path $artifact 'installer\windows'));$null=[IO.Directory]::CreateDirectory((Join-Path $artifact 'native\Sunny'))
 foreach($name in @('Sunny.ps1','SunnyRemote.ps1','SunnyClient.ps1','SunnyDoctor.ps1')){[IO.File]::Copy((Join-Path $root $name),(Join-Path $artifact ('installer\windows\'+$name)))}
 Text (Join-Path $artifact 'native\Sunny\__init__.py') '# literal bridge fixture'
 Text (Join-Path $artifact 'native\Sunny\bridge_contract.json') '{"bridge_protocol_version":47,"target_snapshot_schema_version":35}'
 Text (Join-Path $artifact 'native\Sunny\source.sha256') ('a'*64)
 $artifact=(Get-Item -LiteralPath $artifact).FullName;$inventory=@();$bridgeFiles=@{}
 foreach($item in Get-ChildItem -LiteralPath $artifact -File -Recurse){
  $relative=$item.FullName.Substring($artifact.Length+1).Replace('\','/');$identity=@{path=$relative;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName).Hash.ToLowerInvariant()};$inventory+=$identity
  if($relative.StartsWith('native/Sunny/')){$bridgeFiles[$item.Name]=@{bytes=$item.Length;sha256=$identity.sha256}}
 }
 $manifest=@{release_manifest_schema_version=1;product=@{name='Sunny';version='literal'};source=@{revision=('a'*40)};image=@{platform='linux/amd64';local_immutable_id=('sha256:'+('a'*64))};bridge=@{source_sha256=('a'*64);contract=@{bridge_protocol_version=47;target_snapshot_schema_version=35};files=$bridgeFiles};files=$inventory}
 Text (Join-Path $artifact 'release.json') ($manifest|ConvertTo-Json -Depth 10 -Compress)
 $hash=(Get-FileHash -LiteralPath (Join-Path $artifact 'release.json')).Hash.ToLowerInvariant()
 $captured=Get-SunnyDoctorSources $artifact $hash;Check ($captured.sources.Count-eq4) 'Exact sources not captured'
 Fails {Get-SunnyDoctorSources $artifact ('b'*64)} 'checksum'
 Fails {Invoke-SunnyDoctor '' $artifact $hash 'literal' '' '' 'fixture' 1 5 ''} 'Connection|connection|path|Path'
 $clientCopy=Join-Path $artifact 'installer\windows\SunnyClient.ps1';$original=[IO.File]::ReadAllBytes($clientCopy);Text $clientCopy 'throw "unverified script must not execute"'
 Fails {Get-SunnyDoctorSources $artifact $hash} 'changed';[IO.File]::WriteAllBytes($clientCopy,$original)
 $recovery=Join-Path $root ('Sunny-doctor-'+[Guid]::NewGuid().ToString('N'));$null=[IO.Directory]::CreateDirectory($recovery)
 $state=@{schema_version=1;owner_sid=Get-SunnyOwner;owner=[IO.Path]::GetFileName($recovery).Substring(13);parent_pid=$PID;parent_started=(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks.ToString();child_pid=$success.journal.parent_pid;child_started=$success.journal.parent_started;client_scope=$success.state.client_scope;cleanup_confirmed=$false;manifest_sha256=$hash;operator_sources=$captured.identities}
 foreach($name in $captured.sources.Keys){[IO.File]::WriteAllBytes((Join-Path $recovery $name),$captured.sources[$name])}
 $recoverPath=Join-Path $recovery 'doctor.json';Write-SunnyDoctorState $recoverPath $state -Initial
 Fails {Recover-SunnyDoctor $recovery $artifact $hash 5} 'still running'
 $state.schema_version=2;Write-SunnyDoctorState $recoverPath $state;Fails {Recover-SunnyDoctor $recovery $artifact $hash 5} 'integer'
 Check (Test-Path -LiteralPath $recoverPath) 'Future-schema recovery removed evidence'
 $passed.Add('exact_verified_release_sources_and_failclosed_recovery_admission')
 $bootstrapDirectory=Join-Path $root 'literal verified launcher Café';$null=[IO.Directory]::CreateDirectory($bootstrapDirectory)
 foreach($name in @('Sunny.ps1','SunnyRemote.ps1')){[IO.File]::Copy((Join-Path $root $name),(Join-Path $bootstrapDirectory $name))}
 $probeSource=@'
param($Action,$ConnectionFile,$ReleaseDirectory,$ExpectedManifestSHA256,$ImageId,$ConfigurationFile,$WorkspaceVolume,$DockerContext,$StartupSeconds,$CleanupSeconds)
[IO.File]::WriteAllText($ConnectionFile,[string]$PID)
[Console]::Error.WriteLine($ConfigurationFile)
[Console]::Out.WriteLine((@{config=$ConfigurationFile;action=$Action;image=$ImageId;context=$DockerContext}|ConvertTo-Json -Compress))
if($WorkspaceVolume-eq'sleep'){Start-Sleep -Seconds 60}
if($WorkspaceVolume-eq'bad_exit'){exit 9};exit 0
'@
 $probePath=Join-Path $bootstrapDirectory 'SunnyClient.ps1';Text $probePath $probeSource
 $probeIdentities=@{};foreach($name in @('Sunny.ps1','SunnyRemote.ps1','SunnyClient.ps1')){$probeIdentities[$name]=(Get-FileHash -LiteralPath (Join-Path $bootstrapDirectory $name)).Hash.ToLowerInvariant()}
 $literalConfig='literal Café $() ; [never execute].json'
 function Bootstrap([string]$Mode,[switch]$Drift){
  $marker=Join-Path $bootstrapDirectory ('probe-'+[Guid]::NewGuid().ToString('N')+'.json')
  $args=New-SunnyDoctorLauncherArguments $bootstrapDirectory $probeIdentities $marker $artifact $hash ('sha256:'+('a'*64)) $literalConfig $Mode 'fixture' 5
  if($Drift){Text $probePath '[IO.File]::WriteAllText("must never execute","foreign")'}
  $command=(@($shell)+$args|ForEach-Object{ConvertTo-SunnyWindowsArgument $_})-join' ';$child=$null
  $out=[IO.MemoryStream]::new();$err=[IO.MemoryStream]::new()
  try{
   $child=[SunnyDoctorProcess]::new($shell,$command,$bootstrapDirectory);$clock=[Diagnostics.Stopwatch]::StartNew()
   $outTask=$child.Output.CopyToAsync($out);$errTask=$child.Error.CopyToAsync($err)
   if($Mode-ceq'sleep'){
    while(-not(Test-Path -LiteralPath $marker)-and$clock.Elapsed.TotalSeconds-lt5){Start-Sleep -Milliseconds 5}
    Check (Test-Path -LiteralPath $marker) 'Verified data launcher did not start'
    Fails {Text (Join-Path $bootstrapDirectory 'SunnyRemote.ps1') 'changed executable helper'} 'used by another process|being used'
    $child.Terminate()
   }
   while((-not$child.Process.HasExited-or-not$child.Empty)-and$clock.Elapsed.TotalSeconds-lt6){Start-Sleep -Milliseconds 5}
   Check ($child.Process.HasExited-and$child.Empty) ('Bootstrap process was not finitely reaped: '+$Mode)
   while((-not$outTask.IsCompleted-or-not$errTask.IsCompleted)-and$clock.Elapsed.TotalSeconds-lt8){Start-Sleep -Milliseconds 5}
   Check ($outTask.IsCompleted-and$errTask.IsCompleted) 'Bootstrap streams did not close'
   $null=$outTask.GetAwaiter().GetResult();$null=$errTask.GetAwaiter().GetResult()
   return @{code=$child.Process.ExitCode;output=[Text.UTF8Encoding]::new($false,$true).GetString($out.ToArray());error=[Text.UTF8Encoding]::new($false,$true).GetString($err.ToArray());marker=$marker}
  }finally{if($null-ne$child){$child.Dispose()};$out.Dispose();$err.Dispose();if($Drift){Text $probePath $probeSource}}
 }
 $probe=Bootstrap 'normal';$decoded=ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes($probe.output.Trim()))
 Check ($probe.code-eq0-and$decoded.config-ceq$literalConfig-and$decoded.action-ceq'Run'-and$probe.error.Trim()-ceq$literalConfig) ('Unicode/data-only fixed bootstrap altered literal input or encoding: '+($probe|ConvertTo-Json -Compress))
 $bad=Bootstrap 'bad_exit';Check ($bad.code-eq9) 'Fixed bootstrap hid launcher failure exit'
 $drift=Bootstrap 'normal' -Drift;Check ($drift.code-ne0-and-not(Test-Path -LiteralPath $drift.marker)-and-not$drift.output) 'Changed launcher executed after capture'
 $sleep=Bootstrap 'sleep';Check ($sleep.code-ne0) 'Held source bootstrap was not exactly terminated'
 $passed.Add('fixed_base64_data_utf8_launcher_holds_verified_bytes_and_propagates_exit')

 Fails {Assert-SunnyDoctorPolling ([double]::NaN) 0.25 $false} 'finite'
 Fails {Assert-SunnyDoctorPolling 1 ([double]::PositiveInfinity) $false} 'finite'
 Fails {Assert-SunnyDoctorPolling 0 0.25 $true} 'finite'
 foreach($json in @('{"text":"\ud800"}','{"text":"\udc00"}','{"text":"\ud800x\udc00"}')){
  Fails {ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes($json))} 'surrogate'
 }
 $literalEscape=ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes('{"text":"\\ud800"}'))
 Check ($literalEscape.text-ceq'\ud800') 'Literal escaped backslash was mistaken for a Unicode escape'
 $pairedEscape=ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes('{"text":"\ud83d\ude00"}'))
 Check ($pairedEscape.text-ceq[char]::ConvertFromUtf32(0x1f600)) 'Valid escaped surrogate pair was refused'
 # The callback contract owns exact backoff timing. A public Stopwatch subclass
 # advances only at the pump, so OS/pipe throughput cannot decide this check.
 Add-Type -TypeDefinition 'public class SunnyDoctorFixtureClock : System.Diagnostics.Stopwatch { private long ticks; public new System.TimeSpan Elapsed { get { return System.TimeSpan.FromTicks(ticks); } } public void Advance(){ticks+=100000;} }'
 $backoffClock=[SunnyDoctorFixtureClock]::new();$backoffTimes=[Collections.Generic.List[double]]::new();$backoffWaits=[Collections.Generic.List[bool]]::new()
 $emptyCall={param($Cursor)
  $backoffTimes.Add($backoffClock.Elapsed.TotalSeconds)
  return @{success=$true;entries=@();next_sequence=0;latest_sequence=0;oldest_sequence=1;stream_id=('d'*32);reset=$false;truncated=$false;observed_at=([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0);has_more=$false}
 }
 $backoffPump={param([switch]$Waiting);$backoffWaits.Add([bool]$Waiting);$backoffClock.Advance()}
 $backoff=Invoke-SunnyDoctorPolling $emptyCall $backoffPump $backoffClock 0.4 0.05 $false
 Check ($backoff.requests-eq3-and-not$backoff.stale-and-not$backoff.incomplete-and$backoffTimes.Count-eq3-and$backoffTimes[0]-eq0-and-not$backoffWaits.Contains($false)) 'Controlled empty polling did not retain fresh finite observations'
 # Each doubled wait is observed within one 10 ms logical-clock tick; the same
 # tick bounds floating-point wake/deadline rounding without a throughput floor.
 Check ($backoffTimes[1]-$backoffTimes[0]-ge0.099999-and$backoffTimes[1]-$backoffTimes[0]-le0.110001-and$backoffTimes[2]-$backoffTimes[1]-ge0.199999-and$backoffTimes[2]-$backoffTimes[1]-le0.210001-and$backoffClock.Elapsed.TotalSeconds-ge0.399999-and$backoffClock.Elapsed.TotalSeconds-le0.410001) 'Controlled empty polling did not double .1/.2 backoff within its finite deadline'
 $fixturePhase='empty_log_polling'
 $empty=Run 'log_empty' 8 0.4
 $emptyLogs=$empty.report.logs;$hasEmptyLogs=$emptyLogs-is[Collections.IDictionary]
 $emptyRequests=if([IO.File]::Exists($empty.marker+'.logrequests')){[IO.File]::ReadAllLines($empty.marker+'.logrequests').Count}else{0}
 $healthyEmpty=$empty.report.success-and$empty.report.read_only_ready-and$empty.report.cleanup.success-and$empty.report.cleanup.owned_job_absent-and$hasEmptyLogs-and-not$emptyLogs.stale-and-not$emptyLogs.incomplete-and$emptyLogs.requests-ge1-and$emptyLogs.requests-le4-and$emptyLogs.requests-eq$emptyRequests-and$emptyLogs.cursor.after_sequence-eq0-and$emptyLogs.cursor.stream_id-ceq('d'*32)-and$emptyLogs.failures.Count-eq0-and$emptyLogs.resets-eq0-and$emptyLogs.gaps-eq0-and$emptyLogs.records_seen-eq0-and$emptyLogs.omitted_records-eq0
 if(-not$healthyEmpty){
  # Closed booleans/counters and fixed codes only; no messages, argv or records.
  $facts=[ordered]@{summary_schema_version=1;received_requests=$emptyRequests;log_null=($null-eq$emptyLogs)}
  foreach($field in @('success','read_only_ready')){$facts[$field]=if($empty.report[$field]-is[bool]){$empty.report[$field]}else{$null}}
  foreach($field in @('stale','incomplete')){$facts[$field]=if($hasEmptyLogs-and$emptyLogs.Contains($field)-and$emptyLogs[$field]-is[bool]){$emptyLogs[$field]}else{$null}}
  foreach($field in @('requests','resets','gaps','records_seen','omitted_records')){$facts[$field]=if($hasEmptyLogs-and$emptyLogs.Contains($field)-and($emptyLogs[$field]-is[int]-or$emptyLogs[$field]-is[long])){$emptyLogs[$field]}else{$null}}
  $facts.cleanup_success=if($empty.report.cleanup.success-is[bool]){$empty.report.cleanup.success}else{$null}
  $facts.owned_job_absent=if($empty.report.cleanup.owned_job_absent-is[bool]){$empty.report.cleanup.owned_job_absent}else{$null}
  $facts.check_codes=@(foreach($check in $empty.report.checks){if($check.code-in@('owned_process_spawned','protocol_exchange_passed','diagnostic_tools_available','log_poll_incomplete','diagnosis_incomplete','protocol_close_incomplete','owned_cleanup_unconfirmed','recovery_checkpoint_failed')){$check.code}else{'unclassified_code'}})
  $facts.log_failure_codes=if($hasEmptyLogs){@(foreach($failure in $emptyLogs.failures){if($failure.code-ceq'log_observation_unavailable'){$failure.code}else{'unclassified_code'}})}else{@()}
  [Console]::Error.WriteLine('SUNNY_DOCTOR_EMPTY_POLL_FAILURE '+($facts|ConvertTo-Json -Depth 3 -Compress))
 }
 Check $healthyEmpty 'Finite valid-empty backoff failed'
 $fixturePhase='protocol_fixture_checks'
 $pages=Run 'log_pages' 8 0.5
 Check ($pages.report.success-and$pages.report.logs.records_seen-eq3-and$pages.report.logs.cursor.after_sequence-eq3) 'Contiguous pagination failed'
 $arguments=@([IO.File]::ReadAllLines($pages.marker+'.logrequests')|ForEach-Object{ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes($_))})
 Check ($arguments[0].Count-eq1-and$arguments[0].after_sequence-eq0-and$arguments[1].after_sequence-eq1-and$arguments[1].stream_id-ceq('d'*32)-and$arguments[2].after_sequence-eq3) 'Polling cursor arguments are not exact'
 foreach($row in $pages.report.logs.records){Check (-not$row.Contains('message')) 'Default poll exported messages'}
 Check $pages.report.logs.project_content_possible 'Caller-controlled metadata content boundary absent'
 $structuredLogs=Run 'log_structured' 8 0.3 0.25;Check $structuredLogs.report.success 'Equivalent structured log payload failed'
 $passed.Add('finite_empty_backoff_contiguous_pagination_and_messages_absent_default')
 foreach($mode in @('log_gap','log_rotate','log_ahead')){
  $loss=Run $mode 8 0.8 0.25;Check (-not$loss.report.success-and-not$loss.report.read_only_ready-and$loss.report.logs.incomplete-and-not$loss.report.logs.stale-and$loss.report.logs.records_seen-gt0) ('Visible stream loss cleared: '+$mode)
  if($mode-eq'log_gap'){Check ($loss.report.logs.gaps-ge1) 'Gap counter absent'}else{Check ($loss.report.logs.resets-eq1) 'Reset counter absent'}
 }
 $passed.Add('reset_rollover_ahead_cursor_and_gap_facts_stay_incomplete_after_progress')
 foreach($mode in @('log_stale','log_unknown','log_legacy','log_bool','log_seqfloat','log_seq_array','log_gap_hidden','log_reset_hidden','log_hasmore_bad','log_next_bad','log_unordered','log_duplicate','log_unpaired','log_error')){
  $badLogs=Run $mode 8 0.8 0.25
  Check (-not$badLogs.report.success-and$badLogs.report.logs.stale-and$badLogs.report.logs.incomplete-and$badLogs.report.logs.failures.Count-eq1) ('Invalid log evidence accepted: '+$mode)
  Check (($badLogs.report|ConvertTo-Json -Depth 15 -Compress)-notmatch'raw-native-private-error') 'Raw native error exported'
 }
 $unicode=Run 'log_unicode' 8 0.6 0.25;Check ($unicode.report.success-and$unicode.report.logs.records_seen-eq1) 'Valid supplementary Unicode metadata rejected'
 $passed.Add('strict_log_page_types_fields_timestamps_unicode_and_loss_algebra')
 $watch=[Diagnostics.Stopwatch]::StartNew();$withheld=Run 'log_timeout' 8 0.25
 Check (-not$withheld.report.success-and$withheld.report.logs.requests-eq1-and$withheld.report.cleanup.success-and$watch.Elapsed.TotalSeconds-lt5) 'Withheld page retried or exceeded bounded owned cleanup'
 $seen=[IO.File]::ReadAllLines($withheld.marker+'.logrequests');Check ($seen.Count-eq1) 'Uncertain log request overlapped/retried'
 $messages=Run 'log_messages' 8 0.6 0.25 $true
 Check ($messages.report.success-and$messages.report.logs.include_messages-and$messages.report.logs.project_content_possible-and$messages.report.logs.records[0].Contains('message')) 'Explicit message retention unavailable'
 Check (($messages.report.logs.records|ConvertTo-Json -Depth 6)-notmatch'literal private|private-token|private-pass|private-bearer|sk-abcdefghijk|ghp_abcdefghijk') 'Deliberate log messages were not redacted'
 $many=Run 'log_many' 20 7 0.05 $true
 Check ($many.report.logs.records_seen-eq300-and$many.report.logs.omitted_records-gt0-and$many.report.logs.incomplete-and-not$many.report.success) 'Record byte bound silently omitted content or changed readiness'
 $messageReport=Join-Path $root 'deliberate logs.json';Export-SunnyDoctorReport $messageReport $messages.report
 Check ((Get-Item -LiteralPath $messageReport).Length-le1048576-and[IO.File]::ReadAllText($messageReport)-notmatch'private-token|private-pass|private-bearer') 'Explicit log export was unbounded/unredacted'
 $pending=Run 'log_pending' 8 0.25;Check ($pending.report.logs.incomplete-and-not$pending.report.success) 'Outstanding pagination claimed complete at deadline'
 $passed.Add('poll_deadline_no_retry_retention_caps_and_deliberate_redacted_export')
 $capWatch=[Diagnostics.Stopwatch]::new()
 $literalCall={param($Cursor)
  $n=$Cursor.after_sequence+1
  return @{success=$true;entries=@(@{sequence=$n;time=0;level='INFO';source='literal';message='literal'});next_sequence=$n;latest_sequence=($n+1);oldest_sequence=1;stream_id=('d'*32);reset=$false;truncated=$false;has_more=$true;observed_at=([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0)}
 }
 $capped=Invoke-SunnyDoctorPolling $literalCall {} $capWatch 1 0.05 $false
 Check ($capped.requests-eq1000-and$capped.incomplete-and$capped.cursor.after_sequence-eq1000) 'Deterministic continuously available producer escaped request cap'
 # A deliberately delayed detail sink is observed at the next budget check;
 # this exercises uncertainty after a validated page, not kernel preemption.
 $protect=(Get-Command Protect-SunnyDoctorValues).ScriptBlock
 try{
  function Protect-SunnyDoctorValues($Value){Start-Sleep -Milliseconds 250;return $Value}
  $detailCall={param($Cursor)
   return @{success=$true;entries=@(foreach($n in 1..3){@{sequence=$n;time=0;level='INFO';source='literal';message='literal'}});next_sequence=3;latest_sequence=3;oldest_sequence=1;stream_id=('d'*32);reset=$false;truncated=$false;has_more=$false;observed_at=([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0)}
  }
  $detail=Invoke-SunnyDoctorPolling $detailCall {} ([Diagnostics.Stopwatch]::StartNew()) 0.2 0.05 $false
  Check ($detail.requests-eq1-and$detail.incomplete-and$detail.cursor.after_sequence-eq3-and$detail.records_seen-eq3-and$detail.omitted_records-gt0) 'Detail deadline lost the independently validated cursor or omitted loss facts'
 }finally{Set-Item Function:Protect-SunnyDoctorValues $protect}
 $passed.Add('deterministic_continuous_producer_cannot_escape_the_finite_request_cap')

 # Actual local Docker daemon orphan: only the captured name/CID/label is
 # removed. An independent foreign owner and its container remain untouched.
 $docker=(Get-Command docker.exe -ErrorAction Stop).Source;$dockerWitness=$false
 $fixturePhase='docker_busybox_availability'
 $available=Invoke-SunnyBoundedProcess $docker @('--context','desktop-linux','image','inspect','busybox:1.36') '' 5
 if($available.exit_code-eq0){
  $fixtureVolume='sunny-doctor-fixture-'+[Guid]::NewGuid().ToString('N')
  $fixturePhase='docker_volume_create'
  $null=Invoke-SunnyClientDocker $docker 'desktop-linux' @('volume','create',$fixtureVolume) 10
  $fixturePhase='docker_volume_seed'
  $null=Invoke-SunnyClientDocker $docker 'desktop-linux' @('run','--rm','--mount',('type=volume,source='+$fixtureVolume+',target=/data'),'busybox:1.36','sh','-c','printf durable-fixture > /data/sentinel') 10
  $env:SUNNY_DOCTOR_FIXTURE_VOLUME=$fixtureVolume
  $fixturePhase='docker_foreign_create'
  $foreignOwner=[Guid]::NewGuid().ToString('N');$foreign=Invoke-SunnyClientDocker $docker 'desktop-linux' @('run','-d','--name',('sunny-client-'+$foreignOwner+'-run'),'--label',('org.sunny.client.owner='+$foreignOwner),'busybox:1.36','sleep','90') 10
  $dockerFailure=$null
  try{
   $fixturePhase='docker_owned_orphan_exchange'
   $script:realDocker=$true;$orphan=Run 'docker_orphan' 12;$script:realDocker=$false
   Check ($orphan.report.success-and$orphan.report.cleanup.container_absence_confirmed) 'Exact real daemon orphan cleanup failed'
   $fixturePhase='docker_owned_absence'
   $cid=[IO.File]::ReadAllText($orphan.marker+'.cid');$remaining=Invoke-SunnyClientDocker $docker 'desktop-linux' @('ps','-aq','--no-trunc','--filter',('id='+$cid)) 5
   Check (-not$remaining) 'Owned real container remains'
   $fixturePhase='docker_foreign_readback'
   $foreignStill=Invoke-SunnyClientDocker $docker 'desktop-linux' @('ps','-q','--no-trunc','--filter',('id='+$foreign)) 5
   Check ($foreignStill-ceq$foreign) 'Foreign actual daemon container changed'
   # Recreate only this fixture's proved owner as an interrupted-session
   # orphan. Recovery uses actual expired process identities, never a name glob.
   $fixturePhase='docker_recovery_orphan_create'
   $residual=Invoke-SunnyClientDocker $docker 'desktop-linux' @('run','-d','--name',('sunny-client-'+$orphan.journal.owner+'-run'),'--label',('org.sunny.client.owner='+$orphan.journal.owner),'busybox:1.36','sleep','90') 10
   $residualFailure=$null
   try{
    $fixturePhase='expired_doctor_pid_peer'
    $identity=Invoke-SunnyBoundedProcess $shell @('-NoProfile','-NonInteractive','-Command','@{pid=$PID;started=(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks.ToString()}|ConvertTo-Json -Compress') '' 5
    $expired=ConvertFrom-SunnyDoctorJson ([Text.Encoding]::UTF8.GetBytes($identity.stdout.Trim()))
    Check ($identity.exit_code-eq0-and$null-eq(Get-Process -Id $expired.pid -ErrorAction SilentlyContinue)) 'Independent former doctor process did not exit'
    $recoverDirectory=Join-Path $root ('Sunny-doctor-'+[Guid]::NewGuid().ToString('N'));$null=[IO.Directory]::CreateDirectory($recoverDirectory)
    foreach($name in $captured.sources.Keys){[IO.File]::WriteAllBytes((Join-Path $recoverDirectory $name),$captured.sources[$name])}
    $recoverState=@{schema_version=1;owner_sid=Get-SunnyOwner;owner=[IO.Path]::GetFileName($recoverDirectory).Substring(13);parent_pid=$expired.pid;parent_started=$expired.started;child_pid=$orphan.journal.parent_pid;child_started=$orphan.journal.parent_started;client_scope=$orphan.state.client_scope;cleanup_confirmed=$false;manifest_sha256=$hash;operator_sources=$captured.identities}
    $recoverFile=Join-Path $recoverDirectory 'doctor.json';Write-SunnyDoctorState $recoverFile $recoverState -Initial
    $realDaemon=$recoverState.client_scope.daemon_id;$recoverState.client_scope.daemon_id='different-literal-daemon';Write-SunnyDoctorState $recoverFile $recoverState
    $fixturePhase='docker_recovery_changed_daemon_refusal'
    Fails {Recover-SunnyDoctor $recoverDirectory $artifact $hash 10} 'daemon identity changed'
    Check (Test-Path -LiteralPath $recoverFile) 'Changed daemon recovery discarded ownership record'
    $recoverState.client_scope.daemon_id=$realDaemon;Write-SunnyDoctorState $recoverFile $recoverState
    Text (Join-Path $recoverDirectory 'user.als') 'independent user bytes'
    $fixturePhase='docker_recovery_foreign_file_refusal'
    Fails {Recover-SunnyDoctor $recoverDirectory $artifact $hash 10} 'Unexpected'
    Check ([IO.File]::ReadAllText((Join-Path $recoverDirectory 'user.als'))-ceq'independent user bytes') 'Unexpected user content changed'
    [IO.File]::Delete((Join-Path $recoverDirectory 'user.als'))
    $fixturePhase='docker_exact_recovery'
    $receipt=Recover-SunnyDoctor $recoverDirectory $artifact $hash 10
    Check ($receipt.containers_absent-and$receipt.recorded_processes_absent-and-not$receipt.project_volume_removed-and-not(Test-Path -LiteralPath $recoverDirectory)) 'Exact durable recovery failed'
    $fixturePhase='docker_recovered_absence'
    $remaining=Invoke-SunnyClientDocker $docker 'desktop-linux' @('ps','-aq','--no-trunc','--filter',('id='+$residual)) 5
    Check (-not$remaining) 'Recovered original-daemon fixture container remains'
    $fixturePhase='docker_recovery_foreign_readback'
    $foreignStill=Invoke-SunnyClientDocker $docker 'desktop-linux' @('ps','-q','--no-trunc','--filter',('id='+$foreign)) 5
    Check ($foreignStill-ceq$foreign) 'Recovery changed foreign actual daemon container'
    $fixturePhase='docker_volume_readback'
    $sentinel=Invoke-SunnyClientDocker $docker 'desktop-linux' @('run','--rm','--mount',('type=volume,source='+$fixtureVolume+',target=/data,readonly'),'busybox:1.36','cat','/data/sentinel') 10
    Check ($sentinel-ceq'durable-fixture') 'Doctor cleanup or recovery altered project-volume bytes'
   }catch{
    $residualFailure=$_;if($null-eq$fixturePrimaryPhase){$fixturePrimaryPhase=$fixturePhase};WriteFixtureFailure $_ $fixturePrimaryPhase 'operation_failure';throw
   }finally{
    $fixturePhase='docker_residual_cleanup'
    try{
     $residualLeft=Invoke-SunnyClientDocker $docker 'desktop-linux' @('ps','-aq','--no-trunc','--filter',('id='+$residual)) 5
     if($residualLeft){$null=Invoke-SunnyClientDocker $docker 'desktop-linux' @('container','rm','--force',$residual) 10}
    }catch{WriteFixtureFailure $_ $fixturePhase 'cleanup_failure';if($null-eq$residualFailure){throw}}
   }
   $dockerWitness=$true
  }catch{
   $dockerFailure=$_;if($null-eq$fixturePrimaryPhase){$fixturePrimaryPhase=$fixturePhase};WriteFixtureFailure $_ $fixturePrimaryPhase 'operation_failure';throw
  }finally{
   $fixturePhase='docker_foreign_and_volume_cleanup';$script:realDocker=$false
   try{$null=Invoke-SunnyClientDocker $docker 'desktop-linux' @('container','rm','--force',$foreign) 10;$env:SUNNY_DOCTOR_FIXTURE_VOLUME=$null;$null=Invoke-SunnyClientDocker $docker 'desktop-linux' @('volume','rm',$fixtureVolume) 10}
   catch{WriteFixtureFailure $_ $fixturePhase 'cleanup_failure';if($null-eq$dockerFailure){throw}}
  }
 }
 if($dockerWitness){$passed.Add('actual_local_docker_orphan_exact_cleanup_and_foreign_preservation')}else{$passed.Add('docker_orphan_fixture_not_run_without_preloaded_busybox')}
 @{passed=$passed;ssh_executed=$false;native_live_executed=$false;physical_kernel_stalls_tested=$false;incremental_polling_fixtures_passed=$true;actual_docker_witness=$dockerWitness}|ConvertTo-Json -Depth 5 -Compress
}catch{
 $fixtureFailure=$_;if($null-eq$fixturePrimaryPhase){$fixturePrimaryPhase=$fixturePhase};WriteFixtureFailure $_ $fixturePrimaryPhase 'operation_failure';throw
}finally{
 # Fixture-only teardown; production preserves every unproved directory.
 try{
  foreach($item in Get-ChildItem -LiteralPath $root -Filter 'peer.json' -Recurse -ErrorAction SilentlyContinue){
   try{$record=Get-Content -LiteralPath $item.FullName -Raw|ConvertFrom-Json;$path=Join-Path ([IO.Path]::GetTempPath()) ('Sunny-client-'+$record.owner);if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Recurse -Force}}catch{}
  }
  if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
 }catch{WriteFixtureFailure $_ 'fixture_directory_cleanup' 'cleanup_failure';if($null-eq$fixtureFailure){throw}}
}
