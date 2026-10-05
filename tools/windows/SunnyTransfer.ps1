[CmdletBinding()]
param(
    [ValidateSet('Plan','Status','Transfer','Recover','Clean')][string]$Action = 'Plan',
    [string]$ConnectionFile, [string]$ReleaseDirectory, [string]$ExpectedManifestSHA256,
    [string]$DestinationDirectory,
    [ValidateRange(30,3600)][int]$TimeoutSeconds = 600
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:SunnyTransferReadWatch=$null
$script:SunnyTransferReadSeconds=30

# This is a finite artifact transfer through already-approved access. It does not
# change SSH trust, keys, firewall policy, Live, installations or Docker state.
function Initialize-SunnyTransferTypes {
    if ('SunnyTransferFileInfo' -as [type]) { return }
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
public static class SunnyTransferFileInfo {
    public static bool EqualPrefix(byte[] left, byte[] right, int count) {
        for (int i=0; i<count; i++) if (left[i]!=right[i]) return false;
        return true;
    }
    [StructLayout(LayoutKind.Sequential)] struct Info {
        public uint attributes;
        public System.Runtime.InteropServices.ComTypes.FILETIME created, accessed, written;
        public uint volume, sizeHigh, sizeLow, links, indexHigh, indexLow;
    }
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetFileInformationByHandle(IntPtr h, out Info info);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern Microsoft.Win32.SafeHandles.SafeFileHandle CreateFile(string path, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
    [StructLayout(LayoutKind.Sequential)] struct Disposition { [MarshalAs(UnmanagedType.Bool)] public bool delete; }
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetFileInformationByHandle(IntPtr h, int kind, ref Disposition info, uint size);
    public static FileStream ForDelete(string path) {
        var handle=CreateFile(path, 0x80010000, 0, IntPtr.Zero, 3, 0x00200000, IntPtr.Zero);
        if (handle.IsInvalid) { var error=Marshal.GetLastWin32Error(); handle.Dispose(); throw new System.ComponentModel.Win32Exception(error); }
        try { return new FileStream(handle, FileAccess.Read); } catch { handle.Dispose(); throw; }
    }
    public static void DeleteOnClose(FileStream stream) {
        var info=new Disposition { delete=true };
        if (!SetFileInformationByHandle(stream.SafeFileHandle.DangerousGetHandle(), 4, ref info, 4))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    }
    public static void SingleLink(FileStream stream) {
        Info info;
        if (!GetFileInformationByHandle(stream.SafeFileHandle.DangerousGetHandle(), out info))
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        if (info.links != 1 || (info.attributes & 0x400) != 0)
            throw new IOException("Transfer refuses hardlinked or reparse files.");
    }
}
'@
}
function Get-SunnyTransferHash([byte[]]$Bytes) {
    Assert-SunnyTransferBudget
    $hash = [Security.Cryptography.SHA256]::Create()
    try {
        $value=[BitConverter]::ToString($hash.ComputeHash($Bytes)).Replace('-','').ToLowerInvariant()
        Assert-SunnyTransferBudget
        return $value
    }
    finally { $hash.Dispose() }
}
function Assert-SunnyTransferBudget {
    if ($null -ne $script:SunnyTransferReadWatch -and
        $script:SunnyTransferReadWatch.Elapsed.TotalSeconds -ge $script:SunnyTransferReadSeconds) {
        throw 'Receiver transfer deadline expired; durable state requires Recover.'
    }
}
function Read-SunnyTransferExact([IO.Stream]$Stream, [int]$Count) {
    if ($Count -lt 0 -or $Count -gt 2097152) { throw 'Transfer frame exceeds 2 MiB.' }
    $bytes = [byte[]]::new($Count); $offset = 0
    while ($offset -lt $Count) {
        if ($null -eq $script:SunnyTransferReadWatch) { $read = $Stream.Read($bytes, $offset, $Count-$offset) }
        else {
            $left=$script:SunnyTransferReadSeconds-$script:SunnyTransferReadWatch.Elapsed.TotalSeconds
            if ($left -le 0) { throw 'Receiver transfer deadline expired; durable state requires Recover.' }
            $task=$Stream.ReadAsync($bytes,$offset,$Count-$offset)
            if (-not $task.Wait([int][Math]::Ceiling($left*1000))) { throw 'Receiver transfer deadline expired; durable state requires Recover.' }
            $read=$task.GetAwaiter().GetResult()
        }
        if ($read -le 0) { throw 'Interrupted transfer; durable receiving state requires Recover.' }
        $offset += $read
    }
    return ,$bytes
}
function Read-SunnyTransferFrame([IO.Stream]$Stream) {
    $header = Read-SunnyTransferExact $Stream 4
    $size = [BitConverter]::ToUInt32($header,0)
    if ($size -gt 2097152) { throw 'Transfer frame exceeds 2 MiB.' }
    return ,(Read-SunnyTransferExact $Stream ([int]$size))
}
function ConvertFrom-SunnyTransferJson([byte[]]$Bytes) {
    if ($Bytes.Length -gt 2097152) { throw 'Transfer JSON exceeds 2 MiB.' }
    $text = [Text.UTF8Encoding]::new($false,$true).GetString($Bytes)
    # Scan all JSON strings before PS5.1's parser can discard duplicate keys.
    $stack = [Collections.Generic.Stack[object]]::new()
    $strings = [regex]::new('"(?:[^"\\\x00-\x1f]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"',
        [Text.RegularExpressions.RegexOptions]::None,[TimeSpan]::FromSeconds(1))
    for ($i=0; $i -lt $text.Length; $i++) {
        $c=$text[$i]
        if ($c -eq '"') {
            $match=$strings.Match($text,$i)
            if (-not $match.Success -or $match.Index -ne $i) { throw 'Malformed transfer JSON string.' }
            $end=$i+$match.Length; $j=$end
            while ($j -lt $text.Length -and [char]::IsWhiteSpace($text[$j])) { $j++ }
            if ($j -lt $text.Length -and $text[$j] -eq ':') {
                if ($stack.Count -eq 0 -or $null -eq $stack.Peek()) { throw 'Malformed transfer JSON field.' }
                $key=(' {"key":'+$match.Value+'}' | ConvertFrom-Json).key
                if (-not $stack.Peek().Add($key)) { throw 'Duplicate transfer JSON field.' }
            }
            $i=$end-1
        } elseif ($c -eq '{' -or $c -eq '[') {
            if ($stack.Count -ge 8) { throw 'Transfer JSON exceeds depth 8.' }
            if ($c -eq '{') { $stack.Push([Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)) }
            else { $stack.Push($null) }
        } elseif ($c -eq '}' -or $c -eq ']') {
            if ($stack.Count -eq 0) { throw 'Malformed transfer JSON nesting.' }
            $null=$stack.Pop()
        }
    }
    return ConvertTo-SunnyMap ($text | ConvertFrom-Json)
}
function Assert-SunnyTransferFields($Value,[string[]]$Keys) {
    if ($Value -isnot [Collections.IDictionary]) { throw 'Transfer value requires an object.' }
    $observed=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($key in $Value.Keys) { $null=$observed.Add($key) }
    if (-not $observed.SetEquals($Keys)) { throw 'Transfer has missing or unknown fields.' }
}
function Assert-SunnyTransferInteger($Value,[long]$Minimum,[long]$Maximum) {
    if (($Value -isnot [int] -and $Value -isnot [long]) -or $Value -lt $Minimum -or $Value -gt $Maximum) {
        throw 'Transfer requires a bounded JSON integer.'
    }
}
function Open-SunnyTransferFile([string]$Path,[IO.FileMode]$Mode,[IO.FileAccess]$Access) {
    Assert-SunnyTransferBudget
    $null=Assert-SunnyPhysicalPath $Path
    $stream=[IO.File]::Open($Path,$Mode,$Access,[IO.FileShare]::None)
    try { [SunnyTransferFileInfo]::SingleLink($stream);Assert-SunnyTransferBudget }
    catch { $stream.Dispose(); throw }
    return $stream
}
function Write-SunnyTransferJournal([string]$Path,$Value,[switch]$Initial) {
    Assert-SunnyTransferBudget
    $temporary=$Path+'.writing-'+[Guid]::NewGuid().ToString('N')
    $bytes=[Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 8 -Compress))
    $created=$false
    try {
        $stream=Open-SunnyTransferFile $temporary ([IO.FileMode]::CreateNew) ([IO.FileAccess]::Write)
        $created=$true
        try { $stream.Write($bytes,0,$bytes.Length); $stream.Flush($true);Assert-SunnyTransferBudget }
        finally { $stream.Dispose() }
        if ($Initial) { [IO.File]::Move($temporary,$Path) }
        else { $null=Assert-SunnyPhysicalPath $Path; [IO.File]::Replace($temporary,$Path,[NullString]::Value) }
        Assert-SunnyTransferBudget
    } finally {
        # This process exclusively created this exact temporary file. No glob cleanup.
        if ($created -and (Test-Path -LiteralPath $temporary)) { [IO.File]::Delete($temporary) }
    }
}
function Get-SunnyTransferPaths([string]$Destination) {
    if (-not $Destination -or $Destination -cnotmatch '^[A-Za-z]:[\\/]' -or $Destination -match '[\x00-\x1f]') {
        throw 'Transfer destination requires an explicit absolute local Windows path.'
    }
    $full=Assert-SunnyPhysicalPath $Destination
    $parent=[IO.Path]::GetDirectoryName($full)
    if (-not $parent -or -not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw 'Transfer destination parent must already exist.'
    }
    if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($full)).DriveFormat -cne 'NTFS') { throw 'Transfer currently requires a physical local NTFS volume.' }
    return @{ final=$full; stage=($full+'.sunny-stage'); journal=($full+'.sunny-transfer.json'); lock=($full+'.sunny-transfer.lock') }
}
function Assert-SunnyTransferFiles($Files) {
    if ($Files -isnot [array] -or $Files.Count -lt 2 -or $Files.Count -gt 512) { throw 'Transfer requires 2..512 files.' }
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    [long]$total=0
    foreach ($file in $Files) {
        Assert-SunnyTransferFields $file @('path','bytes','sha256','chunks')
        if ($file.path -isnot [string] -or $file.path.Length -gt 512 -or
            $file.path -cnotmatch '^(?:[A-Za-z0-9_-]+/)*[A-Za-z0-9_.-]+$' -or
            @($file.path.Split('/') | Where-Object { $_ -in @('.','..') -or $_ -match '\.$|^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)' }).Count -gt 0 -or
            -not $seen.Add($file.path) -or $file.sha256 -isnot [string] -or $file.sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Unsafe or colliding transfer file.' }
        Assert-SunnyTransferInteger $file.bytes 0 4294967296
        $total += $file.bytes
        if ($total -gt 4294967296) { throw 'Transfer exceeds 4 GiB.' }
        if ($file.chunks -isnot [array] -or $file.chunks.Count -ne [Math]::Ceiling($file.bytes/1048576.0)) { throw 'Transfer chunk inventory is incomplete.' }
        foreach ($chunk in $file.chunks) { if ($chunk -isnot [string] -or $chunk -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid transfer chunk checksum.' } }
    }
    if (-not $seen.Contains('release.json') -or -not $seen.Contains('image.tar')) { throw ('Transfer lacks the full release manifest or archive: '+($seen -join ',')) }
}
function Get-SunnyTransferPlanHash($Files,[string]$ManifestHash) {
    $body=$ManifestHash+"`n"
    foreach ($file in $Files) { $body += $file.path+"`n"+$file.bytes+"`n"+$file.sha256+"`n"+($file.chunks -join "`n")+"`n" }
    return Get-SunnyTransferHash ([Text.Encoding]::UTF8.GetBytes($body))
}
function Assert-SunnyTransferTree([string]$Root,$Files,[switch]$Partial) {
    Assert-SunnyTransferBudget
    $null=Assert-SunnyPhysicalPath $Root
    $expected=@{}; $directories=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($file in $Files) {
        $expected[$file.path]=$file
        $parent=$file.path
        while ($parent.Contains('/')) { $parent=$parent.Substring(0,$parent.LastIndexOf('/')); $null=$directories.Add($parent) }
    }
    $observed=0
    $enumeratedRoot=(Get-Item -LiteralPath $Root -Force).FullName
    $pending=[Collections.Generic.Stack[string]]::new();$pending.Push($enumeratedRoot)
    while ($pending.Count -gt 0) {
      foreach ($path in [IO.Directory]::EnumerateFileSystemEntries($pending.Pop())) {
        Assert-SunnyTransferBudget
        $item=Get-Item -LiteralPath $path -Force
        $null=Assert-SunnyPhysicalPath $item.FullName
        $name=$item.FullName.Substring($enumeratedRoot.Length+1).Replace('\','/')
        if ($item.PSIsContainer) {
            if (-not $directories.Contains($name)) { throw 'Foreign directory in transfer staging; preserve it.' }
            $pending.Push($item.FullName)
        } else {
            if (-not $expected.ContainsKey($name) -or $expected[$name].path -cne $name) { throw 'Foreign file in transfer staging; preserve it.' }
            $stream=Open-SunnyTransferFile $item.FullName ([IO.FileMode]::Open) ([IO.FileAccess]::Read)
            try {
                if ($stream.Length -gt $expected[$name].bytes) { throw 'Changed transfer file length; preserve it.' }
                if (-not $Partial -and ($stream.Length -ne $expected[$name].bytes -or
                    (Get-FileHash -InputStream $stream -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected[$name].sha256)) { throw 'Changed transfer file checksum; preserve it.' }
            } finally { $stream.Dispose() }
            Assert-SunnyTransferBudget
            $observed++
        }
      }
    }
    if (-not $Partial -and $observed -ne $Files.Count) { throw 'Complete transfer file inventory is missing.' }
    Assert-SunnyTransferBudget
}
function New-SunnyTransferPlan([string]$Root,[string]$Hash,[string]$Destination,[string]$Operation) {
    Initialize-SunnyTransferTypes
    $physical=Assert-SunnyPhysicalPath $Root
    $manifestPath=Join-Path $physical 'release.json'
    if ($Hash -cnotmatch '^[0-9a-f]{64}$' -or (Get-Item -LiteralPath $manifestPath).Length -gt 1048576 -or
        (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $Hash) { throw 'Bounded release manifest checksum mismatch.' }
    $manifest=ConvertFrom-SunnyTransferJson ([IO.File]::ReadAllBytes($manifestPath))
    $records=@($manifest.files)+@(@{path='release.json';bytes=(Get-Item -LiteralPath (Join-Path $physical 'release.json')).Length;sha256=$Hash})
    if ($records.Count -lt 2 -or $records.Count -gt 512) { throw 'Transfer requires 2..512 files.' }
    $draft=@()
    [long]$declared=0
    foreach ($record in $records) {
        Assert-SunnyTransferFields $record @('path','bytes','sha256')
        Assert-SunnyTransferInteger $record.bytes 0 4294967296
        $declared += $record.bytes
        if ($declared -gt 4294967296) { throw 'Transfer exceeds 4 GiB.' }
        $draft += @{path=$record.path;bytes=$record.bytes;sha256=$record.sha256;chunks=@(for ($c=0;$c -lt [Math]::Ceiling($record.bytes/1048576.0);$c++) { '0'*64 })}
    }
    Assert-SunnyTransferFiles $draft
    $null=Read-SunnyRelease $Root $Hash
    $files=@()
    foreach ($record in $records) {
        $path=Join-Path $physical $record.path.Replace('/','\')
        $stream=Open-SunnyTransferFile $path ([IO.FileMode]::Open) ([IO.FileAccess]::Read)
        try {
            $chunks=@(); [long]$remaining=$stream.Length
            while ($remaining -gt 0) {
                $chunk=Read-SunnyTransferExact $stream ([int][Math]::Min(1048576,$remaining))
                $chunks += Get-SunnyTransferHash $chunk; $remaining -= $chunk.Length
            }
            $files += @{path=$record.path;bytes=$record.bytes;sha256=$record.sha256;chunks=$chunks}
        } finally { $stream.Dispose() }
    }
    Assert-SunnyTransferFiles $files
    Assert-SunnyTransferTree $physical $files
    return @{transfer_schema_version=1;operation=$Operation;request_id=[Guid]::NewGuid().ToString('N');
        destination=([IO.Path]::GetFullPath($Destination).TrimEnd('\'));manifest_sha256=$Hash;files=$files;plan_sha256=(Get-SunnyTransferPlanHash $files $Hash);timeout_seconds=600}
}
function Send-SunnyTransferAck($Value) {
    Assert-SunnyTransferBudget
    [Console]::Out.WriteLine(($Value | ConvertTo-Json -Depth 5 -Compress)); [Console]::Out.Flush()
}
function Write-SunnyTransferChunk([IO.FileStream]$Stream,[byte[]]$Chunk,[int]$Offset,[int]$Count) {
    Assert-SunnyTransferBudget
    $Stream.Write($Chunk,$Offset,$Count); $Stream.Flush($true)
    Assert-SunnyTransferBudget
}
function Move-SunnyTransferRelease([string]$Stage,[string]$Final) {
    Assert-SunnyTransferBudget
    $null=Assert-SunnyPhysicalPath $Stage; $null=Assert-SunnyPhysicalPath $Final
    [IO.Directory]::Move($Stage,$Final)
    Assert-SunnyTransferBudget
}
function Read-SunnyTransferRelease([string]$Root,[string]$Hash) {
    Assert-SunnyTransferBudget
    $result=Read-SunnyRelease $Root $Hash
    Assert-SunnyTransferBudget
    return $result
}
function Remove-SunnyTransferProvenFile([string]$Path,$Proof) {
    Assert-SunnyTransferBudget
    $null=Assert-SunnyPhysicalPath $Path
    $stream=[SunnyTransferFileInfo]::ForDelete($Path)
    try {
        [SunnyTransferFileInfo]::SingleLink($stream)
        if ($stream.Length -ne $Proof.bytes -or
            (Get-FileHash -InputStream $stream -Algorithm SHA256).Hash.ToLowerInvariant() -cne $Proof.sha256) {
            throw 'Changed staging bytes after source proof; preserve them.'
        }
        Assert-SunnyTransferBudget
        [SunnyTransferFileInfo]::DeleteOnClose($stream)
    } finally { $stream.Dispose() }
    Assert-SunnyTransferBudget
}
function Invoke-SunnyTransferReceiver([IO.Stream]$InputStream,[Diagnostics.Stopwatch]$Started=$null) {
    Initialize-SunnyTransferTypes
    $script:SunnyTransferReadWatch=if ($null -eq $Started) { [Diagnostics.Stopwatch]::StartNew() } else { $Started }
    $script:SunnyTransferReadSeconds=30
    try {
    $request=ConvertFrom-SunnyTransferJson ([Convert]::FromBase64String([Text.Encoding]::ASCII.GetString((Read-SunnyTransferFrame $InputStream))))
    Assert-SunnyTransferFields $request @('transfer_schema_version','operation','request_id','destination','manifest_sha256','files','plan_sha256','timeout_seconds')
    Assert-SunnyTransferInteger $request.transfer_schema_version 1 1
    Assert-SunnyTransferInteger $request.timeout_seconds 30 3600
    $script:SunnyTransferReadSeconds=$request.timeout_seconds
    if ($request.operation -isnot [string] -or $request.operation -cnotin @('Status','Transfer','Recover','Clean') -or
        $request.request_id -isnot [string] -or $request.request_id -cnotmatch '^[0-9a-f]{32}$' -or
        $request.manifest_sha256 -isnot [string] -or $request.manifest_sha256 -cnotmatch '^[0-9a-f]{64}$' -or
        $request.plan_sha256 -isnot [string] -or $request.plan_sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Unsupported or uncorrelated transfer request.' }
    Assert-SunnyTransferFiles $request.files
    if ((Get-SunnyTransferPlanHash $request.files $request.manifest_sha256) -cne $request.plan_sha256) { throw 'Transfer plan checksum mismatch.' }
    $paths=Get-SunnyTransferPaths $request.destination
    Assert-SunnyTransferBudget
    } catch { $script:SunnyTransferReadWatch=$null;throw }
    $sid=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $state=$null; $lease=$null
    try {
        if (Test-Path -LiteralPath $paths.journal) {
            $journal=Open-SunnyTransferFile $paths.journal ([IO.FileMode]::Open) ([IO.FileAccess]::Read)
            try { $state=ConvertFrom-SunnyTransferJson (Read-SunnyTransferExact $journal ([int][Math]::Min($journal.Length,2097153))) }
            finally { $journal.Dispose() }
            Assert-SunnyTransferFields $state @('transfer_schema_version','owner_sid','owner_id','destination','manifest_sha256','plan_sha256','files','phase')
            Assert-SunnyTransferInteger $state.transfer_schema_version 1 1
            if ($state.owner_sid -isnot [string] -or $state.owner_sid -cne $sid -or
                $state.owner_id -isnot [string] -or $state.owner_id -cnotmatch '^[0-9a-f]{32}$' -or
                $state.destination -isnot [string] -or $state.destination -cne $paths.final -or
                $state.manifest_sha256 -isnot [string] -or $state.manifest_sha256 -cne $request.manifest_sha256 -or
                $state.plan_sha256 -isnot [string] -or $state.plan_sha256 -cne $request.plan_sha256 -or $state.phase -isnot [string] -or
                $state.phase -cnotin @('receiving','verified','committed','cleaning')) { throw 'Transfer journal ownership or source mismatch; preserve it.' }
            Assert-SunnyTransferFiles $state.files
            if ((Get-SunnyTransferPlanHash $state.files $state.manifest_sha256) -cne $state.plan_sha256) { throw 'Transfer journal inventory changed.' }
        } else {
            foreach ($path in @($paths.final,$paths.stage,$paths.lock)) {
                if (Test-Path -LiteralPath $path) { throw 'Existing destination/staging is unowned; preserve it.' }
            }
            if ($request.operation -in @('Status','Clean')) {
                Send-SunnyTransferAck @{transfer_schema_version=1;request_id=$request.request_id;status='absent';payload_required=$false;destination=$paths.final}
                return
            }
            $state=@{transfer_schema_version=1;owner_sid=$sid;owner_id=[Guid]::NewGuid().ToString('N');destination=$paths.final;
                manifest_sha256=$request.manifest_sha256;plan_sha256=$request.plan_sha256;files=$request.files;phase='receiving'}
            Write-SunnyTransferJournal $paths.journal $state -Initial
        }
        if ($request.operation -eq 'Status') {
            if (Test-Path -LiteralPath $paths.final) {
                if ($state.phase -notin @('verified','committed') -or (Test-Path -LiteralPath $paths.stage)) { throw 'Final destination appeared outside the journal; preserve it.' }
                Assert-SunnyTransferTree $paths.final $state.files
                $null=Read-SunnyTransferRelease $paths.final $state.manifest_sha256
            } elseif (Test-Path -LiteralPath $paths.stage) { Assert-SunnyTransferTree $paths.stage $state.files -Partial }
            Send-SunnyTransferAck @{transfer_schema_version=1;request_id=$request.request_id;status=$state.phase;payload_required=$false;destination=$paths.final;manifest_sha256=$state.manifest_sha256}
            return
        }
        if (Test-Path -LiteralPath $paths.lock) {
            $lease=Open-SunnyTransferFile $paths.lock ([IO.FileMode]::Open) ([IO.FileAccess]::ReadWrite)
            if ($lease.Length -gt 0) {
                if ($lease.Length -ne 32 -or [Text.Encoding]::ASCII.GetString((Read-SunnyTransferExact $lease 32)) -cne $state.owner_id) { throw 'Transfer lease file is foreign; preserve it.' }
            }
        } else { $lease=Open-SunnyTransferFile $paths.lock ([IO.FileMode]::CreateNew) ([IO.FileAccess]::ReadWrite) }
        if ($lease.Length -eq 0) {
            $ownerBytes=[Text.Encoding]::ASCII.GetBytes($state.owner_id); $lease.Write($ownerBytes,0,32); $lease.Flush($true)
            Assert-SunnyTransferBudget
        }
        if (Test-Path -LiteralPath $paths.final) {
            if ($state.phase -notin @('verified','committed') -or (Test-Path -LiteralPath $paths.stage)) { throw 'Final destination appeared outside the recorded move; preserve it.' }
            Assert-SunnyTransferTree $paths.final $state.files
            $null=Read-SunnyTransferRelease $paths.final $state.manifest_sha256
            if ($state.phase -eq 'verified') { $state.phase='committed'; Write-SunnyTransferJournal $paths.journal $state }
            Send-SunnyTransferAck @{transfer_schema_version=1;request_id=$request.request_id;status='committed';payload_required=$false;destination=$paths.final;manifest_sha256=$state.manifest_sha256}
            return
        }
        if ($state.phase -eq 'committed') { throw 'Committed release is missing; preserve the journal.' }
        if (Test-Path -LiteralPath $paths.stage) { Assert-SunnyTransferTree $paths.stage $state.files -Partial }
        if ($request.operation -eq 'Status') {
            Send-SunnyTransferAck @{transfer_schema_version=1;request_id=$request.request_id;status=$state.phase;payload_required=$false;destination=$paths.final;manifest_sha256=$state.manifest_sha256}
            return
        }
        if ($state.phase -eq 'cleaning' -and $request.operation -ne 'Clean') { throw 'Finish the recorded Clean operation before transfer.' }
        if (-not (Test-Path -LiteralPath $paths.stage)) { [IO.Directory]::CreateDirectory($paths.stage) | Out-Null }
        Send-SunnyTransferAck @{transfer_schema_version=1;request_id=$request.request_id;status='ready';payload_required=$true;destination=$paths.final}
        $proofs=@{}
        foreach ($file in $state.files) {
            Assert-SunnyTransferBudget
            $path=Join-Path $paths.stage $file.path.Replace('/','\')
            $exists=Test-Path -LiteralPath $path
            $output=$null
            try {
                if ($exists) { $output=Open-SunnyTransferFile $path ([IO.FileMode]::Open) ([IO.FileAccess]::ReadWrite) }
                elseif ($request.operation -ne 'Clean') {
                    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path)) | Out-Null
                    $output=Open-SunnyTransferFile $path ([IO.FileMode]::CreateNew) ([IO.FileAccess]::ReadWrite)
                }
                [long]$offset=0; $index=0
                foreach ($expected in $file.chunks) {
                    $size=[int][Math]::Min(1048576,($file.bytes-$offset))
                    $chunk=Read-SunnyTransferExact $InputStream $size
                    if ((Get-SunnyTransferHash $chunk) -cne $expected) { throw 'Incoming chunk checksum mismatch; receiving state retained.' }
                    if ($null -ne $output) {
                        $present=[int][Math]::Min($size,[Math]::Max(0,($output.Length-$offset)))
                        if ($present -gt 0) {
                            $old=Read-SunnyTransferExact $output $present
                            if (-not [SunnyTransferFileInfo]::EqualPrefix($old,$chunk,$present)) { throw 'Changed staging prefix; preserve it before recovery.' }
                        }
                        if ($present -lt $size -and $request.operation -ne 'Clean') {
                            Write-SunnyTransferChunk $output $chunk $present ($size-$present)
                        }
                    }
                    $offset += $size; $index++
                }
                if ($null -ne $output -and $request.operation -eq 'Clean') {
                    $output.Position=0
                    $proofs[$file.path]=@{bytes=$output.Length;sha256=(Get-FileHash -InputStream $output -Algorithm SHA256).Hash.ToLowerInvariant()}
                    Assert-SunnyTransferBudget
                }
            } finally { if ($null -ne $output) { $output.Dispose() } }
        }
        $end=[byte[]]::new(1)
        $left=$script:SunnyTransferReadSeconds-$script:SunnyTransferReadWatch.Elapsed.TotalSeconds
        if ($left -le 0) { throw 'Receiver transfer deadline expired; durable state requires Recover.' }
        $eof=$InputStream.ReadAsync($end,0,1)
        if (-not $eof.Wait([int][Math]::Ceiling($left*1000))) { throw 'Receiver transfer deadline expired; durable state requires Recover.' }
        if ($eof.GetAwaiter().GetResult() -ne 0) { throw 'Transfer has unexpected trailing payload.' }
        if ($request.operation -eq 'Clean') {
            # Source-authenticated prefix proof completed for every extant known file.
            $state.phase='cleaning'; Write-SunnyTransferJournal $paths.journal $state
            foreach ($file in $state.files) {
                $path=Join-Path $paths.stage $file.path.Replace('/','\')
                if (Test-Path -LiteralPath $path) {
                    if (-not $proofs.ContainsKey($file.path)) { throw 'New staging file after source proof; preserve it.' }
                    Remove-SunnyTransferProvenFile $path $proofs[$file.path]
                }
            }
            $folders=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            foreach ($file in $state.files) {
                $folder=[IO.Path]::GetDirectoryName((Join-Path $paths.stage $file.path.Replace('/','\')))
                while ($folder.Length -gt $paths.stage.Length) { $null=$folders.Add($folder);$folder=[IO.Path]::GetDirectoryName($folder) }
            }
            foreach ($folder in @($folders | Sort-Object Length -Descending)) {
                Assert-SunnyTransferBudget
                if (Test-Path -LiteralPath $folder) { $null=Assert-SunnyPhysicalPath $folder;[IO.Directory]::Delete($folder) }
                Assert-SunnyTransferBudget
            }
            $null=Assert-SunnyPhysicalPath $paths.stage
            [IO.Directory]::Delete($paths.stage)
            Assert-SunnyTransferBudget
            Send-SunnyTransferAck @{transfer_schema_version=1;request_id=$request.request_id;status='cleaned';payload_required=$false;destination=$paths.final}
            return
        }
        Assert-SunnyTransferTree $paths.stage $state.files
        $null=Read-SunnyTransferRelease $paths.stage $state.manifest_sha256
        $state.phase='verified'; Write-SunnyTransferJournal $paths.journal $state
        Move-SunnyTransferRelease $paths.stage $paths.final
        $state.phase='committed'; Write-SunnyTransferJournal $paths.journal $state
        Assert-SunnyTransferTree $paths.final $state.files
        Send-SunnyTransferAck @{transfer_schema_version=1;request_id=$request.request_id;status='committed';payload_required=$false;destination=$paths.final;manifest_sha256=$state.manifest_sha256}
    } finally {
        $script:SunnyTransferReadWatch=$null
        if ($null -ne $lease) { $lease.Dispose() }
        if ($null -ne $state -and $request.operation -eq 'Clean' -and $state.phase -eq 'cleaning' -and -not (Test-Path -LiteralPath $paths.stage)) {
            [IO.File]::Delete($paths.lock); [IO.File]::Delete($paths.journal)
        }
    }
}
function Get-SunnyTransferStarter {
    $code=@'
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
try {
 $started=[Diagnostics.Stopwatch]::StartNew()
 $s=[Console]::OpenStandardInput()
 function ReadSunnyBootstrap([int]$n) {
  $b=[byte[]]::new($n);$p=0
  while($p-lt$n){
   $left=30-$started.Elapsed.TotalSeconds;if($left-le0){throw 'Bootstrap deadline expired'}
   $t=$s.ReadAsync($b,$p,$n-$p);if(-not$t.Wait([int][Math]::Ceiling($left*1000))){throw 'Bootstrap deadline expired'}
   $c=$t.GetAwaiter().GetResult();if($c-le0){throw 'Incomplete transfer bootstrap'};$p+=$c
  };return ,$b
 }
 $n=[BitConverter]::ToUInt32((ReadSunnyBootstrap 4),0);if($n-gt524288){throw 'Transfer bootstrap exceeds 512 KiB'}
 $v=[Text.Encoding]::UTF8.GetString((ReadSunnyBootstrap ([int]$n))) | ConvertFrom-Json
 . ([ScriptBlock]::Create([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($v.installer))))
 . ([ScriptBlock]::Create([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($v.transfer))))
 [Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
 Invoke-SunnyTransferReceiver $s $started
 exit 0
} catch { [Console]::Error.WriteLine('Sunny transfer receiver: '+$_.Exception.Message);exit 1 }
'@
    return [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($code))
}
function Invoke-SunnyTransferExchange([string]$Executable,[string[]]$Arguments,[string]$Root,$Plan,
    [string]$InstallerSource,[string]$TransferSource,[int]$Seconds) {
    if ($Seconds -lt 30 -or $Seconds -gt 3600) { throw 'Transfer timeout must be 30..3600 seconds.' }
    $Plan.timeout_seconds=$Seconds
    $start=[Diagnostics.ProcessStartInfo]::new()
    $start.FileName=$Executable
    $start.Arguments=(@($Arguments | ForEach-Object { ConvertTo-SunnyWindowsArgument $_ }) -join ' ')
    $start.UseShellExecute=$false; $start.RedirectStandardInput=$true
    $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
    $start.StandardOutputEncoding=[Text.UTF8Encoding]::new($false,$true)
    $start.StandardErrorEncoding=[Text.UTF8Encoding]::new($false)
    $process=Start-SunnyUtf8PipeProcess $start
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $io=@{out=[Text.StringBuilder]::new();err=[Text.StringBuilder]::new();pending='';received=0;
        outBuffer=[char[]]::new(4096);errBuffer=[char[]]::new(4096);outTask=$null;errTask=$null}
    $io.outTask=$process.StandardOutput.ReadAsync($io.outBuffer,0,1)
    $io.errTask=$process.StandardError.ReadAsync($io.errBuffer,0,1)
    $pump={
        if ($watch.Elapsed.TotalSeconds -ge $Seconds) { throw 'Transfer deadline expired; inspect Status and Recover the durable journal.' }
        foreach ($channel in @('out','err')) {
          for ($drained=0;$drained -lt 8192;$drained++) {
            $task=$io[$channel+'Task']
            if ($null -eq $task -or -not $task.IsCompleted) { break }
            $count=$task.GetAwaiter().GetResult()
            if ($channel -eq 'out') {
                $io.received += $count
                if ($io.received -gt 65536) { throw 'Transfer acknowledgment exceeds 64 KiB.' }
                if ($count -gt 0) { $io.pending += [string]::new($io.outBuffer,0,$count) }
            } else {
                if ($io.err.Length+$count -gt 65536) { throw 'Transfer stderr exceeds 64 KiB.' }
                $null=$io.err.Append($io.errBuffer,0,$count)
            }
            if ($channel -eq 'out') { $io.outTask=if ($count -gt 0) { $process.StandardOutput.ReadAsync($io.outBuffer,0,1) } else { $null } }
            else { $io.errTask=if ($count -gt 0) { $process.StandardError.ReadAsync($io.errBuffer,0,1) } else { $null } }
          }
        }
    }
    $write={ param([byte[]]$Bytes)
        $task=$process.StandardInput.BaseStream.WriteAsync($Bytes,0,$Bytes.Length)
        while (-not $task.IsCompleted) { & $pump; Start-Sleep -Milliseconds 5 }
        $null=$task.GetAwaiter().GetResult(); & $pump
    }
    $frame={ param([byte[]]$Bytes)
        if ($Bytes.Length -gt 2097152) { throw 'Transfer frame exceeds 2 MiB.' }
        & $write ([BitConverter]::GetBytes([uint32]$Bytes.Length)); & $write $Bytes
    }
    $flush={
        $task=$process.StandardInput.BaseStream.FlushAsync()
        while (-not $task.IsCompleted) { & $pump; Start-Sleep -Milliseconds 5 }
        $null=$task.GetAwaiter().GetResult()
    }
    $ack={
        while (-not $io.pending.Contains("`n")) {
            & $pump
            if ($null -eq $io.outTask -and $process.HasExited) { throw 'Transfer ended without a correlated acknowledgment; inspect Status.' }
            Start-Sleep -Milliseconds 5
        }
        $end=$io.pending.IndexOf("`n"); $line=$io.pending.Substring(0,$end).TrimEnd("`r")
        $io.pending=$io.pending.Substring($end+1)
        $value=ConvertFrom-SunnyTransferJson ([Text.Encoding]::UTF8.GetBytes($line))
        if ($value -isnot [Collections.IDictionary] -or -not $value.Contains('status') -or $value.status -isnot [string]) { throw 'Transfer acknowledgment must be a typed object.' }
        $keys=@('transfer_schema_version','request_id','status','payload_required','destination')
        if ($value.status -cin @('committed','receiving','verified','cleaning')) { $keys += 'manifest_sha256' }
        Assert-SunnyTransferFields $value $keys
        Assert-SunnyTransferInteger $value.transfer_schema_version 1 1
        if ($value.status -cnotin @('ready','absent','committed','cleaned','receiving','verified','cleaning') -or
            $value.request_id -isnot [string] -or $value.request_id -cne $Plan.request_id -or
            $value.payload_required -isnot [bool] -or $value.destination -isnot [string] -or $value.destination -cne $Plan.destination) { throw 'Transfer acknowledgment is malformed or uncorrelated.' }
        if ($value.status -cin @('committed','receiving','verified','cleaning') -and
            ($value.manifest_sha256 -isnot [string] -or $value.manifest_sha256 -cne $Plan.manifest_sha256)) { throw 'Transfer acknowledgment source mismatch.' }
        return $value
    }
    try {
        $bootstrap=@{installer=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($InstallerSource));
            transfer=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($TransferSource))}
        $bootstrapBytes=[Text.Encoding]::UTF8.GetBytes(($bootstrap | ConvertTo-Json -Compress))
        if ($bootstrapBytes.Length -gt 524288) { throw 'Transfer bootstrap exceeds 512 KiB.' }
        & $frame $bootstrapBytes
        $metadata=[Text.Encoding]::UTF8.GetBytes(($Plan | ConvertTo-Json -Depth 8 -Compress))
        & $frame ([Text.Encoding]::ASCII.GetBytes([Convert]::ToBase64String($metadata)))
        & $flush
        $ready=& $ack
        $allowed=if ($Plan.operation -ceq 'Status') { @('absent','receiving','verified','cleaning','committed') }
            elseif ($Plan.operation -ceq 'Clean') { @('absent','committed','ready') } else { @('committed','ready') }
        if ($ready.status -cnotin $allowed -or ($Plan.operation -ceq 'Status' -and $ready.payload_required)) {
            throw 'Transfer acknowledgment does not match the requested operation.'
        }
        if ($ready.payload_required) {
            if ($ready.status -cne 'ready') { throw 'Transfer did not explicitly accept payload.' }
            foreach ($file in $Plan.files) {
                $path=Join-Path $Root $file.path.Replace('/','\')
                $stream=Open-SunnyTransferFile $path ([IO.FileMode]::Open) ([IO.FileAccess]::Read)
                try {
                    if ($stream.Length -ne $file.bytes) { throw 'Source length changed after preflight.' }
                    [long]$offset=0; $index=0
                    while ($offset -lt $file.bytes) {
                        $chunk=Read-SunnyTransferExact $stream ([int][Math]::Min(1048576,($file.bytes-$offset)))
                        if ((Get-SunnyTransferHash $chunk) -cne $file.chunks[$index]) { throw 'Source bytes changed after preflight.' }
                        & $write $chunk; $offset += $chunk.Length; $index++
                    }
                } finally { $stream.Dispose() }
            }
            & $flush
            $process.StandardInput.Close()
            $result=& $ack
            $expectedStatus=if ($Plan.operation -ceq 'Clean') { 'cleaned' } else { 'committed' }
            if ($result.status -cne $expectedStatus -or $result.payload_required) { throw 'Transfer did not confirm its final state.' }
        } else {
            $result=$ready; $process.StandardInput.Close()
        }
        while (-not $process.HasExited -or $null -ne $io.outTask -or $null -ne $io.errTask) { & $pump; Start-Sleep -Milliseconds 5 }
        if ($process.ExitCode -ne 0 -or $io.pending.Trim()) { throw 'Transfer exit/readback is incomplete; inspect Status before repeating.' }
        if ($result.status -ceq 'committed' -and $result.manifest_sha256 -cne $Plan.manifest_sha256) { throw 'Transfer committed the wrong source manifest.' }
        return $result
    } catch {
        $original=$_
        $drain=[Diagnostics.Stopwatch]::StartNew()
        while ($drain.ElapsedMilliseconds -lt 100 -and $null -ne $io.errTask) {
            try { & $pump } catch { }
            Start-Sleep -Milliseconds 5
        }
        if ($io.err.ToString() -match 'REMOTE HOST IDENTIFICATION HAS CHANGED|Host key verification failed') { throw 'SSH host identity rejected; no trust change was made.' }
        if ($io.err.ToString() -match '(?m)^\S.*: Permission denied \([a-z,-]+\)\.?\s*$') { throw 'SSH authentication denied; existing approved access is required.' }
        if ($io.err.Length -gt 0) { throw ('Transfer failed ('+$original.Exception.Message+'); inspect Status: '+$io.err.ToString().Substring(0,[Math]::Min(8192,$io.err.Length))) }
        throw $original
    } finally {
        if (-not $process.HasExited) {
            $process.Kill()
            if (-not $process.WaitForExit(1000)) { throw ('Owned transfer SSH cleanup unconfirmed for PID '+$process.Id) }
        }
        $process.Dispose()
    }
}
function Invoke-SunnyTransfer([string]$Operation,[string]$ConnectionPath,[string]$Root,[string]$Hash,[string]$Destination,[int]$Seconds) {
    . (Join-Path $PSScriptRoot 'Sunny.ps1')
    . (Join-Path $PSScriptRoot 'SunnyRemote.ps1')
    if ($Operation -notin @('Plan','Status','Transfer','Recover','Clean')) { throw 'Unsupported transfer operation.' }
    $connection=Read-SunnyConnection $ConnectionPath
    # Destination is remote data. Its filesystem preconditions are checked there,
    # not against a coincidentally identical drive/path on the client machine.
    if ($Destination -cnotmatch '^[A-Za-z]:[\\/]' -or $Destination -match '[\x00-\x1f]') { throw 'Supply an absolute remote NTFS destination.' }
    $plan=New-SunnyTransferPlan $Root $Hash ([IO.Path]::GetFullPath($Destination).TrimEnd('\')) $Operation
    $manifest=Read-SunnyJson (Join-Path $Root 'release.json')
    $installer=Read-SunnyOperatorSource $Root $manifest 'installer/windows/Sunny.ps1'
    $transfer=Read-SunnyOperatorSource $Root $manifest 'installer/windows/SunnyTransfer.ps1'
    if ($Operation -eq 'Plan') {
        return @{transfer_schema_version=1;status='plan';manifest_sha256=$Hash;destination=$plan.destination;
            bytes=($plan.files | Measure-Object bytes -Sum).Sum;files=$plan.files.Count;remote_action_executed=$false;
            security_settings_changed=$false;next_step='Use approved existing SSH access; Transfer never installs or starts Live.'}
    }
    $ssh=(Get-Command ssh.exe -ErrorAction Stop).Source
    $arguments=@(Get-SunnySshArguments $connection)+@('powershell.exe','-NoProfile','-NonInteractive','-EncodedCommand',(Get-SunnyTransferStarter))
    return Invoke-SunnyTransferExchange $ssh $arguments $Root $plan $installer $transfer $Seconds
}
if ($MyInvocation.InvocationName -ne '.') {
    try {
        Invoke-SunnyTransfer $Action $ConnectionFile $ReleaseDirectory $ExpectedManifestSHA256 $DestinationDirectory $TimeoutSeconds | ConvertTo-Json -Depth 8
        exit 0
    } catch { [Console]::Error.WriteLine('Sunny transfer: '+$_.Exception.Message); exit 1 }
}
