[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Run', 'Recover')][string]$Action = 'Plan',
    [string]$ConnectionFile,
    [string]$ReleaseDirectory,
    [string]$ExpectedManifestSHA256,
    [string]$ImageId,
    [string]$ConfigurationFile,
    [string]$WorkspaceVolume,
    [string]$DockerContext = 'desktop-linux',
    [string]$SessionDirectory,
    [ValidateRange(5, 30)][int]$StartupSeconds = 15,
    [ValidateRange(5, 30)][int]$CleanupSeconds = 15
)

# The MCP client owns one stdio session. Windows owns its loopback SSH forward;
# Docker owns only the matching Linux Sunny process and named project volume.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-SunnyClientDocker([string]$Executable, [string]$Context, [string[]]$Arguments, [double]$Seconds = 10) {
    if ($Seconds -le 0) { throw 'The local Docker command deadline has expired.' }
    $result = Invoke-SunnyBoundedProcess $Executable (@('--context', $Context) + $Arguments) '' $Seconds
    if ($result.exit_code -ne 0) { throw 'The selected local Docker operation failed. Check Docker Desktop and the verified loaded image.' }
    return $result.stdout.Trim()
}

function Get-SunnyClientDaemon([string]$Executable, [string]$Context, [double]$Seconds = 20) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $contextValue = ConvertTo-SunnyMap ((Invoke-SunnyClientDocker $Executable $Context @('context', 'inspect', $Context) $Seconds) | ConvertFrom-Json)
    if ($contextValue.Count -ne 1 -or $contextValue[0].Endpoints.docker.Host -cnotmatch '^npipe:/{4}\./pipe/[A-Za-z0-9_.-]+$') {
        throw 'This Windows profile requires a local Docker Desktop named-pipe endpoint.'
    }
    $daemon = ConvertTo-SunnyMap ((Invoke-SunnyClientDocker $Executable $Context @('info', '--format', '{{json .}}') ($Seconds - $watch.Elapsed.TotalSeconds)) | ConvertFrom-Json)
    if ($daemon.OSType -cne 'linux' -or $daemon.Architecture -notin @('x86_64', 'amd64') -or -not $daemon.ID) {
        throw 'A running local Linux/amd64 Docker Desktop engine is required.'
    }
    return [string]$daemon.ID
}

function Remove-SunnyClientContainers([string]$Executable, [string]$Context, [string]$Daemon,
    [string]$Owner, [int]$Seconds) {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $remaining = {
        if ($watch.Elapsed.TotalSeconds -ge $Seconds) { throw 'Owned-container cleanup deadline expired.' }
        return $Seconds - $watch.Elapsed.TotalSeconds
    }
    if ((Get-SunnyClientDaemon $Executable $Context (& $remaining)) -cne $Daemon) { throw 'Docker daemon identity changed; owned-container cleanup is unconfirmed.' }
    $selector = 'org.sunny.client.owner=' + $Owner
    $ids = @( (Invoke-SunnyClientDocker $Executable $Context @('ps', '-aq', '--no-trunc', '--filter', ('label=' + $selector)) (& $remaining)) -split '\s+' | Where-Object { $_ } )
    if ($ids.Count -gt 2 -or @($ids | Where-Object { $_ -cnotmatch '^[0-9a-f]{64}$' }).Count) {
        throw 'Owned-container selection is ambiguous; preserve the ownership label for manual status.'
    }
    foreach ($id in $ids) {
        if ($watch.Elapsed.TotalSeconds -ge $Seconds) { throw 'Owned-container cleanup deadline expired.' }
        $items = ConvertTo-SunnyMap ((Invoke-SunnyClientDocker $Executable $Context @('container', 'inspect', $id) (& $remaining)) | ConvertFrom-Json)
        $item = $items[0]
        if ($items.Count -ne 1 -or $item.Id -cne $id -or
            $item.Config.Labels['org.sunny.client.owner'] -cne $Owner -or
            $item.Name -notin @(('/sunny-client-' + $Owner + '-validate'), ('/sunny-client-' + $Owner + '-run'))) {
            throw 'Container ownership could not be confirmed; no unrelated container was removed.'
        }
        $null = Invoke-SunnyClientDocker $Executable $Context @('container', 'rm', '--force', $id) (& $remaining)
    }
    if ($watch.Elapsed.TotalSeconds -ge $Seconds) { throw 'Owned-container absence was not confirmed within the cleanup deadline.' }
    $left = Invoke-SunnyClientDocker $Executable $Context @('ps', '-aq', '--no-trunc', '--filter', ('label=' + $selector)) (& $remaining)
    if ($left -or (Get-SunnyClientDaemon $Executable $Context (& $remaining)) -cne $Daemon) {
        throw 'Owned-container cleanup could not be confirmed on the original daemon.'
    }
}

