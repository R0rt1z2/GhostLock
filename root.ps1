[CmdletBinding()]
param(
  [string]$Tarball = "",
  [int]$Tries = 15
)

$ErrorActionPreference = "Continue"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Resolve-Adb {
  if ($env:ADB) { return $env:ADB }
  if (Get-Command adb -CommandType Application -ErrorAction SilentlyContinue) { return "adb" }
  $bundled = Join-Path $Here "bin\windows\adb.exe"
  if (Test-Path $bundled) { return $bundled }
  return $null
}

$Adb = Resolve-Adb
if (-not $Adb) {
  Write-Host "adb not found."
  Write-Host "Install Android platform-tools and add it to PATH, or put adb.exe together with"
  Write-Host "AdbWinApi.dll and AdbWinUsbApi.dll in bin\windows next to this script."
  exit 1
}
$Remote = "/data/local/tmp/gl"
$Log = "$Remote/run.log"
$Settle = if ($env:SETTLE) { [int]$env:SETTLE } else { 25 }
$PostWait = if ($env:POSTWAIT) { [int]$env:POSTWAIT } else { 30 }

$script:RemoteScript = ""
$script:ExecArg = ""

function AdbText {
  $out = & $Adb @args 2>$null
  if ($null -eq $out) { return "" }
  return (($out -join "`n") -replace "`r", "").Trim()
}

$Bin = $null
foreach ($cand in @((Join-Path $Here "ghostlock_root"), (Join-Path $Here "build\ghostlock_root"))) {
  if (Test-Path $cand) { $Bin = $cand; break }
}
if (-not $Bin) { Write-Host "ghostlock_root not found next to this script"; exit 1 }

if ($Tarball) {
  if (-not (Test-Path $Tarball)) { Write-Host "tarball not found: $Tarball"; exit 1 }
  $Tarball = (Resolve-Path $Tarball).Path
  $RemoteTar = "$Remote/" + (Split-Path -Leaf $Tarball)
}

function Adb-State { AdbText get-state }

function Disable-Wifi {
  $serial = AdbText get-serialno
  if ($serial -notmatch ':\d' -and $serial -notmatch '_tcp') {
    & $Adb shell svc wifi disable *> $null
  }
}

function Wait-Boot {
  & $Adb wait-for-device 2>$null | Out-Null
  for ($i = 0; $i -lt 120; $i++) {
    if ((AdbText shell getprop sys.boot_completed) -eq "1") { break }
    Start-Sleep -Seconds 2
  }
  Start-Sleep -Seconds $Settle
}

function Log-Has([string]$pattern, [bool]$ext = $false) {
  $flag = if ($ext) { "-aqE" } else { "-aq" }
  $out = & $Adb shell "grep $flag '$pattern' $Log && echo GL_HIT" 2>$null
  return (($out -join "`n") -match "GL_HIT")
}

function Stage {
  if ((Adb-State) -ne "device") { Write-Host "  [stage] device not online"; return $false }
  & $Adb shell "rm -rf $Remote; mkdir -p $Remote" *> $null; if ($LASTEXITCODE -ne 0) { Write-Host "  [stage] mkdir failed"; return $false }
  & $Adb push $Bin "$Remote/ghostlock_root" *> $null; if ($LASTEXITCODE -ne 0) { Write-Host "  [stage] push binary failed"; return $false }
  & $Adb shell "chmod 755 $Remote/ghostlock_root" *> $null; if ($LASTEXITCODE -ne 0) { Write-Host "  [stage] chmod failed"; return $false }
  if ($Tarball) {
    $base = Split-Path -Leaf $Tarball
    & $Adb push $Tarball $RemoteTar *> $null; if ($LASTEXITCODE -ne 0) { Write-Host "  [stage] push $base failed"; return $false }
    & $Adb shell "cd $Remote && { tar xf '$base' || gzip -dc '$base' | tar xf - || gunzip -c '$base' | tar xf -; } 2>/dev/null; true" *> $null
    $found = AdbText shell "find $Remote -type f -name '*.sh' | LC_ALL=C sort | head -1"
    if (-not $found) {
      Write-Host "  [stage] extract produced no .sh inside $base; device tar said:"
      (& $Adb shell "cd $Remote && tar xf '$base'" 2>&1) | ForEach-Object { Write-Host "    $_" }
      return $false
    }
    $script:RemoteScript = $found
    & $Adb shell "chmod 755 '$found'" *> $null
    $script:ExecArg = "--exec $found"
  }
  return $true
}

for ($i = 1; $i -le $Tries; $i++) {
  Write-Host "Attempt $i/$Tries"
  if ((Adb-State) -eq "device") { & $Adb reboot *> $null }
  Wait-Boot
  Disable-Wifi
  if (-not (Stage)) { continue }

  & $Adb shell "cd $Remote && (nohup ./ghostlock_root $script:ExecArg >run.log 2>&1 &); sleep 1" *> $null

  $result = "timeout"
  $rootedSeen = $false
  $postn = 0
  $shown = 0
  $downs = 0
  $maxn = if ($Tarball) { 1800 } else { 120 }
  for ($n = 0; $n -lt $maxn; $n++) {
    Start-Sleep -Seconds 1

    $lines = @(& $Adb shell "cat $Log 2>/dev/null")
    if ($lines.Count -gt $shown) {
      for ($k = $shown; $k -lt $lines.Count; $k++) { Write-Host ($lines[$k] -replace "`r", "") }
      $shown = $lines.Count
    }

    if ($Tarball -and (Log-Has 'exited status=')) { $result = "execdone"; break }
    if (-not $rootedSeen -and (Log-Has 'ROOT] uid=0 daemon')) {
      $rootedSeen = $true
      $result = "rooted"
    }
    if ($rootedSeen -and -not $Tarball) {
      $postn++
      if ($postn -ge $PostWait -or (Log-Has 'ota] done')) { break }
    }
    if (-not $rootedSeen -and (Log-Has 'preloaded read slot failed|holding reclaim' $true)) { $result = "failed"; break }
    if ((Adb-State) -ne "device") {
      $downs++
      if ($downs -ge 3) { $result = "down"; break }
    } else {
      $downs = 0
    }
  }

  if ($result -eq "execdone") {
    $status = AdbText shell "grep -a 'exited status=' $Log | tail -1 | sed 's/.*status=//' | tr -dc '0-9-'"
    if (-not $status) { $status = "?" }
    if ($status -match '^-?\d+$') { exit [int]$status } else { exit 0 }
  }

  if ($result -eq "rooted") {
    if ($Tarball) {
      Write-Host ("Rooted, but {0} did not report completion within the wait window; see {1}." -f (Split-Path -Leaf $script:RemoteScript), $Log)
      exit 1
    }
    & $Adb shell -t "su || $Remote/su || $Remote/ghostlock_root --su"
    exit 0
  }
}

exit 1
