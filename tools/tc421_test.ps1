# ============================================================================
# TC421 Live Lamp Control Test (Unicast UDP -> 192.168.4.1:5000)
# ============================================================================
$logFile = Join-Path $PSScriptRoot "tc421_test_log.txt"
$sb = New-Object System.Text.StringBuilder

function Log($msg) {
    $line = "[$(Get-Date -Format 'HH:mm:ss')] $msg"
    Write-Host $line
    [void]$sb.AppendLine($line)
}

function Add-WifiFooter([byte[]]$pkt, [string]$mac) {
    # ProtocolWifi moves checksum from [63] to [61], clears [63], puts 12-char MAC at [49..60]
    $out = New-Object byte[] 64
    [Array]::Copy($pkt, $out, 64)
    $out[61] = $out[63]
    $out[63] = 0
    if ($mac -and $mac.Length -eq 12) {
        $macBytes = [Text.Encoding]::ASCII.GetBytes($mac)
        [Array]::Copy($macBytes, 0, $out, 49, 12)
    }
    return $out
}

function Make-ReadyPlay([string]$name) {
    $pkt = New-Object byte[] 64
    $nameBytes = [Text.Encoding]::ASCII.GetBytes($name)
    $pkt[0] = 85; $pkt[1] = 170; $pkt[2] = 6; $pkt[3] = 1; $pkt[4] = 0
    $pkt[5] = [byte]$nameBytes.Length; $pkt[6] = 0
    [Array]::Copy($nameBytes, 0, $pkt, 7, $nameBytes.Length)
    $sum = 0
    for ($i = 0; $i -lt (7 + $nameBytes.Length); $i++) { $sum += $pkt[$i] }
    $pkt[63] = [byte]($sum -band 0xFF)
    return $pkt
}

function Make-PlayValues([byte]$c1, [byte]$c2, [byte]$c3, [byte]$c4, [byte]$c5) {
    $pkt = New-Object byte[] 64
    $pkt[0] = 85; $pkt[1] = 170; $pkt[2] = 6; $pkt[3] = 2; $pkt[4] = 0; $pkt[5] = 5
    $pkt[6] = $c1; $pkt[7] = $c2; $pkt[8] = $c3; $pkt[9] = $c4; $pkt[10] = $c5
    $sum = 0
    for ($i = 0; $i -lt 11; $i++) { $sum += $pkt[$i] }
    $pkt[63] = [byte]($sum -band 0xFF)
    return $pkt
}

function Make-EndTx() {
    $pkt = New-Object byte[] 64
    $pkt[0] = 85; $pkt[1] = 170; $pkt[2] = 3; $pkt[3] = 8
    $pkt[63] = 10
    return $pkt
}

Log "=== TC421 Live Control Test Started ==="
$target = "192.168.4.1"
$macAp  = "4C9D24539874"
$macSta = "489D24539874"

$udp = New-Object System.Net.Sockets.UdpClient
$udp.Connect($target, 5000)
$udp.Client.ReceiveTimeout = 1000

function Send-And-Recv([string]$label, [byte[]]$pkt) {
    [void]$udp.Send($pkt, $pkt.Length)
    Log "SENT [$label]: $((($pkt[0..15] | ForEach-Object { $_.ToString('X2') }) -join ' ')) ... $((($pkt[49..63] | ForEach-Object { $_.ToString('X2') }) -join ' '))"
    try {
        $rem = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
        $r = $udp.Receive([ref]$rem)
        $hex = ($r | ForEach-Object { $_.ToString("X2") }) -join " "
        Log "RECV [$label] ($($r.Length)B): $hex"
        return $r
    } catch {
        Log "RECV [$label]: (no reply)"
        return $null
    }
}

# 1. SearchDevice unicast
$searchPkt = New-Object byte[] 64
$searchPkt[0]=85; $searchPkt[1]=170; $searchPkt[2]=23; $searchPkt[5]=3
$searchPkt[6]=221; $searchPkt[7]=238; $searchPkt[8]=255; $searchPkt[61]=227
$sr = Send-And-Recv "SearchDevice" $searchPkt

$useMac = $macSta
if ($sr -and $sr.Length -ge 16 -and $sr[0] -eq 136) {
    $useMac = [Text.Encoding]::ASCII.GetString($sr, 4, 12)
    Log ">>> SearchDevice replied! Using MAC=$useMac <<<"
} else {
    Log "No SearchDevice reply, testing with AP MAC ($macAp) and STA MAC ($macSta)"
}

# 2. Test Live Brightness with WiFi-framed packets (MAC = $useMac)
Write-Host ""
Write-Host ">>> WATCH THE LAMP! Setting 10% for 2s, then 80% for 2s... <<<" -ForegroundColor Cyan

Send-And-Recv "ReadyPlay($useMac)" (Add-WifiFooter (Make-ReadyPlay "LIVE") $useMac) | Out-Null
Start-Sleep -Milliseconds 150
Send-And-Recv "Play 10% ($useMac)" (Add-WifiFooter (Make-PlayValues 10 10 10 10 10) $useMac) | Out-Null
Start-Sleep -Seconds 2
Send-And-Recv "Play 80% ($useMac)" (Add-WifiFooter (Make-PlayValues 80 80 80 80 80) $useMac) | Out-Null
Start-Sleep -Seconds 2
Send-And-Recv "EndTx($useMac)" (Add-WifiFooter (Make-EndTx) $useMac) | Out-Null

# 3. Also test with AP MAC if different
if ($useMac -ne $macAp) {
    Start-Sleep -Milliseconds 500
    Write-Host ">>> Testing with AP MAC ($macAp): 10% -> 80%... <<<" -ForegroundColor Cyan
    Send-And-Recv "ReadyPlay($macAp)" (Add-WifiFooter (Make-ReadyPlay "LIVE") $macAp) | Out-Null
    Start-Sleep -Milliseconds 150
    Send-And-Recv "Play 10% ($macAp)" (Add-WifiFooter (Make-PlayValues 10 10 10 10 10) $macAp) | Out-Null
    Start-Sleep -Seconds 2
    Send-And-Recv "Play 80% ($macAp)" (Add-WifiFooter (Make-PlayValues 80 80 80 80 80) $macAp) | Out-Null
    Start-Sleep -Seconds 2
    Send-And-Recv "EndTx($macAp)" (Add-WifiFooter (Make-EndTx) $macAp) | Out-Null
}

$udp.Close()
Log "=== Live Test Finished ==="
[System.IO.File]::WriteAllText($logFile, $sb.ToString(), [System.Text.Encoding]::UTF8)
Write-Host ""
Write-Host "Done! Did the lamp change brightness? Switch Wi-Fi back and tell me!" -ForegroundColor Green
Pause
