[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Install', 'Status', 'Recover', 'Rollback', 'Uninstall', 'Confirm')]
    [string]$Action = 'Plan',
    [string]$UserLibraryPath,
    [string]$ReleaseDirectory,
    [string]$ExpectedManifestSHA256,
    [string]$DoctorReport
)

# Windows PowerShell 5.1. No process is stopped and no access setting is changed.
# Dot sourcing exposes the same filesystem operations to isolated Windows tests.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-SunnyOwner {
    return [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
}

function Assert-SunnyLiveClosed {
    if (@(Get-Process | Where-Object { $_.ProcessName -like 'Ableton Live*' }).Count) {
        throw 'Save your Sets and quit Live before changing Sunny. Live will not be stopped automatically.'
    }
}

function ConvertTo-SunnyMap($Value) {
    if ($null -eq $Value) { return $null }
    if ($Value -is [System.Management.Automation.PSCustomObject]) {
        $result = [ordered]@{}
        foreach ($property in $Value.PSObject.Properties) {
            $result[$property.Name] = ConvertTo-SunnyMap $property.Value
        }
        return $result
    }
    if ($Value -is [System.Collections.IDictionary]) { return $Value }
    if ($Value -is [Array]) {
        $result = @($Value | ForEach-Object { ConvertTo-SunnyMap $_ })
        return ,$result
    }
    return $Value
}

function Read-SunnyJson([string]$Path) {
    return ConvertTo-SunnyMap (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
}

function Write-SunnyState([string]$Path, $State) {
    $temporary = $Path + '.writing'
    if (Test-Path -LiteralPath $temporary) {
        Assert-SunnyPhysicalPath $temporary
        Remove-Item -LiteralPath $temporary
    }
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes(($State | ConvertTo-Json -Depth 30))
    $stream = [IO.File]::Open($temporary, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
    if (Test-Path -LiteralPath $Path) { [IO.File]::Replace($temporary, $Path, [NullString]::Value) }
    else { [IO.File]::Move($temporary, $Path) }
}

function Assert-SunnyPhysicalPath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    if ($full.StartsWith('\\')) { throw 'The managed installer supports a local Windows volume, not a network share.' }
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw ('Use a physical path; linked files or directories are unsupported: ' + $cursor)
            }
        }
        $parent = [IO.Path]::GetDirectoryName($cursor.TrimEnd('\'))
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
    return $full.TrimEnd('\')
}

function Get-SunnyPaths([string]$Library) {
    if (-not $Library -or -not (Test-Path -LiteralPath $Library -PathType Container)) {
        throw 'Supply the existing User Library folder shown in Live Settings > Library.'
    }
    $libraryPath = Assert-SunnyPhysicalPath $Library
    if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($libraryPath)).DriveFormat -cne 'NTFS') {
        throw 'The managed installer currently supports a local NTFS User Library.'
    }
    $scripts = Join-Path $libraryPath 'Remote Scripts'
    $managed = Join-Path $scripts '.sunny-managed'
    $null = Assert-SunnyPhysicalPath $scripts
    $null = Assert-SunnyPhysicalPath $managed
    return [ordered]@{
        library = $libraryPath; scripts = $scripts; managed = $managed
        active = (Join-Path $scripts 'Sunny'); state = (Join-Path $managed 'state.json')
        stage = (Join-Path $managed 'stage'); previous = (Join-Path $managed 'previous')
        before_active = (Join-Path $managed 'before-active')
        before_previous = (Join-Path $managed 'before-previous')
        cleanup_old = (Join-Path $managed 'cleanup-old')
        lock = (Join-Path $managed 'operation.lock')
    }
}

