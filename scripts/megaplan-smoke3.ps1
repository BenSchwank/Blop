# Megaplan Alltagspfad smoke3 — BlockInput, larger window, Erstellen via pixel hunt
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS3 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern bool BlockInput(bool fBlockIt);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public const uint LEFTDOWN=2, LEFTUP=4; public const uint KEYUP=2;
  public static void Click(int x,int y){
    SetCursorPos(x,y); System.Threading.Thread.Sleep(80);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(60);
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void DblClick(int x,int y){ Click(x,y); System.Threading.Thread.Sleep(160); Click(x,y); }
  public static void Drag(int x1,int y1,int x2,int y2){
    SetCursorPos(x1,y1); System.Threading.Thread.Sleep(60);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    for(int i=1;i<=14;i++){ SetCursorPos(x1+(x2-x1)*i/14, y1+(y2-y1)*i/14); System.Threading.Thread.Sleep(28); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Key(byte vk){ keybd_event(vk,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(45); keybd_event(vk,0,KEYUP,UIntPtr.Zero);}
  public static void TypeUnicode(string s){
    foreach(char c in s){
      short vk = VkKeyScan(c);
      byte lo = (byte)(vk & 0xff);
      bool shift = (vk & 0x100) != 0;
      if(shift) keybd_event(0x10,0,0,UIntPtr.Zero);
      keybd_event(lo,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(25);
      keybd_event(lo,0,KEYUP,UIntPtr.Zero);
      if(shift) keybd_event(0x10,0,KEYUP,UIntPtr.Zero);
      System.Threading.Thread.Sleep(20);
    }
  }
  [DllImport("user32.dll")] public static extern short VkKeyScan(char ch);
}
'@

$exe = 'c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Git\Blop\build-check\Blop.exe'
$outDir = Join-Path $env:TEMP 'blop-megaplan-smoke3'
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Get-Process Blop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
Start-Process $exe

function Get-BlopHwnd {
  $p = Get-Process Blop -ErrorAction SilentlyContinue | Where-Object {
    $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like '*Blop*'
  } | Select-Object -First 1
  if ($p) { return $p.MainWindowHandle }
  return [IntPtr]::Zero
}

$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 60; $i++) {
  Start-Sleep -Milliseconds 200
  $hwnd = Get-BlopHwnd
  if ($hwnd -eq [IntPtr]::Zero) { continue }
  $r = New-Object WinS3+RECT
  [void][WinS3]::GetWindowRect($hwnd, [ref]$r)
  if (($r.Right - $r.Left) -gt 500) { break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'Blop window missing' }

[void][WinS3]::ShowWindow($hwnd, 9)
[void][WinS3]::MoveWindow($hwnd, 20, 20, 1600, 1000, $true)
Start-Sleep -Milliseconds 700
[void][WinS3]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 400

# Block mouse/keyboard so user motion can't steal clicks
try { [void][WinS3]::BlockInput($true) } catch {}

function Cap([string]$name) {
  $script:hwnd = Get-BlopHwnd
  if ($script:hwnd -eq [IntPtr]::Zero) { Write-Output "$name SKIP no-hwnd"; return $false }
  [void][WinS3]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 100
  $r = New-Object WinS3+RECT
  [void][WinS3]::GetWindowRect($script:hwnd, [ref]$r)
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  if ($w -lt 200) { Write-Output "$name SKIP size"; return $false }
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $g.Dispose()
  $path = Join-Path $outDir $name
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output "$name ok ${w}x${h}"
  return $true
}
function Rel([int]$rx, [int]$ry) {
  $script:hwnd = Get-BlopHwnd
  $r = New-Object WinS3+RECT
  [void][WinS3]::GetWindowRect($script:hwnd, [ref]$r)
  @{ x = $r.Left + $rx; y = $r.Top + $ry }
}
function ClickRel([int]$rx, [int]$ry) {
  $pt = Rel $rx $ry
  [WinS3]::Click($pt.x, $pt.y)
}
function FindBlueButtonCenter([int]$x0, [int]$y0, [int]$x1, [int]$y1) {
  $script:hwnd = Get-BlopHwnd
  $r = New-Object WinS3+RECT
  [void][WinS3]::GetWindowRect($script:hwnd, [ref]$r)
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $g.Dispose()
  $sx = [Math]::Max(0, $x0); $sy = [Math]::Max(0, $y0)
  $ex = [Math]::Min($w - 1, $x1); $ey = [Math]::Min($h - 1, $y1)
  $bestX = -1; $bestY = -1; $best = 0
  for ($y = $sy; $y -le $ey; $y += 3) {
    for ($x = $sx; $x -le $ex; $x += 3) {
      $c = $bmp.GetPixel($x, $y)
      # primary blue ~ #5B9DFF / saturated blues
      if ($c.B -gt 180 -and $c.G -gt 120 -and $c.G -lt 220 -and $c.R -lt 140 -and ($c.B - $c.R) -gt 60) {
        $score = $c.B - $c.R
        if ($score -gt $best) { $best = $score; $bestX = $x; $bestY = $y }
      }
    }
  }
  $bmp.Dispose()
  if ($bestX -lt 0) { return $null }
  @{ x = $r.Left + $bestX; y = $r.Top + $bestY; rx = $bestX; ry = $bestY }
}

try {
  Cap '01_startup.png'
  [WinS3]::Key(0x1B); Start-Sleep -Milliseconds 250
  Cap '02_dismiss.png'

  # Notes rail icon (document) — below Home, ~48dp rail centered ~x24
  ClickRel 28 175
  Start-Sleep -Milliseconds 900
  Cap '03_notes.png'

  # Ensure Bibliothek selected (left nav)
  ClickRel 160 250
  Start-Sleep -Milliseconds 500
  Cap '04_library.png'

  # New note: + next to Notizen header in content area OR rail
  # Content-area plus near top-right of notes header region
  ClickRel 520 95
  Start-Sleep -Milliseconds 400
  # Also try sidebar + near ORDNER/Notizen header
  ClickRel 330 95
  Start-Sleep -Milliseconds 900
  Cap '05_after_plus.png'

  # If dialog pick cards visible: click DIN A4 card (middle of three)
  # Pick stage: cards roughly center of modal
  ClickRel 800 420
  Start-Sleep -Milliseconds 800
  Cap '06_pick_or_composer.png'

  # Composer: click DIN A4 segment (middle chip)
  ClickRel 720 250
  Start-Sleep -Milliseconds 400
  # Kariert template (~3rd tile)
  ClickRel 780 430
  Start-Sleep -Milliseconds 400
  Cap '07_a4_kariert.png'

  # Title field
  ClickRel 700 320
  Start-Sleep -Milliseconds 200
  # Select-all + type
  [WinS3]::Key(0x11) # ctrl down via keybd - better: Ctrl+A
  # Ctrl+A
  [WinS3]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
  [WinS3]::Key(0x41)
  [WinS3]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 80
  [WinS3]::TypeUnicode('Megaplan A4')
  Start-Sleep -Milliseconds 200
  Cap '08_titled.png'

  # Hunt Erstellen (blue primary) in lower-right of dialog card
  $btn = FindBlueButtonCenter 900 700 1550 980
  if (-not $btn) { $btn = FindBlueButtonCenter 700 650 1550 990 }
  if ($btn) {
    Write-Output ("Erstellen @ rel {0},{1}" -f $btn.rx, $btn.ry)
    [WinS3]::Click($btn.x, $btn.y)
  } else {
    Write-Output 'Erstellen pixel miss — fallback click'
    ClickRel 1180 920
  }
  Start-Sleep -Seconds 2.2
  Cap '09_after_create.png'

  # If still on dialog, try Enter then another click
  $btn2 = FindBlueButtonCenter 900 700 1550 980
  if ($btn2) {
    Write-Output 'still dialog — retry Erstellen'
    [WinS3]::Click($btn2.x, $btn2.y)
    Start-Sleep -Seconds 2
    Cap '09b_retry_create.png'
  }

  # Write on page: drag a stroke in editor area
  $p1 = Rel 700 450; $p2 = Rel 950 520
  [WinS3]::Drag($p1.x, $p1.y, $p2.x, $p2.y)
  Start-Sleep -Milliseconds 400
  Cap '10_write.png'

  # Back / home to library — click rail Home then Notes
  ClickRel 28 120
  Start-Sleep -Milliseconds 900
  Cap '11_home.png'

  ClickRel 28 175
  Start-Sleep -Milliseconds 900
  Cap '12_library_after.png'

  # Dashboard Anpassen — Home first
  ClickRel 28 120
  Start-Sleep -Milliseconds 1000
  Cap '13_dashboard.png'

  # Anpassen on right rail
  $an = FindBlueButtonCenter 1400 40 1580 200
  if (-not $an) {
    # text button often muted white — click typical Anpassen spot
    ClickRel 1500 70
  } else {
    [WinS3]::Click($an.x, $an.y)
  }
  Start-Sleep -Milliseconds 700
  Cap '14_anpassen.png'

  # Drag a widget a bit
  $d1 = Rel 500 350; $d2 = Rel 620 400
  [WinS3]::Drag($d1.x, $d1.y, $d2.x, $d2.y)
  Start-Sleep -Milliseconds 400
  Cap '15_moved.png'

  # Fertig
  ClickRel 1500 70
  Start-Sleep -Milliseconds 600
  Cap '16_fertig.png'

  # Settings via bottom grid / theme — open settings rail if present
  ClickRel 28 860
  Start-Sleep -Milliseconds 800
  Cap '17_settings_or_menu.png'

  Write-Output "DONE out=$outDir"
}
finally {
  try { [void][WinS3]::BlockInput($false) } catch {}
}
