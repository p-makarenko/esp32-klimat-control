# ============================================================================
# Mic Quiet Watcher for Antigravity (agy.exe) -> ESP32 Klimat Kontrol
# Automatically mutes humidifier & all fans while chat microphone is active!
# ============================================================================
param(
    [string]$EspHost = "klimat.local",
    [int]$PollMs = 150,
    [int]$TtlSec = 120,
    [switch]$AnyApp
)

$regBase = "HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\microphone\NonPackaged"
$watchedApps = @("agy.exe", "TextInputHost.exe") # Antigravity + Windows Win+H dictation

$cachedIp = $null
$lastResolve = [DateTime]::MinValue

function Resolve-EspUrl {
    $now = Get-Date
    if (-not $script:cachedIp -or ($now - $script:lastResolve).TotalSeconds -gt 60) {
        try {
            $ips = [System.Net.Dns]::GetHostAddresses($EspHost) | Where-Object { $_.AddressFamily -eq 'InterNetwork' }
            if ($ips -and $ips.Count -gt 0) {
                $script:cachedIp = $ips[0].IPAddressToString
                $script:lastResolve = $now
            }
        } catch {}
    }
    if ($script:cachedIp) { return "http://$($script:cachedIp)/api/quiet" }
    return "http://$EspHost/api/quiet"
}

function Send-Quiet([bool]$enable) {
    $url = Resolve-EspUrl
    $qs = if ($enable) { "?on=1&ttl=$TtlSec" } else { "?on=0" }
    try {
        $resp = Invoke-WebRequest -Uri ($url + $qs) -TimeoutSec 2 -UseBasicParsing
        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] ESP32 Quiet=$($enable) -> $($resp.Content)" -ForegroundColor $(if ($enable) { "Yellow" } else { "Green" })
        return $true
    } catch {
        # Clear IP cache on error so next call re-resolves
        $script:cachedIp = $null
        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] ESP32 unreachable ($url): $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

function Is-MicActive {
    $items = Get-ChildItem $regBase -ErrorAction SilentlyContinue
    foreach ($item in $items) {
        $appName = ($item.PSChildName -split '#')[-1]
        if (-not $AnyApp) {
            if ($watchedApps -notcontains $appName) { continue }
        }
        $p = Get-ItemProperty $item.PSPath -ErrorAction SilentlyContinue
        if ($p -and $p.LastUsedTimeStart -gt 0 -and $p.LastUsedTimeStop -eq 0) {
            return $appName
        }
    }
    return $null
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Mic Quiet Watcher -> $EspHost (/api/quiet)" -ForegroundColor Cyan
Write-Host "  Watching: $(if ($AnyApp) { 'ALL apps' } else { $watchedApps -join ', ' })" -ForegroundColor Cyan
Write-Host "  Press Ctrl+C to stop (will auto-restore climate)." -ForegroundColor Gray
Write-Host "==========================================================" -ForegroundColor Cyan

# Pre-resolve IP on startup
$initUrl = Resolve-EspUrl
Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Target: $initUrl" -ForegroundColor Gray

$quietActive = $false
$lastKeepAlive = [DateTime]::MinValue
$micOffSince = $null

try {
    while ($true) {
        $activeApp = Is-MicActive
        $now = Get-Date

        if ($activeApp) {
            $micOffSince = $null
            if (-not $quietActive) {
                Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Mic ON ($activeApp) -> Muting room noise..." -ForegroundColor Yellow
                Send-Quiet $true | Out-Null
                $quietActive = $true
                $lastKeepAlive = $now
            } elseif (($now - $lastKeepAlive).TotalSeconds -ge 60) {
                # Keep-alive TTL while mic stays open
                Send-Quiet $true | Out-Null
                $lastKeepAlive = $now
            }
        } else {
            if ($quietActive) {
                if (-not $micOffSince) {
                    $micOffSince = $now
                } elseif (($now - $micOffSince).TotalMilliseconds -ge 500) {
                    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Mic OFF -> Restoring climate..." -ForegroundColor Green
                    Send-Quiet $false | Out-Null
                    $quietActive = $false
                    $micOffSince = $null
                }
            }
        }

        Start-Sleep -Milliseconds $PollMs
    }
} finally {
    if ($quietActive) {
        Send-Quiet $false | Out-Null
    }
}
