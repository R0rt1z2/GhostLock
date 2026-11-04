param(
  [string]$Tarball = "",
  [int]$Tries = 0
)

$ExploitName = "ghostlock_root"
$ExploitBuild = "build\ghostlock_root"
$RemoteDir = "/data/local/tmp/gl"
$Log = "$RemoteDir/run.log"
$Clean = $RemoteDir
$CanExec = $true
$IsRooted = "grep -aq 'ROOT] uid=0 daemon' $Log"
$IsReady = "grep -aq 'ota] done' $Log"
$HasFailed = "grep -aqE 'preloaded read slot failed|holding reclaim' $Log"
$RootShell = "su || $RemoteDir/su || $RemoteDir/ghostlock_root --su"
$DefaultTries = 15
$DefaultSettle = 25
$RootTimeout = 120

$ErrorActionPreference = "Continue"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$PostWait = if ($env:POSTWAIT) { [int]$env:POSTWAIT } else { 30 }
$Settle = if ($env:SETTLE) { [int]$env:SETTLE } else { $DefaultSettle }
$ExecTimeout = if ($env:EXEC_TIMEOUT) { [int]$env:EXEC_TIMEOUT } else { 1800 }

function Show-Usage {
  if ($CanExec) { Write-Host "Usage: root.bat [SCRIPT.tar] [TRIES]" }
  else { Write-Host "Usage: root.bat [TRIES]" }
}

if ($Tarball -eq "/?") { Show-Usage; exit 0 }

function Resolve-Adb {
  if ($env:ADB) { return $env:ADB }
  if (Get-Command adb -CommandType Application -ErrorAction SilentlyContinue) { return "adb" }
  $bundled = Join-Path $Here "bin\windows\adb.exe"
  if (Test-Path $bundled) { return $bundled }
  return $null
}

$Adb = Resolve-Adb
$AdbOk = $false
if ($Adb) {
  try { & $Adb version *> $null; $AdbOk = ($LASTEXITCODE -eq 0) } catch { $AdbOk = $false }
}
if (-not $AdbOk) {
  Write-Host "adb not found or not runnable."
  Write-Host "Install Android platform-tools and add it to PATH, set ADB to the full path of adb.exe,"
  Write-Host "or put adb.exe together with AdbWinApi.dll and AdbWinUsbApi.dll in bin\windows next to this script."
  exit 1
}

$Bin = $env:EXPLOIT
if (-not $Bin) {
  foreach ($cand in @((Join-Path $Here "bin\$ExploitName"), (Join-Path $Here $ExploitBuild))) {
    if (Test-Path $cand -PathType Leaf) { $Bin = $cand; break }
  }
}
if (-not $Bin -or -not (Test-Path $Bin -PathType Leaf)) {
  $name = if ($Bin) { $Bin } else { $ExploitName }
  Write-Host ("{0} not found: build it first, or set EXPLOIT to the full path of {1}" -f $name, $ExploitName)
  exit 1
}

if ($Tarball -and -not (Test-Path $Tarball -PathType Leaf)) {
  if ($Tarball -match '^\d+$' -and $Tries -eq 0) {
    $Tries = [int]$Tarball
    $Tarball = ""
  } else {
    Show-Usage
    exit 1
  }
}
if ($Tarball) {
  if (-not $CanExec) {
    Write-Host ("{0}: running a script after root is not supported on this branch" -f $Tarball)
    exit 1
  }
  $Tarball = (Resolve-Path $Tarball).Path
}
if ($Tries -le 0) { $Tries = $DefaultTries }

$Sel = @()
$Serial = $env:ANDROID_SERIAL
if (-not $Serial) {
  $online = @(& $Adb devices 2>$null | ForEach-Object { if (($_ -replace "`r", "") -match '^(\S+)\s+device$') { $Matches[1] } })
  if ($online.Count -gt 1) {
    Write-Host "More than one device is connected; set ANDROID_SERIAL to the one to root"
    exit 1
  }
  if ($online.Count -eq 1) { $Serial = $online[0] }
}
if ($Serial) {
  $Sel = @("-s", $Serial)
  Write-Host "Using device $Serial"
}

function AdbText {
  $out = & $Adb @Sel @args 2>$null
  if ($null -eq $out) { return "" }
  return (($out -join "`n") -replace "`r", "").Trim()
}

function Adb-State { AdbText get-state }

function Disable-Wifi {
  $serial = AdbText get-serialno
  if ($serial -notmatch ':\d' -and $serial -notmatch '\._tcp$') {
    & $Adb @Sel shell svc wifi disable *> $null
  }
}

function Wait-Offline {
  for ($w = 0; $w -lt 30 -and (Adb-State) -eq "device"; $w++) { Start-Sleep -Seconds 1 }
}

function Wait-Online {
  for ($r = 0; (Adb-State) -ne "device"; $r++) {
    if ($Serial -match ':\d' -and $r % 5 -eq 0) {
      & $Adb disconnect $Serial *> $null
      & $Adb connect $Serial *> $null
    }
    Start-Sleep -Seconds 2
  }
}

function Wait-Boot {
  Wait-Online
  for ($n = 0; $n -lt 120; $n++) {
    if ((Adb-State) -ne "device") { Wait-Online }
    if ((AdbText shell getprop sys.boot_completed) -eq "1") { break }
    Start-Sleep -Seconds 2
  }
  Start-Sleep -Seconds $Settle
}

