[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Status', 'Install', 'Recover', 'Rollback', 'Uninstall', 'Confirm', 'Info', 'Log')]
    [string]$Action = 'Plan',
    [string]$ConnectionFile,
    [string]$ReleaseDirectory,
    [string]$ExpectedManifestSHA256,
    [string]$UserLibraryPath,
    [string]$RemoteReleaseDirectory,
    [string]$RemoteDoctorReport,
    [string]$LogPath,
    [switch]$IncludeMessages,
    [ValidateRange(5, 120)][int]$TimeoutSeconds = 30,
    [string]$RequestId = ([Guid]::NewGuid().ToString('N'))
)

# Uses existing approved access only. It never registers keys, enables sshd,
# changes firewall rules, requests a password, or stops an application.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function ConvertTo-SunnyWindowsArgument([string]$Argument) {
    # Windows CRT argv quoting, including trailing backslashes and literal quotes.
    $builder = [Text.StringBuilder]::new()
    $null = $builder.Append('"')
    $slashes = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq '\') { $slashes++; continue }
        if ($character -eq '"') { $null = $builder.Append(('\' * (2 * $slashes + 1))) }
        else { $null = $builder.Append(('\' * $slashes)) }
        $null = $builder.Append($character)
        $slashes = 0
    }
    $null = $builder.Append(('\' * (2 * $slashes))).Append('"')
    return $builder.ToString()
}

function Read-SunnyConnection([string]$Path) {
    $physical = Assert-SunnyPhysicalPath $Path
    if ((Get-Item -LiteralPath $physical).Length -gt 8192) { throw 'Connection configuration exceeds 8 KiB.' }
    $text = [IO.File]::ReadAllText($physical, [Text.UTF8Encoding]::new($false, $true)).Trim()
    if (-not $text.StartsWith('{') -or -not $text.EndsWith('}')) { throw 'Connection configuration must be one flat JSON object.' }
    # This small schema has scalar values only. Consume every member to reject
    # duplicate/escaped duplicate keys before PowerShell's last-value JSON parser.
    $string = '"(?:[^"\\\x00-\x1f]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"'
    $pattern = '\G\s*(?<key>' + $string + ')\s*:\s*(?<value>' + $string + '|-?(?:0|[1-9][0-9]*))\s*(?<end>,|$)'
    $body = $text.Substring(1, $text.Length - 2)
    $keys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $position = 0
    while ($position -lt $body.Length) {
        # \G requires an explicit starting position for each successive member.
        $expression = [regex]::new($pattern, [Text.RegularExpressions.RegexOptions]::None, [TimeSpan]::FromSeconds(1))
        $match = $expression.Match($body, $position)
        if (-not $match.Success -or $match.Index -ne $position) { throw 'Connection configuration has invalid scalar JSON.' }
        $key = ('{"value":' + $match.Groups['key'].Value + '}' | ConvertFrom-Json).value
        if (-not $keys.Add($key)) { throw 'Connection configuration has duplicate fields.' }
        $position += $match.Length
        if ($match.Groups['end'].Value -eq ',' -and -not $body.Substring($position).Trim()) {
            throw 'Connection configuration has a trailing comma.'
        }
    }
    if (-not $keys.SetEquals([string[]]@('connection_schema_version', 'host', 'port', 'user', 'identity_file', 'known_hosts_file'))) {
        throw 'Connection configuration has missing or unknown fields.'
    }
    $value = ConvertTo-SunnyMap ($text | ConvertFrom-Json)
    if ($value.connection_schema_version -isnot [long] -and $value.connection_schema_version -isnot [int]) { throw 'Connection schema must be integer 1.' }
    if ($value.connection_schema_version -ne 1) { throw 'Unsupported connection schema; preserve the file and migrate explicitly.' }
    if (($value.port -isnot [long] -and $value.port -isnot [int]) -or $value.port -lt 1 -or $value.port -gt 65535) { throw 'SSH port must be integer 1..65535.' }
    if ($value.host -isnot [string] -or $value.host.Length -gt 253 -or
        $value.host -cnotmatch '^[A-Za-z0-9:._-]+$' -or $value.host.StartsWith('-') -or
        [Uri]::CheckHostName($value.host) -eq [UriHostNameType]::Unknown) { throw 'SSH host must be an explicit ASCII IP address or DNS name.' }
    if ($value.user -isnot [string] -or $value.user -cnotmatch '^[A-Za-z0-9_.-]{1,64}$') { throw 'This profile requires an explicit Windows local-account name.' }
    foreach ($name in @('identity_file', 'known_hosts_file')) {
        if ($value[$name] -isnot [string] -or $value[$name] -cnotmatch '^[A-Za-z]:[\\/]' -or $value[$name] -match '[\x00-\x1f]') { throw 'SSH identity and known-host files require absolute physical Windows paths.' }
        $value[$name] = Assert-SunnyPhysicalPath $value[$name]
        if (-not (Test-Path -LiteralPath $value[$name] -PathType Leaf)) { throw 'Supply the existing approved SSH identity and known-host files.' }
    }
    return $value
}

function Get-SunnySshArguments($Connection) {
    return @('-F', 'none', '-T', '-a', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
        '-o', 'UpdateHostKeys=no', '-o', ('UserKnownHostsFile="' + $Connection.known_hosts_file + '"'),
        '-o', 'GlobalKnownHostsFile=none', '-o', 'IdentitiesOnly=yes', '-o', 'ForwardAgent=no',
        '-o', 'ControlMaster=no', '-o', 'ControlPath=none', '-o', 'PermitLocalCommand=no',
        '-o', 'ProxyCommand=none', '-o', 'ProxyJump=none', '-o', 'PasswordAuthentication=no',
        '-o', 'KbdInteractiveAuthentication=no', '-o', 'ConnectionAttempts=1', '-o', 'ConnectTimeout=10',
        '-i', $Connection.identity_file, '-p', [string]$Connection.port, '-l', $Connection.user, $Connection.host)
}

function Get-SunnyBytesHash([byte[]]$Bytes) {
    $digest = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($digest.ComputeHash($Bytes)).Replace('-', '').ToLowerInvariant() }
    finally { $digest.Dispose() }
}

function Read-SunnyOperatorSource([string]$Root, $Manifest, [string]$Relative) {
    $entries = @($Manifest.files | Where-Object { $_.path -ceq $Relative })
    if ($entries.Count -ne 1) { throw 'Operator script is not registered in the verified release.' }
    $path = Assert-SunnyPhysicalPath (Join-Path $Root $Relative.Replace('/', '\'))
    if ((Get-Item -LiteralPath $path).Length -gt 131072) { throw 'Operator script exceeds its transfer bound.' }
    $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ne $entries[0].bytes -or (Get-SunnyBytesHash $bytes) -cne $entries[0].sha256) {
        throw 'Operator script changed after release verification.'
    }
    return [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
}

function New-SunnyRemotePayload([string]$Operation, $Arguments, [string]$Installer, [string]$HostReader) {
    if ($Operation -notin @('Status', 'Install', 'Recover', 'Rollback', 'Uninstall', 'Confirm', 'Info', 'Log')) { throw 'Unsupported remote operation.' }
    $encode = { param([string]$Text) [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($Text)) }
    $installerEncoded = & $encode $Installer
    $hostEncoded = & $encode $HostReader
    $argumentsEncoded = & $encode ($Arguments | ConvertTo-Json -Depth 8 -Compress)
    # Only ASCII base64 crosses the Windows shell/console boundary; Unicode and
    # quoted paths are decoded as JSON data, never spliced into command syntax.
    $lines = @(
        '$ErrorActionPreference = "Stop"',
        'try {',
        ('. ([ScriptBlock]::Create([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("' + $installerEncoded + '"))))'),
        ('. ([ScriptBlock]::Create([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("' + $hostEncoded + '"))))'),
        ('$sunnyRemoteInput = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String("' + $argumentsEncoded + '")) | ConvertFrom-Json')
    )
    if ($Operation -in @('Info', 'Log')) {
        $lines += '$sunnyRemoteResult = Invoke-SunnyHost $sunnyRemoteInput.operation $sunnyRemoteInput.log_path 65536 $sunnyRemoteInput.include_messages $sunnyRemoteInput.request_id'
    } else {
        $lines += '$sunnyRemoteValue = Invoke-SunnyLifecycle $sunnyRemoteInput.operation $sunnyRemoteInput.library $sunnyRemoteInput.release $sunnyRemoteInput.manifest_hash $sunnyRemoteInput.doctor_report'
        $lines += '$sunnyRemoteResult = [ordered]@{schema_version=1;request_id=$sunnyRemoteInput.request_id;observed_at=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()/1000.0;success=$true;operation=$sunnyRemoteInput.operation.ToLowerInvariant();result=$sunnyRemoteValue}'
    }
    $lines += @('$sunnyRemoteResult | ConvertTo-Json -Depth 30', 'exit 0',
        '} catch { [Console]::Error.WriteLine("Sunny remote operation failed: " + $_.Exception.Message); exit 1 }')
    return $lines -join "`n"
}

function Start-SunnyUtf8PipeProcess([Diagnostics.ProcessStartInfo]$Start) {
    if ($Start.UseShellExecute -or -not $Start.RedirectStandardInput) {
        throw 'Sunny pipe process requires redirected standard input without shell execution.'
    }
    $encoding = [Text.UTF8Encoding]::new($false, $true)
    if ($null -ne $Start.GetType().GetProperty('StandardInputEncoding')) {
        $Start.StandardInputEncoding = $encoding
        return [Diagnostics.Process]::Start($Start)
    }
    # Windows PowerShell 5.1 uses .NET Framework, which constructs and
    # AutoFlushes the input StreamWriter with Console.InputEncoding during
    # Process.Start. Its UTF-8 BOM otherwise precedes even raw BaseStream frames.
    $previous = [Console]::InputEncoding
    $process = $null
    try {
        [Console]::InputEncoding = $encoding
        $process = [Diagnostics.Process]::Start($Start)
    } finally {
        try { [Console]::InputEncoding = $previous }
        catch {
            if ($null -ne $process) {
                try {
                    if (-not $process.HasExited) {
                        $process.Kill()
                        if (-not $process.WaitForExit(1000)) { throw ('Owned pipe process cleanup is unconfirmed for PID ' + $process.Id) }
                    }
                } finally { $process.Dispose() }
            }
            throw
        }
    }
    return $process
}

function Invoke-SunnyBoundedProcess([string]$Executable, [string[]]$Arguments, [string]$InputText, [double]$Seconds) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.Arguments = (@($Arguments | ForEach-Object { ConvertTo-SunnyWindowsArgument $_ }) -join ' ')
    $start.UseShellExecute = $false
    $start.RedirectStandardInput = $true; $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false, $true)
    $start.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)
    $process = Start-SunnyUtf8PipeProcess $start
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $out = [Text.StringBuilder]::new(); $err = [Text.StringBuilder]::new()
    $outBuffer = [char[]]::new(4096); $errBuffer = [char[]]::new(4096)
    $outRead = $process.StandardOutput.ReadAsync($outBuffer, 0, $outBuffer.Length)
    $errRead = $process.StandardError.ReadAsync($errBuffer, 0, $errBuffer.Length)
    try {
        # Async input is bounded too: a server that never reads cannot hang this
        # caller before its command deadline. The payload contains no credentials.
        $write = $process.StandardInput.WriteAsync($InputText)
        while (-not $write.IsCompleted) {
            if ($watch.Elapsed.TotalSeconds -ge $Seconds) { throw 'Remote command deadline expired; inspect status before repeating a state change.' }
            Start-Sleep -Milliseconds 10
        }
        try { $null = $write.GetAwaiter().GetResult() }
        catch {
            $inputError = $_.Exception.Message
            # A failed write may mean that the owned child declined its bootstrap
            # before reading it. Retain bounded stderr within the original budget;
            # the operation outcome is unknown and this diagnostic never retries.
            $diagnosticEnd = [Math]::Min($Seconds, $watch.Elapsed.TotalSeconds + 1)
            while ($null -ne $errRead -and $err.Length -lt 8192 -and $watch.Elapsed.TotalSeconds -lt $diagnosticEnd) {
                if (-not $errRead.IsCompleted) { Start-Sleep -Milliseconds 10; continue }
                try { $count = $errRead.GetAwaiter().GetResult() }
                catch { break }
                $null = $err.Append($errBuffer, 0, [Math]::Min($count, 8192 - $err.Length))
                if ($count -le 0) { break }
                $errRead = $process.StandardError.ReadAsync($errBuffer, 0, $errBuffer.Length)
            }
            $childExit = if ($process.HasExited) { [string]$process.ExitCode } else { 'not_exited' }
            $writer = $process.StandardInput.Encoding
            throw ('Remote input pipe failed; outcome unknown, inspect status before repeating. Owned child PID=' + $process.Id +
                ' exit=' + $childExit + ' input_codepage=' + $writer.CodePage +
                ' input_preamble=' + [BitConverter]::ToString($writer.GetPreamble()) +
                '; child stderr=' + $err.ToString() + '; input error=' + $inputError)
        }
        $process.StandardInput.Close()
        while ($null -ne $outRead -or $null -ne $errRead -or -not $process.HasExited) {
            if ($watch.Elapsed.TotalSeconds -ge $Seconds) { throw 'Remote command deadline expired; inspect status before repeating a state change.' }
            foreach ($channel in @('out', 'err')) {
                $task = if ($channel -eq 'out') { $outRead } else { $errRead }
                if ($null -eq $task -or -not $task.IsCompleted) { continue }
                $count = $task.GetAwaiter().GetResult()
                if ($channel -eq 'out') {
                    if ($out.Length + $count -gt 1048576) { throw 'Remote report exceeds the 1 MiB bound.' }
                    $null = $out.Append($outBuffer, 0, $count)
                    $outRead = if ($count -gt 0) { $process.StandardOutput.ReadAsync($outBuffer, 0, $outBuffer.Length) } else { $null }
                } else {
                    $null = $err.Append($errBuffer, 0, [Math]::Min($count, [Math]::Max(0, 8192 - $err.Length)))
                    $errRead = if ($count -gt 0) { $process.StandardError.ReadAsync($errBuffer, 0, $errBuffer.Length) } else { $null }
                }
            }
            Start-Sleep -Milliseconds 10
        }
        return @{ exit_code = $process.ExitCode; stdout = $out.ToString(); stderr = $err.ToString() }
    } finally {
        if (-not $process.HasExited) {
            $process.Kill()
            if (-not $process.WaitForExit(1000)) { throw ('Owned SSH process cleanup is unconfirmed for PID ' + $process.Id) }
        }
        $process.Dispose()
    }
}

function Assert-SunnyRemoteReport($Report, [string]$Operation, [string]$Correlation, [double]$MaxAge) {
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0
    if (($Report.schema_version -isnot [long] -and $Report.schema_version -isnot [int]) -or
        $Report.schema_version -ne 1 -or $Report.request_id -cne $Correlation -or
        $Report.operation -cne $Operation.ToLowerInvariant() -or $Report.success -isnot [bool] -or -not $Report.success -or
        $Report.observed_at -isnot [ValueType] -or $Report.observed_at -is [bool] -or
        [double]::IsNaN([double]$Report.observed_at) -or [double]::IsInfinity([double]$Report.observed_at) -or
        $Report.observed_at -gt $now + 5 -or $now - $Report.observed_at -gt $MaxAge) {
        throw 'Remote report is malformed, stale or uncorrelated.'
    }
    if ($Operation -in @('Info', 'Log') -and
        ($Report.read_only -isnot [bool] -or -not $Report.read_only -or
        $Report.native_bridge_required -isnot [bool] -or $Report.native_bridge_required)) {
        throw 'Host read effect classification is malformed.'
    }
}

function Invoke-SunnyRemote([string]$Operation, [string]$ConnectionPath, [string]$ArtifactRoot, [string]$Hash,
    [string]$Library, [string]$RemoteArtifact, [string]$DoctorReportPath, [string]$NativeLog,
    [bool]$Messages, [int]$Seconds, [string]$Correlation) {
    . (Join-Path $PSScriptRoot 'Sunny.ps1')
    if ($Correlation -cnotmatch '^[0-9a-f]{32}$') { throw 'RequestId requires 32 lowercase hexadecimal digits.' }
    if ($Seconds -lt 5 -or $Seconds -gt 120) { throw 'Remote command timeout must be 5..120 seconds.' }
    $connection = Read-SunnyConnection $ConnectionPath
    $manifestPath = Join-Path $ArtifactRoot 'release.json'
    if ((Get-Item -LiteralPath $manifestPath).Length -gt 1048576) { throw 'Release manifest exceeds 1 MiB.' }
    $null = Read-SunnyRelease $ArtifactRoot $Hash
    $manifestBytes = [IO.File]::ReadAllBytes($manifestPath)
    if ((Get-SunnyBytesHash $manifestBytes) -cne $Hash) { throw 'Release manifest changed after verification.' }
    $manifest = ConvertTo-SunnyMap ([Text.UTF8Encoding]::new($false, $true).GetString($manifestBytes) | ConvertFrom-Json)
    $installerSource = Read-SunnyOperatorSource $ArtifactRoot $manifest 'installer/windows/Sunny.ps1'
    $hostSource = Read-SunnyOperatorSource $ArtifactRoot $manifest 'installer/windows/SunnyHost.ps1'
    if ($Operation -eq 'Plan') {
        return [ordered]@{schema_version=1;request_id=$Correlation;operation='plan';host=$connection.host;user=$connection.user;port=$connection.port;
            manifest_sha256=$Hash;remote_action_executed=$false;security_settings_changed=$false;
            next_step='Register required SSH access manually, verify host identity, then choose an explicit scoped action. Transfer the verified release before Install; Live shutdown and Control Surface selection stay manual.'}
    }
    if ($Operation -notin @('Info', 'Log') -and -not $Library) { throw 'Supply the exact remote user Library path.' }
    if ($Operation -eq 'Install' -and -not $RemoteArtifact) { throw 'Supply the already transferred complete verified remote release directory.' }
    $inputData = [ordered]@{operation=$Operation;request_id=$Correlation;library=$Library;release=$RemoteArtifact;
        manifest_hash=$Hash;doctor_report=$DoctorReportPath;log_path=$NativeLog;include_messages=$Messages}
    $payload = New-SunnyRemotePayload $Operation $inputData $installerSource $hostSource
    $starter = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes('[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); Invoke-Expression ([Console]::In.ReadToEnd())'))
    $ssh = (Get-Command ssh.exe -ErrorAction Stop).Source
    $arguments = @(Get-SunnySshArguments $connection) + @('powershell.exe', '-NoProfile', '-NonInteractive', '-EncodedCommand', $starter)
    $exchange = Invoke-SunnyBoundedProcess $ssh $arguments $payload $Seconds
    if ($exchange.exit_code -ne 0) {
        if ($exchange.stderr -match 'REMOTE HOST IDENTIFICATION HAS CHANGED|Host key verification failed') { throw 'SSH host identity was rejected; verify it manually before updating the separate known-host file.' }
        if ($exchange.stderr -match 'Permission denied') { throw 'SSH authentication was refused; approved access is required and no password will be requested.' }
        throw 'Remote operation failed or its outcome is unknown. Inspect Status and use Recover for a pending filesystem transaction before repeating a change.'
    }
    $report = ConvertTo-SunnyMap ($exchange.stdout | ConvertFrom-Json)
    Assert-SunnyRemoteReport $report $Operation $Correlation ($Seconds + 5)
    return $report
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        Invoke-SunnyRemote $Action $ConnectionFile $ReleaseDirectory $ExpectedManifestSHA256 $UserLibraryPath $RemoteReleaseDirectory $RemoteDoctorReport $LogPath ([bool]$IncludeMessages) $TimeoutSeconds $RequestId | ConvertTo-Json -Depth 30
        exit 0
    } catch { [Console]::Error.WriteLine('Sunny remote: ' + $_.Exception.Message); exit 1 }
}