function New-SunnyClientJob {
    if (-not ('Sunny.ClientJob' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
namespace Sunny {
  public sealed class ClientJob : IDisposable {
    [StructLayout(LayoutKind.Sequential)] struct Basic {
      public long ProcessTime, JobTime; public uint Flags;
      public UIntPtr Minimum, Maximum; public uint Active;
      public UIntPtr Affinity; public uint Priority, Scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct Io {
      public ulong ReadOps, WriteOps, OtherOps, ReadBytes, WriteBytes, OtherBytes;
    }
    [StructLayout(LayoutKind.Sequential)] struct Limits {
      public Basic Basic; public Io Io;
      public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
    }
    [DllImport("kernel32.dll", SetLastError=true)] static extern SafeFileHandle CreateJobObject(IntPtr attributes, string name);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetInformationJobObject(SafeFileHandle job, int kind, ref Limits limits, uint size);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AssignProcessToJobObject(SafeFileHandle job, IntPtr process);
    readonly SafeFileHandle handle;
    public ClientJob() {
      handle = CreateJobObject(IntPtr.Zero, null);
      if (handle.IsInvalid) throw new Win32Exception();
      var limits = new Limits(); limits.Basic.Flags = 0x2000;
      if (!SetInformationJobObject(handle, 9, ref limits, (uint)Marshal.SizeOf(typeof(Limits)))) {
        handle.Dispose(); throw new Win32Exception();
      }
    }
    public void Add(Process process) {
      if (!AssignProcessToJobObject(handle, process.Handle)) throw new Win32Exception();
    }
    public void Dispose() { handle.Dispose(); }
  }
}
'@
    }
    return [Sunny.ClientJob]::new()
}

function Start-SunnyClientForward($Connection, [string]$BindHost, [int]$LocalPort, [int]$RemotePort, [int]$Seconds, $Job) {
    $base = @(Get-SunnySshArguments $Connection)
    $arguments = @($base[0..($base.Count - 2)]) + @('-N', '-o', 'ExitOnForwardFailure=yes',
        '-o', 'ServerAliveInterval=5', '-o', 'ServerAliveCountMax=2', '-L',
        ('127.0.0.1:' + $LocalPort + ':' + $BindHost + ':' + $RemotePort), $base[-1])
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = (Get-Command ssh.exe -ErrorAction Stop).Source
    $start.Arguments = (@($arguments | ForEach-Object { ConvertTo-SunnyWindowsArgument $_ }) -join ' ')
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    # OpenSSH diagnostics inherit stderr. No tunnel stdout or terminal is used.
    $start.RedirectStandardOutput = $true
    $process = [Diagnostics.Process]::Start($start)
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        $Job.Add($process)
        while ($watch.Elapsed.TotalSeconds -lt $Seconds) {
            if ($process.HasExited) { throw 'SSH forwarding was refused. Verify the existing host identity and approved account access.' }
            $listeners = @(Get-NetTCPConnection -State Listen -LocalAddress '127.0.0.1' -LocalPort $LocalPort -ErrorAction SilentlyContinue)
            if ($listeners.Count) {
                if ($listeners.Count -ne 1 -or $listeners[0].OwningProcess -ne $process.Id) {
                    throw 'The requested local forward port is occupied by another process.'
                }
                return $process
            }
            Start-Sleep -Milliseconds 50
        }
        throw 'The approved SSH forward did not become ready before its startup deadline.'
    } catch {
        if (-not $process.HasExited) { $process.Kill(); $null = $process.WaitForExit(1000) }
        $process.Dispose()
        throw
    }
}

function Start-SunnyClientStdio([string]$Executable, [string[]]$Arguments, $Job) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.Arguments = (@($Arguments | ForEach-Object { ConvertTo-SunnyWindowsArgument $_ }) -join ' ')
    $start.UseShellExecute = $false
    # Inherit all three handles without decoding/re-encoding MCP bytes.
    # PowerShell must never write an object or progress message to this stdout.
    $process = [Diagnostics.Process]::Start($start)
    try { $Job.Add($process); return $process }
    catch {
        if (-not $process.HasExited) { $process.Kill(); $null = $process.WaitForExit(1000) }
        $process.Dispose()
        throw
    }
}