$script:ExecArg = ""
$script:RemoteScript = ""
function Stage {
  if ((Adb-State) -ne "device") { Write-Host "Device is not online"; return $false }
  & $Adb @Sel shell "rm -rf $Clean; mkdir -p $RemoteDir" *> $null
  if ($LASTEXITCODE -ne 0) { Write-Host "Could not prepare $RemoteDir"; return $false }
  & $Adb @Sel push $Bin "$RemoteDir/$ExploitName" *> $null
  if ($LASTEXITCODE -ne 0) { Write-Host "Could not push $ExploitName"; return $false }
  & $Adb @Sel shell "chmod 755 $RemoteDir/$ExploitName" *> $null
  if ($LASTEXITCODE -ne 0) { Write-Host "Could not chmod $ExploitName"; return $false }
  if (-not $Tarball) { return $true }

  $base = Split-Path -Leaf $Tarball
  & $Adb @Sel push $Tarball "$RemoteDir/$base" *> $null
  if ($LASTEXITCODE -ne 0) { Write-Host "Could not push $base"; return $false }
  & $Adb @Sel shell "cd $RemoteDir && { tar xf '$base' || gzip -dc '$base' | tar xf - || gunzip -c '$base' | tar xf -; } 2>/dev/null; true" *> $null
  $found = AdbText shell "find $RemoteDir -type f -name '*.sh' | LC_ALL=C sort | head -n 1"
  if (-not $found) {
    Write-Host "No .sh found inside $base; the device tar said:"
    (& $Adb @Sel shell "cd $RemoteDir && tar xf '$base'" 2>&1) | ForEach-Object { Write-Host "  $_" }
    return $false
  }
  & $Adb @Sel shell "chmod 755 '$found'" *> $null
  $script:RemoteScript = $found
  $script:ExecArg = "--exec $found"
  return $true
}

function Probe {
  $exited = if ($Tarball) { "grep -aq 'exited status=' $Log" } else { "false" }
  $out = AdbText shell "{ if $exited; then echo exited; elif $IsReady; then echo ready; elif $IsRooted; then echo rooted; elif $HasFailed; then echo failed; else echo running; fi; } 2>/dev/null"
  return ($out -split "`n")[-1].Trim()
}

$script:Shown = 0
function Show-Log {
  $lines = @(& $Adb @Sel shell "tail -n +$($script:Shown + 1) $Log 2>/dev/null" 2>$null)
  foreach ($line in $lines) { Write-Host ($line -replace "`r", "") }
  $script:Shown += $lines.Count
}

for ($i = 1; $i -le $Tries; $i++) {
  Write-Host "Attempt $i of $Tries"

  if ((Adb-State) -eq "device") {
    & $Adb @Sel reboot *> $null
    Wait-Offline
  }
  Wait-Boot
  Disable-Wifi
  if (-not (Stage)) {
    Write-Host "Attempt $i failed"
    continue
  }

  & $Adb @Sel shell "cd $RemoteDir && (nohup ./$ExploitName $script:ExecArg >$Log 2>&1 &); sleep 1" *> $null
  $script:Shown = 0

  $result = "timeout"
  $rooted = $false
  $limit = $RootTimeout
  $waited = 0
  $downs = 0
  while ($waited -lt $limit) {
    Start-Sleep -Seconds 1
    $waited++
    if ((Adb-State) -ne "device") {
      $downs++
      if ($downs -ge 3) { $result = "offline"; break }
      continue
    }
    $downs = 0
    Show-Log
    $state = Probe
    if ($state -eq "exited" -or $state -eq "failed") { $result = $state; break }
    if ($state -eq "ready" -or $state -eq "rooted") {
      if (-not $rooted) {
        $rooted = $true
        $waited = 0
        $limit = if ($Tarball) { $ExecTimeout } else { $PostWait }
      }
      if ($state -eq "ready" -and -not $Tarball) { $result = "ready"; break }
    }
  }
  if ($result -ne "offline") { Show-Log }

  $scriptName = if ($script:RemoteScript) { Split-Path -Leaf $script:RemoteScript } else { "" }
  if ($result -eq "exited") {
    $status = AdbText shell "grep -a 'exited status=' $Log | tail -n 1 | sed 's/.*status=//' | tr -dc '0-9-'"
    $shownStatus = if ($status) { $status } else { "unknown" }
    Write-Host ("{0} exited with status {1}" -f $scriptName, $shownStatus)
    if ($status -match '^\d+$') { exit ([int]$status) } else { exit 1 }
  }

  if ($rooted -and $Tarball) {
    if ($result -eq "offline") {
      Write-Host ("Rooted, but the device went offline before {0} finished; see {1}" -f $scriptName, $Log)
    } else {
      Write-Host ("Rooted, but {0} did not finish within {1}s; see {2}" -f $scriptName, $ExecTimeout, $Log)
    }
    exit 1
  }

  if ($rooted -and $result -ne "offline") {
    & $Adb @Sel shell -t $RootShell
    exit $LASTEXITCODE
  }

  switch ($result) {
    "failed" { Write-Host "Attempt $i failed: the exploit gave up" }
    "offline" { Write-Host "Attempt $i failed: the device went offline" }
    default { Write-Host "Attempt $i failed: timed out" }
  }
}

Write-Host "Failed after $Tries attempts"
exit 1
