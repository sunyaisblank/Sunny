[CmdletBinding()]
param(
    [ValidateSet('Info', 'Log')][string]$Action = 'Info',
    [string]$LogPath,
    [ValidateRange(1024, 65536)][int]$MaxBytes = 65536,
    [switch]$IncludeMessages,
    [string]$RequestId = ([Guid]::NewGuid().ToString('N'))
)

# Read-only host evidence remains usable when the native bridge never loads.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Protect-SunnyHostMessage([string]$Message) {
    $text = $Message -replace '(?i)(password|secret|token|api[_-]?key|authorization)(\s*[:=]\s*)\S+', '$1$2<redacted>'
    $text = $text -replace '-----BEGIN [^-]+PRIVATE KEY-----.*', '<private key omitted>'
    $text = $text -replace '(?i)://[^\s/@]+:[^\s/@]+@', '://<redacted>@'
    $text = $text -replace '[A-Za-z]:[\\/][^\r\n]+', '<path omitted>'
    if ($text.Length -gt 512) { return $text.Substring(0, 512) + '<truncated>' }
    return $text
}

function Read-SunnyHostLog([string]$Path, [int]$Limit, [bool]$Messages) {
    if (-not $Path -or [IO.Path]::GetFileName($Path) -ine 'Log.txt') {
        throw 'Supply the explicit native Live Log.txt path; other filenames are outside this route.'
    }
    $full = Assert-SunnyPhysicalPath $Path
    $before = Get-Item -LiteralPath $full -Force
    if ($before.PSIsContainer -or
        ($before.PSObject.Properties['LinkType'] -and $before.LinkType -eq 'HardLink')) {
        throw 'Native log scope requires a physical regular file without hard links.'
    }
    $stream = [IO.File]::Open($full, [IO.FileMode]::Open, [IO.FileAccess]::Read,
        ([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    try {
        $length = $stream.Length
        $offset = [Math]::Max(0, $length - $Limit)
        $null = $stream.Seek($offset, [IO.SeekOrigin]::Begin)
        $bytes = [byte[]]::new([int]($length - $offset))
        $count = 0
        while ($count -lt $bytes.Length) {
            $read = $stream.Read($bytes, $count, $bytes.Length - $count)
            if ($read -eq 0) { break }
            $count += $read
        }
        # A tail can begin inside a multibyte character. Discard its first partial
        # line before strict decoding, rather than manufacture replacement text.
        $start = 0
        if ($offset -gt 0) {
            while ($start -lt $count -and $bytes[$start] -ne 10) { $start++ }
            if ($start -lt $count) { $start++ }
        }
        # A writer may leave its last line inside a UTF-8 codepoint. Trim that
        # unfinished line as bytes, so complete preceding evidence still decodes
        # strictly and malformed complete lines remain an explicit failure.
        $end = $count
        $partial = $count -gt 0 -and $bytes[$count - 1] -ne 10
        if ($partial) {
            while ($end -gt $start -and $bytes[$end - 1] -ne 10) { $end-- }
        }
        try { $text = [Text.UTF8Encoding]::new($false, $true).GetString($bytes, $start, $end - $start) }
        catch { throw 'Native log encoding is unavailable; a valid UTF-8 log is required.' }
        $changed = $stream.Length -ne $length
    } finally { $stream.Dispose() }
    $null = Assert-SunnyPhysicalPath $full
    $after = Get-Item -LiteralPath $full -Force
    if ($after.PSObject.Properties['LinkType'] -and $after.LinkType -eq 'HardLink') {
        throw 'Native log scope changed to a hard link.'
    }
    $changed = $changed -or $before.CreationTimeUtc -ne $after.CreationTimeUtc -or
        $before.LastWriteTimeUtc -ne $after.LastWriteTimeUtc
    $lines = @($text -split "`n")
    $counts = [ordered]@{ sunny = 0; errors = 0; tracebacks = 0 }
    foreach ($line in $lines) {
        if ($line -match '(?i)sunny') { $counts.sunny++ }
        if ($line -match '(?i)error|exception') { $counts.errors++ }
        if ($line -match '(?i)traceback') { $counts.tracebacks++ }
    }
    $result = [ordered]@{
        file_name = 'Log.txt'; total_bytes_at_read = $length; read_bytes = $count
        range_start = $offset + $start; range_end = $offset + $end
        last_write_at = ([DateTimeOffset]$after.LastWriteTimeUtc).ToUnixTimeMilliseconds() / 1000.0
        truncated = ($offset -gt 0); incomplete = ($changed -or $partial -or $count -ne $bytes.Length)
        encoding = 'utf-8'; matching_line_counts = $counts; messages_included = $Messages
        project_content_may_remain = $Messages; bridge_required = $false
    }
    if ($Messages) {
        # Content is explicit and finite. Keep only Sunny/loading/error evidence;
        # it is still not a guarantee of complete project-content redaction.
        $selected = @($lines | Where-Object { $_ -match '(?i)sunny|error|exception|traceback' })
        $result.messages = @($selected | Select-Object -Last 100 | ForEach-Object { Protect-SunnyHostMessage $_ })
        $result.messages_truncated = $selected.Count -gt 100
    }
    return $result
}

function Invoke-SunnyHost([string]$Operation, [string]$Path, [int]$Limit, [bool]$Messages, [string]$Correlation) {
    if ($PSVersionTable.PSVersion -lt [Version]'5.1') { throw 'Windows PowerShell 5.1 or newer is required.' }
    if ($Limit -lt 1024 -or $Limit -gt 65536) { throw 'Host log byte limit must be between 1024 and 65536.' }
    if ($Correlation -cnotmatch '^[0-9a-f]{32}$') { throw 'RequestId requires 32 lowercase hexadecimal digits.' }
    # Dot source within this function so installer parameter defaults cannot
    # replace the host command's own arguments. No installer action runs.
    if (-not (Get-Command Assert-SunnyPhysicalPath -CommandType Function -ErrorAction SilentlyContinue)) {
        . (Join-Path $PSScriptRoot 'Sunny.ps1')
    }
    $report = [ordered]@{
        schema_version = 1; request_id = $Correlation; success = $true; read_only = $true
        operation = $Operation.ToLowerInvariant()
        observed_at = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0
        native_bridge_required = $false
    }
    if ($Operation -eq 'Log') {
        $report.log = Read-SunnyHostLog $Path $Limit $Messages
    } elseif ($Operation -eq 'Info') {
        $os = Get-CimInstance Win32_OperatingSystem
        $report.host = [ordered]@{
            computer_name = $env:COMPUTERNAME; windows_version = $os.Version; windows_build = $os.BuildNumber
            owner_sid = Get-SunnyOwner; powershell_version = $PSVersionTable.PSVersion.ToString()
        }
        $report.live_processes = @(Get-Process | Where-Object { $_.ProcessName -like 'Ableton Live*' } |
            ForEach-Object { [ordered]@{ process_id = $_.Id; session_id = $_.SessionId; name = $_.ProcessName } })
        $report.unverified = @('Actual Live edition and patch', 'Required licenses', 'Audio driver and audible readiness', 'RDP connect/disconnect behavior')
    } else { throw 'Unsupported read-only host operation.' }
    $report.observed_at = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0
    return $report
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        Invoke-SunnyHost $Action $LogPath $MaxBytes ([bool]$IncludeMessages) $RequestId | ConvertTo-Json -Depth 12
        exit 0
    } catch {
        [Console]::Error.WriteLine('Sunny host read: ' + $_.Exception.Message)
        exit 1
    }
}