function Remove-SunnyClientSessionDirectory([string]$Directory) {
    $null = Assert-SunnyPhysicalPath $Directory
    foreach ($item in Get-ChildItem -LiteralPath $Directory -Force) {
        if ($item.PSIsContainer -or $item.Name -notin @('session.json', 'session.json.writing', 'configuration.json', 'validation.cid', 'runtime.cid') -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
            ($item.PSObject.Properties['LinkType'] -and $item.LinkType -eq 'HardLink')) {
            throw 'Unexpected session recovery content was preserved.'
        }
    }
    Remove-Item -LiteralPath $Directory -Recurse -Force
}

function Recover-SunnyClient([string]$Directory, [int]$Seconds) {
    . (Join-Path $PSScriptRoot 'SunnyRemote.ps1')
    . (Join-Path $PSScriptRoot 'Sunny.ps1')
    $full = Assert-SunnyPhysicalPath $Directory
    $path = Assert-SunnyPhysicalPath (Join-Path $full 'session.json')
    if ((Get-Item -LiteralPath $path).Length -gt 4096) { throw 'Invalid bounded client recovery record.' }
    $state = Read-SunnyJson $path
    if ($state.schema_version -ne 1 -or $state.owner_sid -cne (Get-SunnyOwner) -or $state.owner -cnotmatch '^[0-9a-f]{32}$' -or
        [IO.Path]::GetFileName($full) -cne ('Sunny-client-' + $state.owner) -or $state.context -cnotmatch '^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$') {
        throw 'Unsupported or foreign client recovery record.'
    }
    $parent = Get-Process -Id $state.parent_pid -ErrorAction SilentlyContinue
    if ($null -ne $parent -and $parent.StartTime.ToUniversalTime().Ticks.ToString() -ceq $state.parent_started) {
        throw 'The recorded client process is still running; close that MCP session before recovery.'
    }
    $docker = (Get-Command docker.exe -ErrorAction Stop).Source
    Remove-SunnyClientContainers $docker $state.context $state.daemon_id $state.owner $Seconds
    Remove-SunnyClientSessionDirectory $full
    return [ordered]@{schema_version=1;operation='recover';owner=$state.owner;containers_absent=$true;project_volume_removed=$false}
}

