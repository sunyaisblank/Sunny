[CmdletBinding()]
param(
    [ValidateSet('Diagnose','Recover')][string]$Action='Diagnose',
    [string]$ConnectionFile,[string]$ReleaseDirectory,[string]$ExpectedManifestSHA256,
    [string]$ImageId,[string]$ConfigurationFile,[string]$WorkspaceVolume,
    [string]$DockerContext='desktop-linux',
    [ValidateRange(1,120)][int]$TimeoutSeconds=30,
    [ValidateRange(5,30)][int]$CleanupSeconds=15,
    [string]$ExportPath,[string]$SessionDirectory,
    [ValidateRange(0,60)][double]$PollSeconds=0,
    [ValidateRange(0.05,5)][double]$PollInterval=0.25,[switch]$IncludeMessages
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$script:SunnyDoctorScriptFile=$PSCommandPath

# One fixed verified Windows launcher, no arbitrary command/tool, no Python.
# Optional log reads stay in the same owned session and share its finite deadline.
function Initialize-SunnyDoctorTypes {
 if ('SunnyDoctorProcess' -as [type]) { return }
 Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
public sealed class SunnyDoctorProcess : IDisposable {
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
  public uint cb; public string reserved, desktop, title; public uint x,y,xSize,ySize,xChars,yChars,fill,flags;
  public ushort show,reservedBytes; public IntPtr reservedData,input,output,error;
 }
 [StructLayout(LayoutKind.Sequential)] struct StartupEx { public Startup startup;public IntPtr attributes; }
 [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr process,thread; public uint pid,tid; }
 [StructLayout(LayoutKind.Sequential)] struct BasicLimits {
  public long processTime,jobTime; public uint flags; public UIntPtr minimum,maximum; public uint active;
  public UIntPtr affinity; public uint priority,scheduling;
 }
 [StructLayout(LayoutKind.Sequential)] struct Io { public ulong readOps,writeOps,otherOps,readBytes,writeBytes,otherBytes; }
 [StructLayout(LayoutKind.Sequential)] struct Limits { public BasicLimits basic; public Io io; public UIntPtr processMemory,jobMemory,peakProcess,peakJob; }
 [StructLayout(LayoutKind.Sequential)] struct Accounting { public long user,kernel,periodUser,periodKernel; public uint faults,total,active,terminated; }
 [StructLayout(LayoutKind.Sequential)] struct FileInfo { public uint attributes,creationLow,creationHigh,accessLow,accessHigh,writeLow,writeHigh,volume,sizeHigh,sizeLow,links,indexHigh,indexLow; }
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetFileInformationByHandle(SafeFileHandle file,out FileInfo info);
 [DllImport("kernel32.dll", SetLastError=true)] static extern SafeFileHandle CreateJobObject(IntPtr security,string name);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetInformationJobObject(SafeFileHandle job,int kind,ref Limits limits,uint size);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool AssignProcessToJobObject(SafeFileHandle job,IntPtr process);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool QueryInformationJobObject(SafeFileHandle job,int kind,out Accounting info,uint size,IntPtr returned);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool TerminateJobObject(SafeFileHandle job,uint code);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool InitializeProcThreadAttributeList(IntPtr list,int count,uint flags,ref IntPtr size);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool UpdateProcThreadAttribute(IntPtr list,uint flags,IntPtr attribute,IntPtr value,UIntPtr size,IntPtr previous,IntPtr returned);
 [DllImport("kernel32.dll")] static extern void DeleteProcThreadAttributeList(IntPtr list);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CreateProcess(string application,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr environment,string directory,ref StartupEx startup,out ProcessInfo info);
 [DllImport("kernel32.dll", SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool TerminateProcess(IntPtr process,uint code);
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 readonly SafeFileHandle job;
 IntPtr suspendedThread=IntPtr.Zero;
 public readonly AnonymousPipeServerStream Input,Output,Error;
 public Process Process { get; private set; }
 public bool Empty {
  get { Accounting info;if(!QueryInformationJobObject(job,1,out info,(uint)Marshal.SizeOf(typeof(Accounting)),IntPtr.Zero)) throw new System.ComponentModel.Win32Exception();return info.active==0; }
 }
 public SunnyDoctorProcess(string executable,string command,string directory):this(executable,command,directory,true) {}
 public SunnyDoctorProcess(string executable,string command,string directory,bool resume) {
  job=CreateJobObject(IntPtr.Zero,null);
  if(job.IsInvalid) throw new System.ComponentModel.Win32Exception();
  ProcessInfo info=new ProcessInfo();
  IntPtr attributes=IntPtr.Zero,handles=IntPtr.Zero;bool initialized=false;
  try {
   var limits=new Limits();limits.basic.flags=0x2000;
   if(!SetInformationJobObject(job,9,ref limits,(uint)Marshal.SizeOf(typeof(Limits)))) throw new System.ComponentModel.Win32Exception();
   Input=new AnonymousPipeServerStream(PipeDirection.Out,HandleInheritability.Inheritable);
   Output=new AnonymousPipeServerStream(PipeDirection.In,HandleInheritability.Inheritable);
   Error=new AnonymousPipeServerStream(PipeDirection.In,HandleInheritability.Inheritable);
   IntPtr size=IntPtr.Zero;InitializeProcThreadAttributeList(IntPtr.Zero,1,0,ref size);
   if(size==IntPtr.Zero)throw new System.ComponentModel.Win32Exception();
   attributes=Marshal.AllocHGlobal(size);
   if(!InitializeProcThreadAttributeList(attributes,1,0,ref size))throw new System.ComponentModel.Win32Exception();initialized=true;
   handles=Marshal.AllocHGlobal(3*IntPtr.Size);
   var startup=new StartupEx { attributes=attributes,startup=new Startup { cb=(uint)Marshal.SizeOf(typeof(StartupEx)),flags=0x100,
    input=Input.ClientSafePipeHandle.DangerousGetHandle(),output=Output.ClientSafePipeHandle.DangerousGetHandle(),error=Error.ClientSafePipeHandle.DangerousGetHandle() } };
   Marshal.WriteIntPtr(handles,0,startup.startup.input);Marshal.WriteIntPtr(handles,IntPtr.Size,startup.startup.output);Marshal.WriteIntPtr(handles,2*IntPtr.Size,startup.startup.error);
   if(!UpdateProcThreadAttribute(attributes,0,(IntPtr)0x20002,handles,(UIntPtr)(3*IntPtr.Size),IntPtr.Zero,IntPtr.Zero))throw new System.ComponentModel.Win32Exception();
   // No code runs before exclusive job admission. All descendant handles inherit
   // the job; the daemon container still needs separate exact ownership cleanup.
   if(!CreateProcess(executable,new StringBuilder(command),IntPtr.Zero,IntPtr.Zero,true,0x08080004,IntPtr.Zero,directory,ref startup,out info)) throw new System.ComponentModel.Win32Exception();
   if(!AssignProcessToJobObject(job,info.process)) throw new System.ComponentModel.Win32Exception();
   Process=Process.GetProcessById((int)info.pid);
   // Retain the exact process handle before resume; GetProcessById alone may
   // lose the exit code when a short-lived process exits before a later query.
   var retained=Process.Handle;
   Input.DisposeLocalCopyOfClientHandle();Output.DisposeLocalCopyOfClientHandle();Error.DisposeLocalCopyOfClientHandle();
   suspendedThread=info.thread;info.thread=IntPtr.Zero;
   if(resume)Resume();
  } catch {
   if(info.process!=IntPtr.Zero) TerminateProcess(info.process,125);
   Dispose();throw;
  } finally { if(initialized)DeleteProcThreadAttributeList(attributes);if(attributes!=IntPtr.Zero)Marshal.FreeHGlobal(attributes);if(handles!=IntPtr.Zero)Marshal.FreeHGlobal(handles);if(info.thread!=IntPtr.Zero)CloseHandle(info.thread);if(info.process!=IntPtr.Zero)CloseHandle(info.process); }
 }
 public void Resume() {
  if(suspendedThread==IntPtr.Zero)throw new InvalidOperationException("Diagnostic child is not suspended.");
  if(ResumeThread(suspendedThread)==UInt32.MaxValue)throw new System.ComponentModel.Win32Exception();
  CloseHandle(suspendedThread);suspendedThread=IntPtr.Zero;
 }
 public void Terminate() { if(!TerminateJobObject(job,125))throw new System.ComponentModel.Win32Exception(); }
 public static void RequireSingleLink(SafeFileHandle file) { FileInfo info;if(!GetFileInformationByHandle(file,out info))throw new System.ComponentModel.Win32Exception();if(info.links!=1)throw new IOException("Linked diagnostic files are unsupported."); }
 public static int ScalarCount(string text) { int count=0;for(int i=0;i<text.Length;i++){if(Char.IsHighSurrogate(text[i])){if(i+1>=text.Length||!Char.IsLowSurrogate(text[++i]))throw new IOException("Unpaired diagnostic surrogate.");}else if(Char.IsLowSurrogate(text[i]))throw new IOException("Unpaired diagnostic surrogate.");count++;}return count; }
 public static void RequireJsonScalars(string token) {
  bool high=false;
  for(int i=1;i<token.Length-1;i++) {
   char c=token[i];
   if(c=='\\') { c=token[++i];if(c=='u'){c=(char)Convert.ToInt32(token.Substring(i+1,4),16);i+=4;}else c=' '; }
   if(high) { if(!Char.IsLowSurrogate(c))throw new IOException("Unpaired diagnostic JSON surrogate.");high=false; }
   else if(Char.IsHighSurrogate(c))high=true;
   else if(Char.IsLowSurrogate(c))throw new IOException("Unpaired diagnostic JSON surrogate.");
  }
  if(high)throw new IOException("Unpaired diagnostic JSON surrogate.");
 }
 public void Dispose() {
  if(job!=null)job.Dispose();
  if(suspendedThread!=IntPtr.Zero){CloseHandle(suspendedThread);suspendedThread=IntPtr.Zero;}
  if(Input!=null)Input.Dispose();if(Output!=null)Output.Dispose();if(Error!=null)Error.Dispose();
  if(Process!=null)Process.Dispose();
 }
}
'@
}
function ConvertFrom-SunnyDoctorJson([byte[]]$Bytes,[Diagnostics.Stopwatch]$Watch=$null,[double]$Seconds=0) {
 Initialize-SunnyDoctorTypes
 if($Bytes.Length -gt 16777216){throw 'Diagnostic JSON exceeds 16 MiB.'}
 $text=[Text.UTF8Encoding]::new($false,$true).GetString($Bytes)
 $stack=[Collections.Generic.Stack[object]]::new()
 $strings=[regex]::new('"(?:[^"\\\x00-\x1f]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"',[Text.RegularExpressions.RegexOptions]::None,[TimeSpan]::FromSeconds(1))
 for($i=0;$i-lt$text.Length;$i++){
  if($null-ne$Watch-and($i%4096)-eq0-and$Watch.Elapsed.TotalSeconds-ge$Seconds){throw 'Diagnostic JSON deadline expired.'}
  $c=$text[$i]
  if($c-eq'"'){
   $m=$strings.Match($text,$i);if(-not$m.Success-or$m.Index-ne$i){throw 'Malformed diagnostic JSON string.'}
   # Windows ConvertFrom-Json may replace an escaped unpaired surrogate.
   # Reject it before that conversion can erase the original wire evidence.
   [SunnyDoctorProcess]::RequireJsonScalars($m.Value)
   $end=$i+$m.Length;$j=$end;while($j-lt$text.Length-and[char]::IsWhiteSpace($text[$j])){$j++}
   if($j-lt$text.Length-and$text[$j]-eq':'){
    if($stack.Count-eq0-or$null-eq$stack.Peek()){throw 'Malformed diagnostic JSON field.'}
    $key=(' {"key":'+$m.Value+'}'|ConvertFrom-Json).key
    if(-not$stack.Peek().Add($key)){throw 'Duplicate diagnostic JSON field.'}
   };$i=$end-1
  }elseif($c-eq'{'-or$c-eq'['){
   if($stack.Count-ge32){throw 'Diagnostic JSON exceeds depth 32.'}
   if($c-eq'{'){$stack.Push([Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase))}else{$stack.Push($null)}
  }elseif($c-eq'}'-or$c-eq']'){if($stack.Count-eq0){throw 'Malformed diagnostic JSON nesting.'};$null=$stack.Pop()}
 }
 $value=ConvertTo-SunnyDoctorMap ($text|ConvertFrom-Json)
 if($null-ne$Watch-and$Watch.Elapsed.TotalSeconds-ge$Seconds){throw 'Diagnostic JSON deadline expired.'};return $value
}
function ConvertTo-SunnyDoctorMap($Value) {
 if($Value-is[Management.Automation.PSCustomObject]){
  $map=[ordered]@{};foreach($property in $Value.PSObject.Properties){$map[$property.Name]=ConvertTo-SunnyDoctorMap $property.Value};return $map
 }
 if($Value-is[array]){$items=@(foreach($item in $Value){ConvertTo-SunnyDoctorMap $item});return ,$items}
 return $Value
}
function Test-SunnyDoctorEqual($Left,$Right) {
 if($null-eq$Left-or$null-eq$Right){return $null-eq$Left-and$null-eq$Right}
 if($Left-is[Collections.IDictionary]){
  if($Right-isnot[Collections.IDictionary]-or$Left.Count-ne$Right.Count){return $false}
  foreach($key in $Left.Keys){if(-not$Right.Contains($key)-or-not(Test-SunnyDoctorEqual $Left[$key] $Right[$key])){return $false}};return $true
 }
 if($Left-is[array]){if($Right-isnot[array]-or$Left.Count-ne$Right.Count){return $false};for($i=0;$i-lt$Left.Count;$i++){if(-not(Test-SunnyDoctorEqual $Left[$i] $Right[$i])){return $false}};return $true}
 if($Left.GetType()-ne$Right.GetType()){return $false};return $Left-ceq$Right
}
function Assert-SunnyDoctorPath([string]$Path) {
 $full=[IO.Path]::GetFullPath($Path);if($full.StartsWith('\\')){throw 'Doctor state requires a local Windows volume.'}
 $cursor=$full
 while($cursor){
  if(Test-Path -LiteralPath $cursor){$item=Get-Item -LiteralPath $cursor -Force;if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Doctor linked paths are unsupported.'}}
  $parent=[IO.Path]::GetDirectoryName($cursor.TrimEnd('\'));if($parent-eq$cursor){break};$cursor=$parent
 };return $full.TrimEnd('\')
}
function Read-SunnyDoctorFile([string]$Path,[int]$Limit) {
 Initialize-SunnyDoctorTypes;$physical=Assert-SunnyDoctorPath $Path
 $file=[IO.File]::Open($physical,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
 try{
  [SunnyDoctorProcess]::RequireSingleLink($file.SafeFileHandle)
  if($file.Length-gt$Limit){throw 'Doctor file exceeds its bound.'}
  $bytes=[byte[]]::new([int]$file.Length);$offset=0
  while($offset-lt$bytes.Length){$count=$file.Read($bytes,$offset,$bytes.Length-$offset);if($count-eq0){throw 'Doctor file ended during capture.'};$offset+=$count};return ,$bytes
 }finally{$file.Dispose()}
}
function Assert-SunnyDoctorFields($Value,[string[]]$Keys) {
 if($Value-isnot[Collections.IDictionary]){throw 'Diagnostic value must be an object.'}
 $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($key in $Value.Keys){$null=$seen.Add($key)}
 if(-not$seen.SetEquals($Keys)){throw 'Diagnostic object has missing or unknown fields.'}
}
function Assert-SunnyDoctorInteger($Value,[long]$Minimum,[long]$Maximum) {
 if(($Value-isnot[int]-and$Value-isnot[long])-or$Value-lt$Minimum-or$Value-gt$Maximum){throw 'Diagnostic integer type or bound is invalid.'}
}
function Assert-SunnyDoctorText($Value,[int]$Limit) {
 if($Value-isnot[string]-or$Value.Length-gt$Limit){throw 'Diagnostic text type or bound is invalid.'}
}
function Assert-SunnyDoctorTime($Value,[double]$Started) {
 if(($Value-isnot[int]-and$Value-isnot[long]-and$Value-isnot[double]-and$Value-isnot[decimal])-or
  [double]::IsNaN($Value)-or[double]::IsInfinity($Value)-or$Value-lt($Started-5)-or
  $Value-gt([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0+5)){throw 'Diagnostic evidence is stale or has an invalid clock.'}
}
function Write-SunnyDoctorState([string]$Path,$State,[switch]$Initial) {
 $bytes=[Text.Encoding]::UTF8.GetBytes(($State|ConvertTo-Json -Depth 8 -Compress))
 if($bytes.Length-gt8192){throw 'Doctor recovery state exceeds 8 KiB.'}
 $temp=$Path+'.writing-'+[Guid]::NewGuid().ToString('N');$created=$false
 try{
  $s=[IO.File]::Open($temp,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);$created=$true
  try{$s.Write($bytes,0,$bytes.Length);$s.Flush($true)}finally{$s.Dispose()}
  if($Initial){[IO.File]::Move($temp,$Path)}else{[IO.File]::Replace($temp,$Path,[NullString]::Value)}
 }finally{if($created-and(Test-Path -LiteralPath $temp)){[IO.File]::Delete($temp)}}
}
function Assert-SunnyDoctorScope($Scope,[string]$Directory,[string]$Context,[int]$ProcessId,[string]$Started) {
 Assert-SunnyDoctorFields $Scope @('schema_version','owner_sid','owner','context','daemon_id','parent_pid','parent_started')
 Assert-SunnyDoctorInteger $Scope.schema_version 1 1;Assert-SunnyDoctorInteger $Scope.parent_pid 1 2147483647
 if($Scope.owner_sid-isnot[string]-or$Scope.owner_sid-cne(Get-SunnyOwner)-or$Scope.owner-isnot[string]-or$Scope.owner-cnotmatch'^[0-9a-f]{32}$'-or
  [IO.Path]::GetFileName($Directory)-cne('Sunny-client-'+$Scope.owner)-or$Scope.context-isnot[string]-or$Scope.context-cne$Context-or
  $Scope.context-cnotmatch'^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$'-or$Scope.daemon_id-isnot[string]-or$Scope.daemon_id-cnotmatch'^[A-Za-z0-9:_-]{1,128}$'-or
  $Scope.parent_pid-ne$ProcessId-or$Scope.parent_started-isnot[string]-or$Scope.parent_started-cne$Started){throw 'Client ownership does not match the launched process.'}
}
function Assert-SunnyDoctorPayload($Doctor,[string]$Correlation,$Release,[double]$Started) {
 $keys=@('schema_version','request_id','success','read_only_ready','observed_at','expected_bridge','observed_bridge','session','native_state','capabilities','checks','unverified')
 if($Doctor-is[Collections.IDictionary]-and$Doctor.Contains('connection_failure')){$keys+='connection_failure';Assert-SunnyDoctorText $Doctor.connection_failure 512}
 Assert-SunnyDoctorFields $Doctor $keys;Assert-SunnyDoctorInteger $Doctor.schema_version 1 1
 Assert-SunnyDoctorTime $Doctor.observed_at $Started
 if($Doctor.request_id-isnot[string]-or$Doctor.request_id-cne$Correlation-or$Doctor.success-isnot[bool]-or
  $Doctor.read_only_ready-isnot[bool]-or$Doctor.read_only_ready-ne$Doctor.success){throw 'Doctor readiness or correlation is malformed.'}
 foreach($name in @('expected_bridge','observed_bridge')){
  $value=$Doctor[$name];if($null-eq$value-and$name-eq'observed_bridge'){continue}
  Assert-SunnyDoctorFields $value @('protocol_version','source_sha256');Assert-SunnyDoctorInteger $value.protocol_version 1 4294967295
  if($value.source_sha256-isnot[string]-or$value.source_sha256-cnotmatch'^[0-9a-f]{64}$'){throw 'Doctor bridge source is malformed.'}
 }
 if($Doctor.expected_bridge.source_sha256-cne$Release.bridge.source_sha256-or
  $Doctor.expected_bridge.protocol_version-ne$Release.bridge.contract.bridge_protocol_version){throw 'Doctor server does not match the selected release.'}
 $layers=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);$failed=$false
 if($Doctor.checks-isnot[array]-or$Doctor.checks.Count-lt1-or$Doctor.checks.Count-gt5){throw 'Doctor check count is invalid.'}
 foreach($check in $Doctor.checks){
  Assert-SunnyDoctorFields $check @('layer','status','code','message','next_step')
  foreach($value in $check.Values){Assert-SunnyDoctorText $value 512}
  if($check.status-cnotin@('pass','fail')-or-not$layers.Add($check.layer)){throw 'Doctor checks are contradictory.'}
  if($check.status-ceq'fail'){$failed=$true}
 }
 if($failed-eq$Doctor.success){throw 'Doctor checks contradict readiness.'}
 if($Doctor.unverified-isnot[array]-or$Doctor.unverified.Count-ne5){throw 'Doctor unverified boundaries are malformed.'}
 foreach($value in $Doctor.unverified){Assert-SunnyDoctorText $value 512}
 if($null-ne$Doctor.session){
  Assert-SunnyDoctorFields $Doctor.session @('schema_version','bridge_instance','document_token');Assert-SunnyDoctorInteger $Doctor.session.schema_version 1 1
  foreach($name in @('bridge_instance','document_token')){if($Doctor.session[$name]-isnot[string]-or$Doctor.session[$name]-cnotmatch'^[0-9a-f]{32}$'){throw 'Doctor session is malformed.'}}
 }
 if($null-ne$Doctor.native_state){Assert-SunnyDoctorFields $Doctor.native_state @('is_playing','session_record','record_mode');foreach($value in $Doctor.native_state.Values){if($value-isnot[bool]){throw 'Doctor native state must be boolean.'}}}
 if($null-ne$Doctor.capabilities){
  $cap=$Doctor.capabilities;Assert-SunnyDoctorFields $cap @('basis','reported','live_version','managed_authoring_version_eligible')
  if($cap.basis-isnot[string]-or$cap.basis-cne'version_floor_claims_not_host_qualification'-or$cap.managed_authoring_version_eligible-isnot[bool]){throw 'Doctor capabilities are malformed.'}
  Assert-SunnyDoctorFields $cap.live_version @('major','minor','bugfix');foreach($value in $cap.live_version.Values){Assert-SunnyDoctorInteger $value 0 65535}
  Assert-SunnyDoctorFields $cap.reported @('clip_add_new_notes','track_insert_device_native','automation_envelope_authoring','group_track_creation','arbitrary_browser_loading','structural_snapshot','max_for_live')
  foreach($value in $cap.reported.Values){if($value-isnot[string]-or$value-cnotin@('available','unavailable','unknown')){throw 'Doctor capability claim is malformed.'}}
 }
 if($Doctor.success-and($null-eq$Doctor.session-or$null-eq$Doctor.native_state-or$null-eq$Doctor.capabilities-or
  $null-eq$Doctor.observed_bridge-or$Doctor.observed_bridge.source_sha256-cne$Doctor.expected_bridge.source_sha256-or
  $Doctor.observed_bridge.protocol_version-ne$Doctor.expected_bridge.protocol_version-or
  -not$layers.SetEquals([string[]]@('bridge_connection','release_pairing','native_session','native_readiness')))){throw 'Doctor readiness lacks paired session evidence.'}
}
function Capture-SunnyDoctorScope($Io,$Owned,[string]$Context,[string]$StatePath,$State) {
 if($null-ne$Io.scope){return}
 $matches=[regex]::Matches($Io.stderr.ToString(),'(?m)^Sunny client recovery directory: ([^\r\n]+)\r?$')
 if($matches.Count-eq0){return};if($matches.Count-ne1){throw 'Client ownership discovery is ambiguous.'}
 $directory=Assert-SunnyPhysicalPath $matches[0].Groups[1].Value
 $file=Join-Path $directory 'session.json'
 $scope=ConvertFrom-SunnyDoctorJson (Read-SunnyDoctorFile $file 4096)
 Assert-SunnyDoctorScope $scope $directory $Context $Owned.Process.Id $Owned.Process.StartTime.ToUniversalTime().Ticks.ToString()
 $scope.directory=$directory;$State.client_scope=$scope
 Write-SunnyDoctorState $StatePath $State
 $Io.scope=$scope
}
function Assert-SunnyDoctorPolling([double]$Duration,[double]$Interval,[bool]$Messages) {
 if([double]::IsNaN($Duration)-or[double]::IsInfinity($Duration)-or$Duration-lt0-or$Duration-gt60-or
  [double]::IsNaN($Interval)-or[double]::IsInfinity($Interval)-or$Interval-lt0.05-or$Interval-gt5-or($Messages-and$Duration-eq0)){
  throw 'Polling needs finite duration [0,60], interval [0.05,5], and an explicit positive window for messages.'
 }
}
function ConvertFrom-SunnyDoctorLogResult($Result,[Diagnostics.Stopwatch]$Watch,[double]$Deadline) {
 $keys=@('isError','content');if($Result-is[Collections.IDictionary]-and$Result.Contains('structuredContent')){$keys+='structuredContent'}
 Assert-SunnyDoctorFields $Result $keys
 if($Result.isError-isnot[bool]-or$Result.content-isnot[array]-or$Result.content.Count-ne1){throw 'Malformed log tool result.'}
 Assert-SunnyDoctorFields $Result.content[0] @('type','text')
 if($Result.content[0].type-isnot[string]-or$Result.content[0].type-cne'text'-or$Result.content[0].text-isnot[string]){throw 'Malformed log content.'}
 $page=ConvertFrom-SunnyDoctorJson ([Text.UTF8Encoding]::new($false,$true).GetBytes($Result.content[0].text)) $Watch $Deadline
 if($page-isnot[Collections.IDictionary]-or-not$page.Contains('success')-or$page.success-isnot[bool]-or$Result.isError-eq$page.success){throw 'Contradictory log status.'}
 if($Result.Contains('structuredContent')-and-not(Test-SunnyDoctorEqual $Result.structuredContent $page)){throw 'Log structured/text content differs.'}
 if(-not$page.success){
  $fields=@('success','error');if($page.Contains('connection_failure')){$fields+='connection_failure';Assert-SunnyDoctorText $page.connection_failure 512}
  Assert-SunnyDoctorFields $page $fields;Assert-SunnyDoctorText $page.error 512
 }
 return $page
}
function Assert-SunnyDoctorScalarText($Value,[int]$Maximum) {
 if($Value-isnot[string]-or[SunnyDoctorProcess]::ScalarCount($Value)-gt$Maximum){throw 'Invalid bounded Unicode log text.'}
}
function Assert-SunnyDoctorLogPage($Page,[long]$Sequence,[string]$Stream,[double]$RequestedAt,
 [Diagnostics.Stopwatch]$Watch,[double]$Deadline) {
 Assert-SunnyDoctorFields $Page @('success','entries','next_sequence','latest_sequence','oldest_sequence','stream_id','reset','truncated','observed_at','has_more')
 if($Page.success-isnot[bool]-or-not$Page.success-or$Page.entries-isnot[array]-or$Page.entries.Count-gt1000-or
  $Page.stream_id-isnot[string]-or$Page.stream_id-cnotmatch'^[0-9a-f]{32}$'){throw 'Log page lacks current cursor evidence.'}
 foreach($key in @('reset','truncated','has_more')){if($Page[$key]-isnot[bool]){throw 'Log flag must be boolean.'}}
 foreach($key in @('next_sequence','latest_sequence')){Assert-SunnyDoctorInteger $Page[$key] 0 2147483647}
 Assert-SunnyDoctorInteger $Page.oldest_sequence 1 2147483648
 Assert-SunnyDoctorTime $Page.observed_at $RequestedAt
 $reset=($Stream-and$Stream-cne$Page.stream_id)-or$Sequence-gt$Page.latest_sequence
 [long]$after=if($reset){0}else{$Sequence}
 if($Page.reset-ne[bool]$reset-or$Page.oldest_sequence-gt([long]$Page.latest_sequence+1)-or
  $Page.truncated-ne($Page.oldest_sequence-gt($after+1))){throw 'Contradictory log reset or history gap.'}
 [long]$expected=[Math]::Max($after+1,$Page.oldest_sequence)
 foreach($entry in $Page.entries){
  if($Watch.Elapsed.TotalSeconds-ge$Deadline){throw 'Log page validation deadline expired.'}
  Assert-SunnyDoctorFields $entry @('sequence','time','level','source','message');Assert-SunnyDoctorInteger $entry.sequence 1 2147483647
  if($entry.sequence-ne$expected-or$expected-gt$Page.latest_sequence){throw 'Unordered or skipped log entry.'}
  $time=$entry.time
  if(($time-isnot[int]-and$time-isnot[long]-and$time-isnot[double]-and$time-isnot[decimal])-or
   [double]::IsNaN($time)-or[double]::IsInfinity($time)-or$time-lt0-or$time-gt([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0+5)){throw 'Invalid log record time.'}
  Assert-SunnyDoctorScalarText $entry.level 128;Assert-SunnyDoctorScalarText $entry.source 128;Assert-SunnyDoctorScalarText $entry.message 2000
  $expected++
 }
 [long]$delivered=if($Page.entries.Count){$expected-1}else{$after}
 if($Page.next_sequence-ne$delivered-or$Page.has_more-ne($delivered-lt$Page.latest_sequence)-or
  ($Page.entries.Count-eq0-and$Page.has_more)){throw 'Log cursor progress contradicts its page.'}
}
function Invoke-SunnyDoctorPolling($Call,$Pump,[Diagnostics.Stopwatch]$Watch,[double]$Deadline,
 [double]$Interval,[bool]$Messages) {
 $summary=[ordered]@{requested=$true;stale=$true;incomplete=$false;resets=0;gaps=0;requests=0;records_seen=0;omitted_records=0;records=@();cursor=$null;last_received_at=$null;last_observed_at=$null;include_messages=$Messages;project_content_possible=$Messages;failures=@()}
 $retained=[Collections.Generic.Queue[object]]::new();[long]$retainedBytes=0;[long]$sequence=0;$stream=$null;$delay=$Interval;$more=$false
 while($Watch.Elapsed.TotalSeconds-lt$Deadline-and$summary.requests-lt1000){
  $summary.requests++;$arguments=@{after_sequence=$sequence};if($stream){$arguments.stream_id=$stream}
  $requestedAt=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0
  try{
   $page=&$Call $arguments
   if(-not$page.success){throw 'Native log unavailable.'}
   Assert-SunnyDoctorLogPage $page $sequence $stream $requestedAt $Watch $Deadline
   $summary.stale=$false;$summary.last_received_at=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0;$summary.last_observed_at=$page.observed_at
   if($page.reset){$summary.resets++;$summary.incomplete=$true};if($page.truncated){$summary.gaps++;$summary.incomplete=$true}
   # Retain the fully validated cursor even if detail retention later fails.
   $sequence=$page.next_sequence;$stream=$page.stream_id;$summary.cursor=@{after_sequence=$sequence;stream_id=$stream};$more=$page.has_more
   $summary.records_seen+=$page.entries.Count
   foreach($entry in $page.entries){
    if($Watch.Elapsed.TotalSeconds-ge$Deadline){throw 'Log detail budget expired.'}
    $row=[ordered]@{stream_id=$page.stream_id;sequence=$entry.sequence;time=$entry.time;level=$entry.level;source=$entry.source}
    if($Messages){$row.message=$entry.message};$row=Protect-SunnyDoctorValues $row
    $summary.project_content_possible=$true
    $bytes=[Text.Encoding]::UTF8.GetByteCount(($row|ConvertTo-Json -Depth 4 -Compress))+1
    while($retained.Count-and($retained.Count-ge1000-or$bytes+$retainedBytes-gt393216)){$old=$retained.Dequeue();$retainedBytes-=$old.bytes}
    if($bytes-le393216){$retained.Enqueue(@{row=$row;bytes=$bytes});$retainedBytes+=$bytes}
   }
   if($more){$delay=$Interval;continue}
   if($page.entries.Count){$delay=$Interval}else{$delay=[Math]::Min(5.0,[double]$delay*2)}
   $wake=[Math]::Min($Deadline,$Watch.Elapsed.TotalSeconds+$delay)
   while($Watch.Elapsed.TotalSeconds-lt$wake){&$Pump -Waiting;Start-Sleep -Milliseconds 10}
  }catch{
   $summary.stale=$true;$summary.incomplete=$true
   $summary.failures+=@{layer='native_logs';status='fail';code='log_observation_unavailable';message='A current correlated closed log page was unavailable or invalid.';next_step='Inspect the retained cursor and scoped host status; repeat only read-only diagnosis.'}
   break
  }
 }
 $summary.records=@(foreach($item in $retained){$item.row});$summary.omitted_records=$summary.records_seen-$retained.Count
 if($more-or$summary.stale-or$summary.omitted_records-gt0-or$summary.requests-ge1000){$summary.incomplete=$true}
 return $summary
}
function Invoke-SunnyDoctorExchange([string]$Executable,[string[]]$Arguments,[string]$WorkingDirectory,$Release,
 [string]$Context,[int]$Seconds,[int]$CleanupBudget,[string]$StatePath,$State,
 [double]$Polling=0,[double]$Interval=0.25,[bool]$Messages=$false) {
 Assert-SunnyDoctorPolling $Polling $Interval $Messages
 Initialize-SunnyDoctorTypes
 $report=[ordered]@{schema_version=1;request_id=[Guid]::NewGuid().ToString('N');observed_at=0.0;success=$false;read_only_ready=$false;checks=@();doctor=$null;logs=$null;cleanup=$null;launcher='windows_verified_client';unknowns=@('Installed host/library, audio, licences and musical mutations require independent qualification.','A finite read-only observation cannot qualify subsequent musical effects or synchronous kernel/Live stalls.')}
 $started=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0
 $owned=$null;$watch=[Diagnostics.Stopwatch]::StartNew()
 $phase='client_launch';$jobAbsent=$false;$protocolComplete=$false
 $io=@{deadline=[double]$Seconds;buffer=[IO.MemoryStream]::new();stderr=[Text.StringBuilder]::new();decoder=[Text.UTF8Encoding]::new($false,$true).GetDecoder();chars=[char[]]::new(4096);outBytes=0;errBytes=0;scope=$null;out=[byte[]]::new(4096);err=[byte[]]::new(4096);outTask=$null;errTask=$null;next=0}
 try {
  $command=(@(@($Executable)+$Arguments|ForEach-Object{ConvertTo-SunnyWindowsArgument $_})-join' ')
  $owned=[SunnyDoctorProcess]::new($Executable,$command,$WorkingDirectory,$false)
  $State.child_pid=$owned.Process.Id;$State.child_started=$owned.Process.StartTime.ToUniversalTime().Ticks.ToString();Write-SunnyDoctorState $StatePath $State
  # The exact child identity is durable while no launcher code has run.
  $owned.Resume()
  $io.outTask=$owned.Output.ReadAsync($io.out,0,4096);$io.errTask=$owned.Error.ReadAsync($io.err,0,4096)
  $pump={param([switch]$Closing,[switch]$Waiting)
   if($Waiting-and$watch.Elapsed.TotalSeconds-ge$io.deadline){return}
   if(-not$Closing-and$watch.Elapsed.TotalSeconds-ge$io.deadline){throw 'Diagnostic deadline expired; native outcome remains unqualified.'}
   if($Closing-and$cleanupWatch.Elapsed.TotalSeconds-ge$CleanupBudget){throw 'Diagnostic cleanup deadline expired.'}
   foreach($channel in @('out','err')){
    $task=$io[$channel+'Task'];if($null-eq$task-or-not$task.IsCompleted){continue};$count=$task.GetAwaiter().GetResult()
    if($channel-eq'out'){
     $io.outBytes+=$count;if($io.outBytes-gt33554432-or$io.buffer.Length+$count-gt16777216){throw 'Diagnostic stdout exceeds its bound.'}
     $io.buffer.Position=$io.buffer.Length;$io.buffer.Write($io.out,0,$count)
    }else{
     $io.errBytes+=$count;if($io.errBytes-gt65536){throw 'Diagnostic stderr exceeds 64 KiB.'}
     # Decode incrementally without manufacturing replacement text in split UTF8.
     $characters=$io.decoder.GetChars($io.err,0,$count,$io.chars,0,($count-eq0))
     $null=$io.stderr.Append($io.chars,0,$characters)
     Capture-SunnyDoctorScope $io $owned $Context $StatePath $State
    }
    $stream=if($channel-eq'out'){$owned.Output}else{$owned.Error}
    $io[$channel+'Task']=if($count-gt0){$stream.ReadAsync($io[$channel],0,4096)}else{$null}
   }
  }
  $write={param($Value)
   $bytes=[Text.Encoding]::UTF8.GetBytes(($Value|ConvertTo-Json -Depth 8 -Compress)+"`n")
   $task=$owned.Input.WriteAsync($bytes,0,$bytes.Length)
   while(-not$task.IsCompleted){&$pump;Start-Sleep -Milliseconds 5};$null=$task.GetAwaiter().GetResult()
   $task=$owned.Input.FlushAsync();while(-not$task.IsCompleted){&$pump;Start-Sleep -Milliseconds 5};$null=$task.GetAwaiter().GetResult()
  }
  $request={param([string]$Method,$Parameters)
   $io.next++;$id=$io.next;&$write @{jsonrpc='2.0';id=$id;method=$Method;params=$Parameters}
   while($true){
    &$pump;$bytes=$io.buffer.ToArray();$end=[Array]::IndexOf($bytes,[byte]10)
    if($end-ge0){
     $line=[byte[]]::new($end);[Array]::Copy($bytes,0,$line,0,$end)
     $io.buffer.SetLength(0);if($bytes.Length-gt$end+1){$io.buffer.Write($bytes,$end+1,$bytes.Length-$end-1)}
     $value=ConvertFrom-SunnyDoctorJson $line $watch $io.deadline;Assert-SunnyDoctorFields $value @('jsonrpc','id','result');Assert-SunnyDoctorInteger $value.id $id $id
     if($value.jsonrpc-isnot[string]-or$value.jsonrpc-cne'2.0'-or$value.result-isnot[Collections.IDictionary]){throw 'Diagnostic stdio response is malformed.'}
     return $value.result
    }
    if($null-eq$io.outTask){throw 'Diagnostic stdout ended without a complete response.'}
    Start-Sleep -Milliseconds 5
   }
  }
  $report.checks+=@{layer='client_launch';status='pass';code='owned_process_spawned';message='Verified launcher resumed only after private job admission.';next_step=''}
  $phase='stdio'
  $init=&$request 'initialize' @{protocolVersion='2025-11-25';capabilities=@{};clientInfo=@{name='sunny-windows-doctor';version='1'}}
  Assert-SunnyDoctorFields $init @('protocolVersion','capabilities','serverInfo')
  if($init.capabilities-isnot[Collections.IDictionary]){throw 'Diagnostic MCP capabilities are malformed.'}
  Assert-SunnyDoctorFields $init.serverInfo @('name','version');foreach($value in $init.serverInfo.Values){Assert-SunnyDoctorText $value 128}
  if($init.protocolVersion-isnot[string]-or$init.protocolVersion-cne'2025-11-25'){throw 'Diagnostic MCP protocol negotiation mismatch.'}
  &$write @{jsonrpc='2.0';method='notifications/initialized'}
  $ping=&$request 'ping' @{};if($ping.Count-ne0){throw 'Diagnostic ping payload is invalid.'}
  $report.checks+=@{layer='stdio';status='pass';code='protocol_exchange_passed';message='Correlated initialize and ping passed.';next_step=''}
  $phase='mcp_tools';$inventory=&$request 'tools/list' @{};Assert-SunnyDoctorFields $inventory @('tools')
  if($inventory.tools-isnot[array]-or$inventory.tools.Count-gt4096){throw 'Diagnostic tool inventory is malformed.'}
  $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  foreach($tool in $inventory.tools){if($tool-isnot[Collections.IDictionary]-or-not$tool.Contains('name')-or$tool.name-isnot[string]-or$tool.name.Length-gt128-or-not$names.Add($tool.name)){throw 'Diagnostic tool inventory names are malformed.'}}
  if(-not$names.Contains('doctor_ableton')-or-not$names.Contains('get_ableton_remote_log')){throw 'Diagnostic tools are missing.'}
  $report.tool_count=$inventory.tools.Count;$report.checks+=@{layer='mcp_tools';status='pass';code='diagnostic_tools_available';message='Required read-only tools were advertised.';next_step=''}
  $phase='native_evidence';$result=&$request 'tools/call' @{name='doctor_ableton';arguments=@{request_id=$report.request_id}}
  $resultKeys=@('isError','content');if($result-is[Collections.IDictionary]-and$result.Contains('structuredContent')){$resultKeys+='structuredContent'}
  Assert-SunnyDoctorFields $result $resultKeys
  if($result-isnot[Collections.IDictionary]-or-not$result.Contains('isError')-or$result.isError-isnot[bool]-or
   -not$result.Contains('content')-or$result.content-isnot[array]-or$result.content.Count-ne1){throw 'Doctor tool content is malformed.'}
  Assert-SunnyDoctorFields $result.content[0] @('type','text')
  if($result.content[0].type-isnot[string]-or$result.content[0].type-cne'text'-or$result.content[0].text-isnot[string]){throw 'Doctor tool content is not text JSON.'}
  $doctor=ConvertFrom-SunnyDoctorJson ([Text.UTF8Encoding]::new($false,$true).GetBytes($result.content[0].text)) $watch $io.deadline
  Assert-SunnyDoctorPayload $doctor $report.request_id $Release $started
  if($result.isError-eq$doctor.success){throw 'Doctor tool status is contradictory.'}
  if($result.Contains('structuredContent')){
   $structured=$result.structuredContent
   Assert-SunnyDoctorPayload $structured $report.request_id $Release $started
   if(-not(Test-SunnyDoctorEqual $structured $doctor)){throw 'Doctor structured/text content differs.'}
  }
  if($null-eq$io.scope){Capture-SunnyDoctorScope $io $owned $Context $StatePath $State}
  if($null-eq$io.scope){throw 'Client owned cleanup scope was not captured.'}
  $report.doctor=$doctor;$report.success=$doctor.success;$report.read_only_ready=$doctor.read_only_ready;$protocolComplete=$true
  if($Polling-gt0){
   $phase='native_logs';$io.deadline=[Math]::Min([double]$Seconds,$watch.Elapsed.TotalSeconds+$Polling)
   $logCall={param($Cursor)
    $toolResult=&$request 'tools/call' @{name='get_ableton_remote_log';arguments=$Cursor}
    return ConvertFrom-SunnyDoctorLogResult $toolResult $watch $io.deadline
   }
   $report.logs=Invoke-SunnyDoctorPolling $logCall $pump $watch $io.deadline $Interval $Messages
   if($report.logs.stale-or$report.logs.incomplete){$report.success=$false;$report.read_only_ready=$false;$report.checks+=@{layer='native_logs';status='fail';code='log_poll_incomplete';message='The requested finite log observation has stale, missing, lost or omitted evidence.';next_step='Inspect the retained cursor and loss facts; repeat only read-only diagnosis.'}}
  }
 }catch{
  $report.success=$false;$report.read_only_ready=$false
  $report.checks+=@{layer=$phase;status='fail';code='diagnosis_incomplete';message='The '+$phase+' phase did not provide complete fresh typed evidence.';next_step='Inspect scoped client/host status; repeat only this read-only diagnosis.'}
 }finally{
  $cleanupWatch=[Diagnostics.Stopwatch]::StartNew();$clean=$false;$closureInvalid=$false
  try{
   if($null-ne$owned){
    $owned.Input.Dispose()
    while(-not$owned.Process.HasExited-and$cleanupWatch.Elapsed.TotalSeconds-lt[Math]::Min(2,$CleanupBudget/3)){
     try{&$pump -Closing}catch{$closureInvalid=$true;break};Start-Sleep -Milliseconds 10
    }
    if(-not$owned.Empty){$owned.Terminate()}
    while(-not$owned.Empty-and$cleanupWatch.Elapsed.TotalSeconds-lt[ Math ]::Min(4,$CleanupBudget/2)){Start-Sleep -Milliseconds 10}
    if(-not$owned.Empty){throw 'Owned diagnostic job absence is unconfirmed.'};$jobAbsent=$true
    # Continue bounded draining on normal close. A late error cannot bypass the
    # output bounds or discard an ownership receipt already captured durably.
    while(($null-ne$io.outTask-or$null-ne$io.errTask)-and$cleanupWatch.Elapsed.TotalSeconds-lt[Math]::Min(5,$CleanupBudget*0.7)){
     try{&$pump -Closing}catch{$closureInvalid=$true;break};Start-Sleep -Milliseconds 5
    }
    if($null-ne$io.outTask-or$null-ne$io.errTask){$closureInvalid=$true}
    if($protocolComplete){
     if(-not$owned.Process.HasExited-or$owned.Process.ExitCode-ne0-or$closureInvalid-or$io.buffer.Length-ne0){
      $report.success=$false;$report.read_only_ready=$false
      $report.checks+=@{layer='stdio_close';status='fail';code='protocol_close_incomplete';message='The owned launcher did not close with a zero exit and bounded protocol-only output.';next_step='Inspect the scoped launcher status; repeat only read-only diagnosis.'}
     }
    }
    if($null-eq$io.scope){throw 'Client container scope was not proved; retain the doctor recovery state.'}
    $scope=$io.scope;$remaining=$CleanupBudget-$cleanupWatch.Elapsed.TotalSeconds
    if($remaining-le0){throw 'Doctor cleanup budget expired.'}
    $remaining=[Math]::Floor($remaining);if($remaining-lt1){throw 'Doctor cleanup budget expired.'}
    Remove-SunnyClientContainers (Get-Command docker.exe -ErrorAction Stop).Source $scope.context $scope.daemon_id $scope.owner $remaining
    if(Test-Path -LiteralPath $scope.directory){Remove-SunnyClientSessionDirectory $scope.directory}
    $clean=$true
   }
  }catch{ }
  $report.cleanup=@{success=$clean;owned_job_absent=$jobAbsent;container_absence_confirmed=$clean;project_volume_removed=$false;recovery_directory=if($clean){$null}else{[IO.Path]::GetDirectoryName($StatePath)}}
  if(-not$clean){$report.success=$false;$report.read_only_ready=$false;$report.checks+=@{layer='cleanup';status='fail';code='owned_cleanup_unconfirmed';message='Owned process or original-daemon container absence was not fully proved.';next_step='Retain the recovery directory; use Recover with the same trusted release after the recorded process has exited.'}}
  $report.observed_at=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0
  try{$State.cleanup_confirmed=$clean;Write-SunnyDoctorState $StatePath $State}catch{
   $report.success=$false;$report.read_only_ready=$false;$report.cleanup.success=$false;$report.cleanup.recovery_directory=[IO.Path]::GetDirectoryName($StatePath)
   $report.checks+=@{layer='recovery_state';status='fail';code='recovery_checkpoint_failed';message='The final durable cleanup checkpoint could not be written.';next_step='Preserve the existing scoped receipt and verify cleanup before recovery.'}
  }finally{if($null-ne$owned){$owned.Dispose()};$io.buffer.Dispose()}
 }
 return $report
}
function Export-SunnyDoctorReport([string]$Path,$Report) {
 # Redact bounded values before encoding, preserving valid JSON and types.
 $safe=Protect-SunnyDoctorValues $Report
 $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($safe|ConvertTo-Json -Depth 16 -Compress)+"`n")
 if($bytes.Length-gt1048576){throw 'Doctor export exceeds 1 MiB; nothing was written.'}
 $null=Assert-SunnyDoctorPath $Path
 $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
 try{$stream.Write($bytes,0,$bytes.Length);$stream.Flush($true)}finally{$stream.Dispose()}
}
function Protect-SunnyDoctorValues($Value) {
 if($Value-is[Collections.IDictionary]){$safe=[ordered]@{};foreach($key in $Value.Keys){$safe[$key]=Protect-SunnyDoctorValues $Value[$key]};return $safe}
 if($Value-is[array]){$safe=@(foreach($item in $Value){Protect-SunnyDoctorValues $item});return ,$safe}
 if($Value-is[string]){
  $safe=$Value
  foreach($pattern in @('(?i)("(?:password|passwd|token|secret|api[_-]?key|authorization)"\s*:\s*)"(?:[^"\\]|\\.)*"','(?i)(?:https?|ssh)://[^\s/@:]+:[^\s/@]+@','(?i)\b(?:bearer|basic)\s+[^\s,;]+','(?i)\b(?:password|passwd|token|secret|api[_-]?key|authorization)\s*[:=]\s*(?:"[^"\r\n]*"|[^\s,;]+)','\b(?:sk-[A-Za-z0-9_-]{8,}|gh[pousr]_[A-Za-z0-9]{8,})\b')){
   $safe=[regex]::Replace($safe,$pattern,'[REDACTED]',[Text.RegularExpressions.RegexOptions]::None,[TimeSpan]::FromSeconds(1))
  };return $safe
 }
 return $Value
}
function Get-SunnyDoctorSources([string]$ArtifactRoot,[string]$Hash) {
 if($Hash-cnotmatch'^[0-9a-f]{64}$'){throw 'Supply the independently retained manifest checksum.'}
 $bytes=Read-SunnyDoctorFile (Join-Path $ArtifactRoot 'release.json') 1048576
 $digest=[Security.Cryptography.SHA256]::Create()
 try{$actual=[BitConverter]::ToString($digest.ComputeHash($bytes)).Replace('-','').ToLowerInvariant()}finally{$digest.Dispose()}
 if($actual-cne$Hash){throw 'Release manifest checksum mismatch.'}
 $manifest=ConvertFrom-SunnyDoctorJson $bytes
 if($manifest-isnot[Collections.IDictionary]-or-not$manifest.Contains('release_manifest_schema_version')){throw 'Unsupported release manifest.'}
 Assert-SunnyDoctorInteger $manifest.release_manifest_schema_version 1 1
 if($manifest.files-isnot[array]-or$manifest.files.Count-gt512){throw 'Release inventory is malformed.'}
 $sources=@{};$identities=@{}
 foreach($name in @('Sunny.ps1','SunnyRemote.ps1','SunnyClient.ps1','SunnyDoctor.ps1')){
  $entries=@($manifest.files|Where-Object{$_.path-is[string]-and$_.path-ceq('installer/windows/'+$name)})
  if($entries.Count-ne1){throw 'The release does not register this exact doctor/launcher.'}
  Assert-SunnyDoctorFields $entries[0] @('path','bytes','sha256');Assert-SunnyDoctorInteger $entries[0].bytes 0 131072
  if($entries[0].sha256-isnot[string]-or$entries[0].sha256-cnotmatch'^[0-9a-f]{64}$'){throw 'Operator source identity is malformed.'}
  $source=Read-SunnyDoctorFile (Join-Path $ArtifactRoot ('installer\windows\'+$name)) 131072
  $digest=[Security.Cryptography.SHA256]::Create();try{$actual=[BitConverter]::ToString($digest.ComputeHash($source)).Replace('-','').ToLowerInvariant()}finally{$digest.Dispose()}
  if($source.Length-ne$entries[0].bytes-or$actual-cne$entries[0].sha256){throw 'Operator source changed after release verification.'}
  $null=[Text.UTF8Encoding]::new($false,$true).GetString($source)
  $sources[$name]=$source;$identities[$name]=$actual
 }
 if(-not$script:SunnyDoctorScriptFile){throw 'Doctor must run from its exact verified release script.'}
 $current=Read-SunnyDoctorFile $script:SunnyDoctorScriptFile 131072
 $digest=[Security.Cryptography.SHA256]::Create();try{$actual=[BitConverter]::ToString($digest.ComputeHash($current)).Replace('-','').ToLowerInvariant()}finally{$digest.Dispose()}
 if($actual-cne$identities['SunnyDoctor.ps1']){throw 'The running doctor is not the selected release doctor.'}
 return @{manifest=$manifest;sources=$sources;identities=$identities}
}
function Remove-SunnyDoctorDirectory([string]$Directory,$Identities) {
 $null=Assert-SunnyDoctorPath $Directory
 $names=@('doctor.json')+@($Identities.Keys)
 foreach($item in Get-ChildItem -LiteralPath $Directory -Force){
  if($item.PSIsContainer-or$item.Name-cnotin$names){throw 'Unexpected doctor recovery content was preserved.'}
  $bytes=Read-SunnyDoctorFile $item.FullName 131072
  if($item.Name-cne'doctor.json'-and(Get-SunnyBytesHash $bytes)-cne$Identities[$item.Name]){throw 'Changed doctor recovery content was preserved.'}
 }
 foreach($name in $Identities.Keys){[IO.File]::Delete((Join-Path $Directory $name))}
 [IO.File]::Delete((Join-Path $Directory 'doctor.json'));[IO.Directory]::Delete($Directory)
}
function New-SunnyDoctorLauncherArguments([string]$Directory,$Identities,[string]$Route,[string]$ArtifactRoot,
 [string]$Hash,[string]$Image,[string]$Config,[string]$Volume,[string]$Context,[int]$CleanupBudget) {
 $sunnyDoctorInput=@{directory=$Directory;identities=$Identities;route=$Route;release=$ArtifactRoot;manifest=$Hash;image=$Image;config=$Config;volume=$Volume;context=$Context;cleanup=$CleanupBudget}
 $data=[Convert]::ToBase64String([Text.UTF8Encoding]::new($false).GetBytes(($sunnyDoctorInput|ConvertTo-Json -Depth 4 -Compress)))
 # Only ASCII base64 data is substituted into this fixed bootstrap. Hold every
 # verified helper open for read, denying write/delete throughout execution.
 $body=@'
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$held=[Collections.Generic.List[IO.FileStream]]::new()
try{
 $sunnyDoctorInput=[Text.UTF8Encoding]::new($false,$true).GetString([Convert]::FromBase64String('__SUNNY_DATA__'))|ConvertFrom-Json
 foreach($name in @('Sunny.ps1','SunnyRemote.ps1','SunnyClient.ps1')){
  $path=[IO.Path]::Combine($sunnyDoctorInput.directory,$name)
  $cursor=$path
  while($cursor){if(([IO.File]::GetAttributes($cursor)-band[IO.FileAttributes]::ReparsePoint)-ne0){throw 'Linked operator path.'};$parent=[IO.Path]::GetDirectoryName($cursor.TrimEnd('\'));if($parent-eq$cursor){break};$cursor=$parent}
  $file=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$held.Add($file)
  if($file.Length-gt131072){throw 'Operator source bound.'}
  $hash=[Security.Cryptography.SHA256]::Create();try{$actual=[BitConverter]::ToString($hash.ComputeHash($file)).Replace('-','').ToLowerInvariant()}finally{$hash.Dispose()}
  if($actual-cne$sunnyDoctorInput.identities.PSObject.Properties[$name].Value){throw 'Operator source identity.'}
 }
 & ([IO.Path]::Combine($sunnyDoctorInput.directory,'SunnyClient.ps1')) -Action Run -ConnectionFile $sunnyDoctorInput.route -ReleaseDirectory $sunnyDoctorInput.release -ExpectedManifestSHA256 $sunnyDoctorInput.manifest -ImageId $sunnyDoctorInput.image -ConfigurationFile $sunnyDoctorInput.config -WorkspaceVolume $sunnyDoctorInput.volume -DockerContext $sunnyDoctorInput.context -StartupSeconds 15 -CleanupSeconds $sunnyDoctorInput.cleanup
 if($LASTEXITCODE-isnot[int]){throw 'Launcher exit code is unproved.'};exit $LASTEXITCODE
}catch{[Console]::Error.WriteLine('Sunny doctor verified launcher failed before a complete protocol lifecycle.');exit 1}finally{foreach($file in $held){$file.Dispose()}}
'@
 $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($body.Replace('__SUNNY_DATA__',$data)))
 if($encoded.Length-gt28000){throw 'Doctor launcher data exceeds the Windows command-line bound.'}
 return @('-NoProfile','-NonInteractive','-EncodedCommand',$encoded)
}
function Invoke-SunnyDoctor([string]$Route,[string]$ArtifactRoot,[string]$Hash,[string]$Image,[string]$Config,
 [string]$Volume,[string]$Context,[int]$Seconds,[int]$CleanupBudget,[string]$Export,
 [double]$Polling=0,[double]$Interval=0.25,[bool]$Messages=$false) {
 Assert-SunnyDoctorPolling $Polling $Interval $Messages
 if($PSVersionTable.PSVersion-lt[Version]'5.1'-or-not[Environment]::Is64BitProcess-or[Environment]::OSVersion.Platform-ne[PlatformID]::Win32NT){throw '64-bit Windows PowerShell 5.1 is required.'}
 $preflight=[Diagnostics.Stopwatch]::StartNew()
 $captured=Get-SunnyDoctorSources $ArtifactRoot $Hash;$sources=$captured.sources;$manifest=$captured.manifest
 foreach($name in @('Sunny.ps1','SunnyRemote.ps1','SunnyClient.ps1')){. ([ScriptBlock]::Create([Text.UTF8Encoding]::new($false,$true).GetString($sources[$name])))}
 $null=Read-SunnyRelease $ArtifactRoot $Hash
 $null=Read-SunnyConnection $Route
 if($preflight.Elapsed.TotalSeconds-ge$Seconds){throw 'Doctor deadline expired during release/configuration preflight; no launcher was started.'}
 $owner=[Guid]::NewGuid().ToString('N');$directory=Join-Path ([IO.Path]::GetTempPath()) ('Sunny-doctor-'+$owner)
 $null=Assert-SunnyPhysicalPath $directory;$null=[IO.Directory]::CreateDirectory($directory)
 $statePath=Join-Path $directory 'doctor.json'
 $state=@{schema_version=1;owner_sid=(Get-SunnyOwner);owner=$owner;parent_pid=$PID;parent_started=(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks.ToString();child_pid=$null;child_started=$null;client_scope=$null;cleanup_confirmed=$false;manifest_sha256=$Hash;operator_sources=$captured.identities}
 Write-SunnyDoctorState $statePath $state -Initial
 foreach($name in $sources.Keys){$file=[IO.File]::Open((Join-Path $directory $name),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);try{$file.Write($sources[$name],0,$sources[$name].Length);$file.Flush($true)}finally{$file.Dispose()}}
 [Console]::Error.WriteLine('Sunny doctor recovery directory: '+$directory)
 $shell=Join-Path $PSHOME 'powershell.exe'
 $arguments=New-SunnyDoctorLauncherArguments $directory $captured.identities $Route $ArtifactRoot $Hash $Image $Config $Volume $Context $CleanupBudget
 $remaining=[int][Math]::Floor($Seconds-$preflight.Elapsed.TotalSeconds)
 if($remaining-lt1){throw 'Doctor deadline expired before launching; retain its recovery directory.'}
 $report=Invoke-SunnyDoctorExchange $shell $arguments $directory $manifest $Context $remaining $CleanupBudget $statePath $state $Polling $Interval $Messages
 $report.selected_release=@{manifest_sha256=$Hash;image_id=$Image}
 if($report.cleanup.success){
  try{Remove-SunnyDoctorDirectory $directory $captured.identities}catch{
   $report.success=$false;$report.read_only_ready=$false;$report.cleanup.success=$false;$report.cleanup.recovery_directory=$directory
   if(Test-Path -LiteralPath $statePath){$state.cleanup_confirmed=$false;Write-SunnyDoctorState $statePath $state}
  }
 }
 if($Export){Export-SunnyDoctorReport $Export $report}
 return $report
}
function Recover-SunnyDoctor([string]$Directory,[string]$ArtifactRoot,[string]$Hash,[int]$Seconds) {
 $budget=[Diagnostics.Stopwatch]::StartNew()
 $captured=Get-SunnyDoctorSources $ArtifactRoot $Hash
 foreach($name in @('Sunny.ps1','SunnyRemote.ps1','SunnyClient.ps1')){. ([ScriptBlock]::Create([Text.UTF8Encoding]::new($false,$true).GetString($captured.sources[$name])))}
 $null=Read-SunnyRelease $ArtifactRoot $Hash
 if($budget.Elapsed.TotalSeconds-ge$Seconds){throw 'Doctor recovery deadline expired during source validation; no resource was removed.'}
 $full=Assert-SunnyDoctorPath $Directory;$path=Join-Path $full 'doctor.json'
 $state=ConvertFrom-SunnyDoctorJson (Read-SunnyDoctorFile $path 8192)
 Assert-SunnyDoctorFields $state @('schema_version','owner_sid','owner','parent_pid','parent_started','child_pid','child_started','client_scope','cleanup_confirmed','manifest_sha256','operator_sources')
 Assert-SunnyDoctorInteger $state.schema_version 1 1;Assert-SunnyDoctorInteger $state.parent_pid 1 2147483647
 if($state.owner_sid-isnot[string]-or$state.owner_sid-cne(Get-SunnyOwner)-or$state.owner-isnot[string]-or$state.owner-cnotmatch'^[0-9a-f]{32}$'-or
  [IO.Path]::GetFileName($full)-cne('Sunny-doctor-'+$state.owner)-or$state.parent_started-isnot[string]-or$state.parent_started-cnotmatch'^[0-9]{1,19}$'-or
  $state.manifest_sha256-isnot[string]-or$state.manifest_sha256-cne$Hash-or$state.cleanup_confirmed-isnot[bool]-or
  -not(Test-SunnyDoctorEqual $state.operator_sources $captured.identities)){throw 'Doctor recovery ownership or release identity is unproved.'}
 $parent=Get-Process -Id $state.parent_pid -ErrorAction SilentlyContinue
 if($null-ne$parent-and$parent.StartTime.ToUniversalTime().Ticks.ToString()-ceq$state.parent_started){throw 'The recorded doctor is still running; recovery is declined.'}
 Assert-SunnyDoctorInteger $state.child_pid 1 2147483647
 if($state.child_started-isnot[string]-or$state.child_started-cnotmatch'^[0-9]{1,19}$'){throw 'Doctor child identity is unproved.'}
 $child=Get-Process -Id $state.child_pid -ErrorAction SilentlyContinue
 if($null-ne$child-and$child.StartTime.ToUniversalTime().Ticks.ToString()-ceq$state.child_started){throw 'The recorded client is still running; recovery is declined.'}
 $scope=$state.client_scope
 if($null-eq$scope){throw 'Client cleanup scope was not captured; preserve the doctor record and inspect the exact recorded child manually.'}
 Assert-SunnyDoctorFields $scope @('schema_version','owner_sid','owner','context','daemon_id','parent_pid','parent_started','directory')
 if($scope.directory-isnot[string]){throw 'Doctor client directory is unproved.'}
 $clientDirectory=Assert-SunnyDoctorPath $scope.directory
 $withoutDirectory=[ordered]@{};foreach($key in $scope.Keys){if($key-cne'directory'){$withoutDirectory[$key]=$scope[$key]}}
 Assert-SunnyDoctorScope $withoutDirectory $clientDirectory $scope.context $state.child_pid $state.child_started
 # Validate every private entry before touching the daemon or removing files.
 $names=@('doctor.json')+@($captured.identities.Keys)
 foreach($item in Get-ChildItem -LiteralPath $full -Force){
  if($item.PSIsContainer-or$item.Name-cnotin$names){throw 'Unexpected doctor recovery content was preserved.'}
  $bytes=Read-SunnyDoctorFile $item.FullName 131072
  if($item.Name-cne'doctor.json'-and(Get-SunnyBytesHash $bytes)-cne$captured.identities[$item.Name]){throw 'Changed doctor recovery content was preserved.'}
 }
 if(Test-Path -LiteralPath $clientDirectory){
  $journal=ConvertFrom-SunnyDoctorJson (Read-SunnyDoctorFile (Join-Path $clientDirectory 'session.json') 4096)
  if(-not(Test-SunnyDoctorEqual $journal $withoutDirectory)){throw 'Client recovery scope changed; no resource was removed.'}
 }
 $remaining=[int][Math]::Floor($Seconds-$budget.Elapsed.TotalSeconds)
 if($remaining-lt1){throw 'Doctor recovery deadline expired before original-daemon cleanup.'}
 Remove-SunnyClientContainers (Get-Command docker.exe -ErrorAction Stop).Source $scope.context $scope.daemon_id $scope.owner $remaining
 if($budget.Elapsed.TotalSeconds-ge$Seconds){throw 'Doctor recovery deadline expired; retain its cleanup receipt for readback.'}
 if(Test-Path -LiteralPath $clientDirectory){Remove-SunnyClientSessionDirectory $clientDirectory}
 $state.cleanup_confirmed=$true;Write-SunnyDoctorState $path $state
 if($budget.Elapsed.TotalSeconds-ge$Seconds){throw 'Doctor recovery deadline expired before directory removal; retain its durable receipt.'}
 Remove-SunnyDoctorDirectory $full $captured.identities
 return @{schema_version=1;operation='recover';owner=$state.owner;containers_absent=$true;recorded_processes_absent=$true;project_volume_removed=$false}
}
if($MyInvocation.InvocationName-ne'.'){
 try{
  if($Action-ceq'Recover'){$result=Recover-SunnyDoctor $SessionDirectory $ReleaseDirectory $ExpectedManifestSHA256 $CleanupSeconds}else{
   $result=Invoke-SunnyDoctor $ConnectionFile $ReleaseDirectory $ExpectedManifestSHA256 $ImageId $ConfigurationFile $WorkspaceVolume $DockerContext $TimeoutSeconds $CleanupSeconds $ExportPath $PollSeconds $PollInterval ([bool]$IncludeMessages)
  }
  $result|ConvertTo-Json -Depth 16
  if($Action-ceq'Recover'-or$result.success){exit 0}else{exit 1}
 }catch{[Console]::Error.WriteLine('Sunny doctor: '+$_.Exception.Message);exit 1}
}