function Assert-SunnyFileMap($Files) {
    if ($Files -isnot [System.Collections.IDictionary] -or $Files.Count -lt 3) {
        throw 'Missing bridge file checksums.'
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($name in $Files.Keys) {
        $entry = $Files[$name]
        if ($name -cnotmatch '^[A-Za-z0-9_]+\.(py|json|sha256)$' -or -not $seen.Add($name) -or
            $entry.sha256 -cnotmatch '^[0-9a-f]{64}$' -or
            $entry.bytes -isnot [ValueType] -or $entry.bytes -lt 0 -or $entry.bytes -gt 16777216) {
            throw 'Invalid or colliding native bridge file entry.'
        }
    }
    foreach ($required in @('__init__.py', 'bridge_contract.json', 'source.sha256')) {
        if (-not $Files.Contains($required)) { throw ('Missing bridge file: ' + $required) }
    }
}

function Assert-SunnyTree([string]$Root, $Files, [switch]$Partial) {
    $null = Assert-SunnyPhysicalPath $Root
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { throw ('Missing managed bridge folder: ' + $Root) }
    Assert-SunnyFileMap $Files
    $observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($item in Get-ChildItem -LiteralPath $Root -Recurse -Force) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'A managed tree contains a linked entry.' }
        $relative = $item.FullName.Substring($Root.Length + 1).Replace('\', '/')
        if ($item.PSIsContainer) {
            if ($relative -cne '__pycache__') { throw ('Unmanaged directory in Sunny: ' + $relative) }
            continue
        }
        # Only native Python's generated cache is outside the immutable map.
        if ($relative -cmatch '^__pycache__/[A-Za-z0-9_.-]+\.pyc$') { continue }
        if (-not $Files.Contains($relative)) { throw ('Unmanaged file in Sunny; preserve it before proceeding: ' + $relative) }
        $entry = $Files[$relative]
        # A journal-owned staging file may be incomplete after a failed copy.
        # Recovery may remove its recorded name; activation always requires the
        # complete checksum. Unknown names and links are refused in both cases.
        if (-not $Partial -and ($item.Length -ne $entry.bytes -or
            (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry.sha256)) {
            throw ('Managed file changed; preserve your edits before proceeding: ' + $relative)
        }
        $null = $observed.Add($relative)
    }
    if (-not $Partial -and $observed.Count -ne $Files.Count) { throw 'The managed bridge is incomplete.' }
}

function Read-SunnyRelease([string]$Root, [string]$ExpectedHash) {
    if ($ExpectedHash -cnotmatch '^[0-9a-f]{64}$') { throw 'Supply the trusted release.json SHA256 from release verification.' }
    $rootPath = Assert-SunnyPhysicalPath $Root
    $manifestPath = Join-Path $rootPath 'release.json'
    if ((Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $ExpectedHash) {
        throw 'Release manifest checksum mismatch. No installation change was made.'
    }
    $manifest = Read-SunnyJson $manifestPath
    if ($manifest.release_manifest_schema_version -ne 1 -or $manifest.product.name -cne 'Sunny' -or
        $manifest.source.revision -cnotmatch '^[0-9a-f]{40}$' -or
        $manifest.image.platform -cne 'linux/amd64' -or
        $manifest.image.local_immutable_id -cnotmatch '^sha256:[0-9a-f]{64}$' -or
        $manifest.bridge.source_sha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Unsupported or incomplete Sunny release manifest.' }
    Assert-SunnyFileMap $manifest.bridge.files
    $topFiles = @{}
    foreach ($entry in $manifest.files) {
        if ($entry.path -cnotmatch '^[A-Za-z0-9_.-]+(/[A-Za-z0-9_.-]+)*$' -or
            @($entry.path.Split('/') | Where-Object { $_ -in @('.', '..') }).Count -or
            $topFiles.ContainsKey($entry.path) -or $entry.sha256 -cnotmatch '^[0-9a-f]{64}$') {
            throw 'Unsafe or colliding release payload entry.'
        }
        $topFiles[$entry.path] = $entry
        $path = Join-Path $rootPath $entry.path.Replace('/', '\')
        $null = Assert-SunnyPhysicalPath $path
        $item = Get-Item -LiteralPath $path
        if ($item.PSIsContainer -or $item.Length -ne $entry.bytes -or
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry.sha256) {
            throw ('Release payload checksum mismatch: ' + $entry.path)
        }
    }
    foreach ($name in $manifest.bridge.files.Keys) {
        $key = 'native/Sunny/' + $name
        if (-not $topFiles.ContainsKey($key) -or
            $topFiles[$key].sha256 -cne $manifest.bridge.files[$name].sha256 -or
            $topFiles[$key].bytes -ne $manifest.bridge.files[$name].bytes) { throw 'Bridge and release checksums disagree.' }
    }
    $bridgeRoot = Join-Path $rootPath 'native\Sunny'
    Assert-SunnyTree $bridgeRoot $manifest.bridge.files
    $contract = Read-SunnyJson (Join-Path $bridgeRoot 'bridge_contract.json')
    if ($contract.bridge_protocol_version -ne $manifest.bridge.contract.bridge_protocol_version -or
        $contract.target_snapshot_schema_version -ne $manifest.bridge.contract.target_snapshot_schema_version -or
        (Get-Content -LiteralPath (Join-Path $bridgeRoot 'source.sha256') -Raw).Trim() -cne $manifest.bridge.source_sha256) {
        throw 'Native bridge identity disagrees with the release manifest.'
    }
    return [ordered]@{
        source = $bridgeRoot
        metadata = [ordered]@{
            manifest_sha256 = $ExpectedHash; source_revision = $manifest.source.revision
            image_id = $manifest.image.local_immutable_id; bridge_source_sha256 = $manifest.bridge.source_sha256
            contract = $manifest.bridge.contract; files = $manifest.bridge.files
        }
    }
}

function New-SunnyState($Paths) {
    return [ordered]@{
        state_schema_version = 1; owner_sid = (Get-SunnyOwner); user_library = $Paths.library
        active = $null; previous = $null; pending = $null; native_health = 'unobserved'
        active_since = 0; health_evidence = $null
    }
}

function Read-SunnyState($Paths, [switch]$AllowInitialize) {
    if (-not (Test-Path -LiteralPath $Paths.state)) {
        if (Test-Path -LiteralPath $Paths.managed) {
            $unknown = @(Get-ChildItem -LiteralPath $Paths.managed -Force | Where-Object { $_.Name -notin @('operation.lock', 'state.json.writing') })
            if (-not $AllowInitialize -or $unknown.Count) { throw 'The management folder exists without an ownership record; preserve it before proceeding.' }
            $writing = $Paths.state + '.writing'
            if (Test-Path -LiteralPath $writing) {
                $null = Assert-SunnyPhysicalPath $writing
                try { $initial = Read-SunnyJson $writing } catch { throw 'Incomplete initial ownership record was preserved; move the management folder aside before a fresh installation.' }
                if ($initial.state_schema_version -ne 1 -or $initial.owner_sid -cne (Get-SunnyOwner) -or
                    $initial.user_library -ine $Paths.library -or $null -ne $initial.active -or
                    $null -ne $initial.previous -or $null -ne $initial.pending) {
                    throw 'Unregistered initial management content was preserved.'
                }
            }
        }
        return New-SunnyState $Paths
    }
    $null = Assert-SunnyPhysicalPath $Paths.state
    $state = Read-SunnyJson $Paths.state
    if ($state.state_schema_version -ne 1 -or $state.owner_sid -cne (Get-SunnyOwner) -or
        $state.user_library -ine $Paths.library) { throw 'Managed state belongs to another user, library, or unsupported schema.' }
    foreach ($metadata in @($state.active, $state.previous)) {
        if ($null -ne $metadata) { Assert-SunnyFileMap $metadata.files }
    }
    if ($null -ne $state.pending) {
        if ($state.pending.phase -cnotin @('prepared', 'committed')) { throw 'Unsupported pending transaction phase.' }
        foreach ($metadata in @($state.pending.before_active, $state.pending.before_previous, $state.pending.after_active)) {
            if ($null -ne $metadata) { Assert-SunnyFileMap $metadata.files }
        }
    }
    return $state
}

function Assert-SunnyOwnedLocation([string]$Path, $Metadata) {
    if ($null -eq $Metadata) {
        if (Test-Path -LiteralPath $Path) { throw ('Unregistered Sunny content will be preserved; move it aside manually: ' + $Path) }
    } else { Assert-SunnyTree $Path $Metadata.files }
}

function Move-SunnyDirectory([string]$From, [string]$To) {
    if (Test-Path -LiteralPath $To) { throw ('Transaction destination already exists: ' + $To) }
    [IO.Directory]::Move($From, $To)
}

function Remove-SunnyTree([string]$Path, $Metadata, [switch]$Partial) {
    if (Test-Path -LiteralPath $Path) {
        Assert-SunnyTree $Path $Metadata.files -Partial:$Partial
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Recover-SunnyTransaction($Paths, $State) {
    $pending = $State.pending
    if ($null -eq $pending) { return $State }
    Assert-SunnyLiveClosed
    if ($pending.phase -ceq 'prepared') {
        # Before the atomic state commit, recovery always restores the prior release.
        if ($null -ne $pending.after_active -and (Test-Path -LiteralPath $Paths.active) -and
            ($null -eq $pending.before_active -or (Test-Path -LiteralPath $Paths.before_active))) {
            $isNew = $false
            try { Assert-SunnyTree $Paths.active $pending.after_active.files; $isNew = $true } catch { }
            # Retire the fully validated incoming tree atomically. Its existing
            # journal then permits resumable partial cleanup in the private stage.
            if ($isNew) { Move-SunnyDirectory $Paths.active $Paths.stage }
        }
        foreach ($pair in @(
            @($Paths.before_active, $Paths.active, $pending.before_active),
            @($Paths.before_previous, $Paths.previous, $pending.before_previous)
        )) {
            if (Test-Path -LiteralPath $pair[0]) {
                Assert-SunnyOwnedLocation $pair[0] $pair[2]
                if (Test-Path -LiteralPath $pair[1]) { throw 'Recovery found conflicting content; both copies were retained.' }
                Move-SunnyDirectory $pair[0] $pair[1]
            }
            Assert-SunnyOwnedLocation $pair[1] $pair[2]
        }
        if ($null -ne $pending.after_active) { Remove-SunnyTree $Paths.stage $pending.after_active -Partial }
        $State.active = $pending.before_active
        $State.previous = $pending.before_previous
        $State.native_health = $pending.before_health
        $State.active_since = $pending.before_active_since
        $State.health_evidence = $pending.before_health_evidence
    } else {
        # After commit, finish the reversible promotion and retire only the recorded older backup.
        Assert-SunnyOwnedLocation $Paths.active $State.active
        if (Test-Path -LiteralPath $Paths.before_active) {
            Assert-SunnyOwnedLocation $Paths.before_active $pending.before_active
            Move-SunnyDirectory $Paths.before_active $Paths.previous
        }
        if ($null -eq $pending.before_active -and $null -ne $pending.before_previous -and
            (Test-Path -LiteralPath $Paths.before_previous)) {
            Assert-SunnyOwnedLocation $Paths.before_previous $pending.before_previous
            Move-SunnyDirectory $Paths.before_previous $Paths.previous
        }
        Assert-SunnyOwnedLocation $Paths.previous $State.previous
        if ($null -ne $pending.before_previous) {
            if (Test-Path -LiteralPath $Paths.before_previous) {
                Assert-SunnyTree $Paths.before_previous $pending.before_previous.files
                Move-SunnyDirectory $Paths.before_previous $Paths.cleanup_old
            }
            Remove-SunnyTree $Paths.cleanup_old $pending.before_previous -Partial
        }
    }
    $State.pending = $null
    Write-SunnyState $Paths.state $State
    return $State
}

function Set-SunnyRelease($Paths, $State, $Release, [switch]$AllowUnconfirmed) {
    Assert-SunnyLiveClosed
    Assert-SunnyOwnedLocation $Paths.active $State.active
    Assert-SunnyOwnedLocation $Paths.previous $State.previous
    foreach ($path in @($Paths.before_active, $Paths.before_previous, $Paths.cleanup_old)) {
        if (Test-Path -LiteralPath $path) { throw 'An unrecorded transaction backup exists; preserve it before proceeding.' }
    }
    $incoming = if ($null -eq $Release) { $null } else { $Release.metadata }
    if (Test-Path -LiteralPath $Paths.stage) {
        throw 'An unrecorded staging folder exists; preserve it before proceeding.'
    }
    if ($null -ne $incoming -and $null -ne $State.active -and $null -ne $State.previous -and
        $State.native_health -cne 'paired_read_only_ready' -and -not $AllowUnconfirmed) {
        throw 'Confirm the active release with a fresh matching doctor report before retiring its earlier backup. Rollback and uninstall remain available.'
    }
    # Record the transfer before creating its first file. Disk-full and interrupted
    # copies therefore have the same recoverable ownership as activation failures.
    $State.pending = [ordered]@{
        phase = 'prepared'; before_active = $State.active; before_previous = $State.previous
        before_health = $State.native_health; after_active = $incoming
        before_active_since = $State.active_since; before_health_evidence = $State.health_evidence
        started_at = [DateTime]::UtcNow.ToString('o')
    }
    Write-SunnyState $Paths.state $State
    if ($null -ne $incoming) {
        [IO.Directory]::CreateDirectory($Paths.stage) | Out-Null
        foreach ($name in $incoming.files.Keys) {
            Copy-Item -LiteralPath (Join-Path $Release.source $name) -Destination (Join-Path $Paths.stage $name)
        }
        Assert-SunnyTree $Paths.stage $incoming.files
    }
    Assert-SunnyLiveClosed
    if ($null -ne $State.active) { Move-SunnyDirectory $Paths.active $Paths.before_active }
    if ($null -ne $State.previous) { Move-SunnyDirectory $Paths.previous $Paths.before_previous }
    if ($null -ne $incoming) { Move-SunnyDirectory $Paths.stage $Paths.active }
    Assert-SunnyOwnedLocation $Paths.active $incoming
    $State.previous = if ($null -ne $State.active) { $State.active } else { $State.previous }
    $State.active = $incoming
    $State.native_health = if ($null -eq $incoming) { 'uninstalled' } else { 'pending' }
    $State.active_since = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0
    $State.health_evidence = $null
    $State.pending.phase = 'committed'
    Write-SunnyState $Paths.state $State
    return Recover-SunnyTransaction $Paths $State
}

function Confirm-SunnyHealth($Paths, $State, [string]$ReportPath) {
    if ($null -eq $State.active) { throw 'There is no active release to confirm.' }
    Assert-SunnyOwnedLocation $Paths.active $State.active
    if (-not $ReportPath -or (Get-Item -LiteralPath $ReportPath).Length -gt 1048576) {
        throw 'Supply a bounded explicit doctor export for the configured Live host.'
    }
    $report = Read-SunnyJson $ReportPath
    if (-not $report.Contains('cleanup') -or
        $report.cleanup -isnot [Collections.IDictionary] -or
        -not $report.cleanup.Contains('success')) {
        throw 'Doctor report has no complete cleanup evidence.'
    }
    $doctor = $report.doctor
    if ($report.schema_version -ne 1 -or $doctor.schema_version -ne 1 -or
        $report.success -isnot [bool] -or -not $report.success -or
        $report.read_only_ready -isnot [bool] -or -not $report.read_only_ready -or
        $report.cleanup.success -isnot [bool] -or -not $report.cleanup.success -or
        $doctor.success -isnot [bool] -or -not $doctor.success -or
        $doctor.read_only_ready -isnot [bool] -or -not $doctor.read_only_ready -or
        $report.request_id -cnotmatch '^[0-9a-f]{32}$' -or $doctor.request_id -cne $report.request_id) {
        throw 'Doctor report is unsuccessful, malformed, or not correlated.'
    }
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0
    foreach ($timestamp in @($report.observed_at, $doctor.observed_at)) {
        if ($timestamp -isnot [ValueType] -or [double]::IsNaN($timestamp) -or [double]::IsInfinity($timestamp) -or
            $timestamp -gt ($now + 5) -or $timestamp -lt ($now - 300) -or $timestamp -lt $State.active_since) {
            throw 'Doctor evidence is stale, predates activation, or has an invalid clock.'
        }
    }
    foreach ($identity in @($doctor.expected_bridge, $doctor.observed_bridge)) {
        if ($identity.Count -ne 2 -or $identity.source_sha256 -cne $State.active.bridge_source_sha256 -or
            $identity.protocol_version -ne $State.active.contract.bridge_protocol_version) {
            throw 'Doctor bridge identity does not match the active release.'
        }
    }
    foreach ($pair in @(
        @($report.checks, @('client_launch', 'stdio', 'mcp_tools')),
        @($doctor.checks, @('bridge_connection', 'release_pairing', 'native_session', 'native_readiness'))
    )) {
        $layers = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($check in $pair[0]) {
            if ($check.status -cne 'pass' -or -not $layers.Add($check.layer)) { throw 'Doctor checks are incomplete or contradictory.' }
        }
        if (-not $layers.SetEquals([string[]]$pair[1])) { throw 'Doctor checks are incomplete or contradictory.' }
    }
    if ($doctor.session.schema_version -ne 1 -or
        $doctor.session.bridge_instance -cnotmatch '^[0-9a-f]{32}$' -or
        $doctor.session.document_token -cnotmatch '^[0-9a-f]{32}$') { throw 'Doctor native session is malformed.' }
    foreach ($name in @('is_playing', 'session_record', 'record_mode')) {
        if ($doctor.native_state[$name] -isnot [bool]) { throw 'Doctor native readiness is incomplete.' }
    }
    # This confirms the operator-selected route's paired read-only readiness.
    # It does not qualify the installed host/library, licences, sound, or mutations.
    $State.native_health = 'paired_read_only_ready'
    $State.health_evidence = [ordered]@{
        request_id = $report.request_id; observed_at = $doctor.observed_at
        bridge_instance = $doctor.session.bridge_instance; document_token = $doctor.session.document_token
        host_and_library_qualification = 'manual_pending'; audio_qualification = 'pending'
    }
    Write-SunnyState $Paths.state $State
    return $State
}

function Invoke-SunnyLifecycle([string]$Operation, [string]$Library, [string]$ReleaseRoot, [string]$ManifestHash, [string]$ReportPath) {
    if ($PSVersionTable.PSVersion -lt [Version]'5.1') { throw 'Windows PowerShell 5.1 or newer is required.' }
    $paths = Get-SunnyPaths $Library
    $state = Read-SunnyState $paths -AllowInitialize:($Operation -notin @('Plan', 'Status'))
    $release = if ($Operation -in @('Plan', 'Install')) { Read-SunnyRelease $ReleaseRoot $ManifestHash } else { $null }
    if ($Operation -in @('Plan', 'Status')) {
        $currentValid = $false
        $diagnostic = $null
        try { Assert-SunnyOwnedLocation $paths.active $state.active; $currentValid = $true } catch { $diagnostic = $_.Exception.Message }
        return [ordered]@{
            operation = $Operation.ToLowerInvariant(); user_library = $paths.library; owner_sid = (Get-SunnyOwner)
            active = $state.active; previous = $state.previous; pending = $state.pending
            active_files_valid = $currentValid; diagnostic = $diagnostic; native_health = $state.native_health
            release = if ($release) { $release.metadata } else { $null }
            managed_directory = $paths.managed; control_surface_selection = 'manual_in_live'
            security_settings_changed = $false; live_readiness = 'not_observed'
            observed_at = [DateTime]::UtcNow.ToString('o')
        }
    }
    if ($Operation -ne 'Confirm') { Assert-SunnyLiveClosed }
    if (-not (Test-Path -LiteralPath $paths.managed)) {
        if (Test-Path -LiteralPath $paths.active) { throw 'An unregistered Sunny folder exists; it will not be overwritten.' }
        [IO.Directory]::CreateDirectory($paths.managed) | Out-Null
    }
    $null = Assert-SunnyPhysicalPath $paths.lock
    $lock = [IO.File]::Open($paths.lock, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        # Re-read after admission: another invocation may have completed while we inspected the release.
        $state = Read-SunnyState $paths -AllowInitialize
        if (-not (Test-Path -LiteralPath $paths.state)) { Write-SunnyState $paths.state $state }
        $state = Recover-SunnyTransaction $paths $state
        $changed = $false
        if ($Operation -eq 'Install') {
            if ($null -eq $state.active -or $state.active.manifest_sha256 -cne $release.metadata.manifest_sha256) {
                $state = Set-SunnyRelease $paths $state $release; $changed = $true
            } else { Assert-SunnyOwnedLocation $paths.active $state.active }
        } elseif ($Operation -eq 'Rollback') {
            if ($null -eq $state.previous) { throw 'No recorded previous release is available for rollback.' }
            Assert-SunnyOwnedLocation $paths.previous $state.previous
            $state = Set-SunnyRelease $paths $state ([ordered]@{ source = $paths.previous; metadata = $state.previous }) -AllowUnconfirmed
            $changed = $true
        } elseif ($Operation -eq 'Uninstall' -and $null -ne $state.active) {
            $state = Set-SunnyRelease $paths $state $null; $changed = $true
        } elseif ($Operation -eq 'Confirm') {
            $state = Confirm-SunnyHealth $paths $state $ReportPath; $changed = $true
        }
        return [ordered]@{
            operation = $Operation.ToLowerInvariant(); changed = $changed; active = $state.active
            native_health = $state.native_health; pending = $state.pending
            retained_backup = if ($null -ne $state.previous) { $paths.previous } else { $null }
            managed_state = $paths.state; security_settings_changed = $false
            next_step = if ($state.active) { 'Start Live, select Sunny with Input/Output None, then run the matching release doctor. Native and audio qualification remain required.' } else { 'Remove Sunny from its Control Surface slot in Live. Recorded backup and management state are retained.' }
        }
    } finally { $lock.Dispose() }
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        Invoke-SunnyLifecycle $Action $UserLibraryPath $ReleaseDirectory $ExpectedManifestSHA256 $DoctorReport | ConvertTo-Json -Depth 30
        exit 0
    } catch {
        [Console]::Error.WriteLine('Sunny installer: ' + $_.Exception.Message)
        exit 1
    }
}