function Invoke-SunnyClient([string]$Operation, [string]$ConnectionPath, [string]$ArtifactRoot,
    [string]$Hash, [string]$ImmutableImage, [string]$ConfigurationPath, [string]$Volume,
    [string]$Context, [int]$StartupBudget, [int]$CleanupBudget) {
    . (Join-Path $PSScriptRoot 'SunnyRemote.ps1')
    . (Join-Path $PSScriptRoot 'Sunny.ps1')
    if ($PSVersionTable.PSVersion -lt [Version]'5.1' -or -not [Environment]::Is64BitProcess) { throw '64-bit Windows PowerShell 5.1 or newer is required.' }
    if ($Operation -notin @('Plan', 'Run') -or $StartupBudget -lt 5 -or $StartupBudget -gt 30 -or $CleanupBudget -lt 5 -or $CleanupBudget -gt 30) { throw 'Invalid client operation or finite process budgets.' }
    if ($Context -cnotmatch '^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$' -or $Volume -cnotmatch '^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$') { throw 'Supply an explicit local Docker context and named workspace volume.' }
    if ($ImmutableImage -cnotmatch '^sha256:[0-9a-f]{64}$') { throw 'Supply the verified loaded image immutable ID; tags are unsupported.' }
    $connection = Read-SunnyConnection $ConnectionPath
    $release = Read-SunnyRelease $ArtifactRoot $Hash
    $manifestBytes = [IO.File]::ReadAllBytes((Join-Path $ArtifactRoot 'release.json'))
    if ((Get-SunnyBytesHash $manifestBytes) -cne $Hash) { throw 'Release manifest changed after verification.' }
    $manifest = ConvertTo-SunnyMap ([Text.UTF8Encoding]::new($false, $true).GetString($manifestBytes) | ConvertFrom-Json)
    if ($manifest.configuration_schema_version -ne 1 -or $manifest.configuration_contract -cne 'versioned_json' -or
        $ImmutableImage -notin @($manifest.image.oci_index_digest, $manifest.image.oci_manifest_digest, $manifest.image.oci_config_digest)) {
        throw 'The selected immutable image is not paired with this versioned release.'
    }
    $configuration = Assert-SunnyPhysicalPath $ConfigurationPath
    $item = Get-Item -LiteralPath $configuration
    if ($item.PSIsContainer -or $item.Length -gt 65536 -or $configuration.Contains(',')) { throw 'Supply a physical configuration file of at most 64 KiB without a comma in its mount path.' }
    foreach ($key in @('SUNNY_CONFIG_PATH', 'SUNNY_ABLETON_HOST', 'SUNNY_TCP_PORT', 'SUNNY_WORKSPACE_PATH', 'SUNNY_WORKSPACE_RECOVERY', 'SUNNY_BIND_HOST', 'DOCKER_HOST', 'DOCKER_CONTEXT', 'DOCKER_TLS_VERIFY', 'DOCKER_CERT_PATH')) {
        if (Test-Path ('Env:' + $key)) { throw ('Remove inherited ambiguous setting before this explicit launcher: ' + $key) }
    }
    $docker = (Get-Command docker.exe -ErrorAction Stop).Source
    $daemon = Get-SunnyClientDaemon $docker $Context
    $images = ConvertTo-SunnyMap ((Invoke-SunnyClientDocker $docker $Context @('image', 'inspect', $ImmutableImage)) | ConvertFrom-Json)
    if ($images.Count -ne 1 -or $images[0].Id -cne $ImmutableImage -or $images[0].Os -cne 'linux' -or $images[0].Architecture -cne 'amd64' -or
        $images[0].Config.User -cne 'sunny' -or $images[0].Config.Labels['org.opencontainers.image.revision'] -cne $release.metadata.source_revision) { throw 'The loaded image identity or supported runtime differs from the paired release.' }
    $owner = [Guid]::NewGuid().ToString('N')
    $directory = Join-Path ([IO.Path]::GetTempPath()) ('Sunny-client-' + $owner)
    $null = [IO.Directory]::CreateDirectory($directory)
    $forward = $null; $stdio = $null; $job = $null
    try {
        $parent = Get-Process -Id $PID
        Write-SunnyState (Join-Path $directory 'session.json') ([ordered]@{schema_version=1;owner_sid=(Get-SunnyOwner);
            owner=$owner;context=$Context;daemon_id=$daemon;parent_pid=$PID;parent_started=$parent.StartTime.ToUniversalTime().Ticks.ToString()})
        [Console]::Error.WriteLine('Sunny client recovery directory: ' + $directory)
        [Console]::Error.WriteLine('Sunny client ownership: org.sunny.client.owner=' + $owner)
        $job = New-SunnyClientJob
        # Capture a fixed verified byte copy so editing the external file cannot
        # redirect this session between validation and its native connection.
        $selected = Join-Path $directory 'configuration.json'
        [IO.File]::Copy($configuration, $selected, $false)
        $mount = 'type=bind,source=' + $selected + ',target=/run/sunny/configuration.json,readonly'
        $validation = Invoke-SunnyClientDocker $docker $Context @('run', '--rm', '--pull=never', '--network', 'none',
            '--name', ('sunny-client-' + $owner + '-validate'), '--label', ('org.sunny.client.owner=' + $owner),
            '--cidfile', (Join-Path $directory 'validation.cid'), '--mount', $mount, $ImmutableImage,
            '--validate-config', '/run/sunny/configuration.json') $StartupBudget
        $value = ConvertTo-SunnyMap ($validation | ConvertFrom-Json)
        if ($value.client.transport.mode -cne 'tcp' -or $value.client.transport.host -cne 'host.docker.internal' -or
            $null -eq $value.client.workspace -or $value.client.workspace.path -cnotmatch '^/data/[^/]+$' -or
            $value.client.workspace.path -cmatch '^/data/\.{1,2}$' -or -not $value.Contains('native')) {
            throw 'The supported remote profile requires both roles, host.docker.internal, and a durable workspace file directly under /data.'
        }
        if ($Operation -eq 'Plan') {
            return [ordered]@{ schema_version = 1; operation = 'plan'; ssh_started = $false; image_id = $ImmutableImage;
                host = $connection.host; user = $connection.user; native_port = $value.native.bridge.port;
                local_forward_port = $value.client.transport.port; workspace_volume = $Volume;
                workspace_path = $value.client.workspace.path; manifest_sha256 = $Hash;
                manual_steps = @('Use the existing approved SSH identity and verified known-host file.', 'Select Sunny as a native Live Control Surface and keep the native bridge on loopback.') }
        }
        $forward = Start-SunnyClientForward $connection $value.native.bridge.bind_host $value.client.transport.port $value.native.bridge.port $StartupBudget $job
        $arguments = @('--context', $Context, 'run', '-i', '--rm', '--pull=never',
            '--name', ('sunny-client-' + $owner + '-run'), '--label', ('org.sunny.client.owner=' + $owner),
            '--cidfile', (Join-Path $directory 'runtime.cid'), '--mount', $mount,
            '--mount', ('type=volume,source=' + $Volume + ',target=/data'),
            '--env', 'SUNNY_CONFIG_PATH=/run/sunny/configuration.json', $ImmutableImage)
        $stdio = Start-SunnyClientStdio $docker $arguments $job
        while (-not $stdio.WaitForExit(100)) {
            if ($forward.HasExited) { throw 'The approved SSH tunnel ended; the MCP session will close without repeating mutations.' }
        }
        return [int]$stdio.ExitCode
    } finally {
        $failure = $null
        foreach ($process in @($stdio, $forward)) {
            if ($null -eq $process) { continue }
            try {
                if (-not $process.HasExited) {
                    $process.Kill()
                    if (-not $process.WaitForExit(1000)) { throw 'Owned client process cleanup was not confirmed.' }
                }
            } catch { $failure = $_ }
            finally { $process.Dispose() }
        }
        if ($null -ne $job) { $job.Dispose() }
        try { Remove-SunnyClientContainers $docker $Context $daemon $owner $CleanupBudget }
        catch { $failure = $_ }
        if ($null -ne $failure) {
            [Console]::Error.WriteLine('Client cleanup is unconfirmed; preserve recovery directory: ' + $directory)
            throw $failure
        }
        if (Test-Path -LiteralPath $directory) { Remove-SunnyClientSessionDirectory $directory }
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        if ($Action -eq 'Recover') { Recover-SunnyClient $SessionDirectory $CleanupSeconds | ConvertTo-Json -Depth 8; exit 0 }
        $result = Invoke-SunnyClient $Action $ConnectionFile $ReleaseDirectory $ExpectedManifestSHA256 $ImageId $ConfigurationFile $WorkspaceVolume $DockerContext $StartupSeconds $CleanupSeconds
        if ($Action -eq 'Plan') { $result | ConvertTo-Json -Depth 12; exit 0 }
        exit $result
    } catch { [Console]::Error.WriteLine('Sunny client: ' + $_.Exception.Message); exit 1 }
}
