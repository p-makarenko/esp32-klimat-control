# ============================================================================
# Switch TC421 (Mylinks M0L1) -> STA mode ("KlimatLight", ESP32 AP 192.168.10.x)
# ============================================================================
$logFile = Join-Path $PSScriptRoot "tc421_sta_log.txt"
$sb = New-Object System.Text.StringBuilder

function Log($msg) {
    $line = "[$(Get-Date -Format 'HH:mm:ss')] $msg"
    Write-Host $line
    [void]$sb.AppendLine($line)
}

Log "=== Switching TC421 -> STA ('KlimatLight') ==="

$curIface = (netsh wlan show interfaces | Out-String)
$returnSsid = "Redmi Note 14"
if ($curIface -match "Profile\s*:\s*(.+)") {
    $returnSsid = $Matches[1].Trim()
}
Log "Will return PC Wi-Fi to: '$returnSsid'"

try {
    Log "Connecting PC to Smart-539874..."
    netsh wlan connect name="Smart-539874" | Out-Null

    $ready = $false
    for ($i = 0; $i -lt 12; $i++) {
        Start-Sleep -Seconds 1
        $st = curl.exe -s -m 2 -u "admin:admin" "http://192.168.4.1/system/status.php"
        if ($st -and $st.StartsWith("{")) {
            Log "Connected to TC421! Current status: $st"
            $ready = $true
            break
        }
    }

    if (-not $ready) {
        Log "ERROR: Could not reach 192.168.4.1"
    } else {
        $json = '{"apname":"Smart-539874","apchn":"1","apkey":"","apip":"192.168.4.1","apmask":"255.255.255.0","staname":"KlimatLight","stakey":"12345678","stadhcp":"1","staip":"0.0.0.0","stamask":"0.0.0.0","stagw":"0.0.0.0","type":"1","server":"192.168.10.1","port":"5000","timeout":"5000","baud":"115200","Nuser":"admin","Nkey":"admin","mode":"1","active":3}'
        $tmpJson = Join-Path $PSScriptRoot "set_payload.json"
        [System.IO.File]::WriteAllText($tmpJson, $json, (New-Object System.Text.UTF8Encoding($false)))

        Log "Sending POST /system/set.php -> KlimatLight..."
        $res = curl.exe -s -i -m 5 -u "admin:admin" -X POST -H "Expect:" -H "Content-Type: application/x-www-form-urlencoded; charset=UTF-8" --data-binary "@$tmpJson" "http://192.168.4.1/system/set.php"
        Log "POST response: $res"
    }
} finally {
    Log "Reconnecting PC Wi-Fi back to '$returnSsid'..."
    Start-Sleep -Seconds 2
    netsh wlan connect name="$returnSsid" | Out-Null

    for ($i = 0; $i -lt 15; $i++) {
        Start-Sleep -Seconds 1
        $iface = (netsh wlan show interfaces | Out-String)
        if ($iface -match [regex]::Escape($returnSsid) -and $iface -match "connected") {
            Log "Back on '$returnSsid'!"
            break
        }
    }
}

Log "Waiting 8s for TC421 to connect to KlimatLight..."
Start-Sleep -Seconds 8

Log "Triggering ESP32 discover: http://klimat.local/tc421/discover"
curl.exe -s -m 5 "http://klimat.local/tc421/discover" | Out-Null
$stJson = curl.exe -s -m 5 "http://klimat.local/tc421/api/status"
Log "ESP32 /tc421/api/status: $stJson"

$nets = (netsh wlan show networks | Select-String "SSID" | Out-String).Trim()
Log "Visible SSIDs:`n$nets"
Log "=== Done ==="
[System.IO.File]::WriteAllText($logFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
