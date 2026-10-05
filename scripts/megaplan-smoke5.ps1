# Smoke5: rail "new" → DIN A4 → Erstellen → write → home Anpassen
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS5 {
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
    SetCursorPos(x,y); System.Threading.Thread.Sleep(100);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(80);
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void DblClick(int x,int y){ Click(x,y); System.Threading.Thread.Sleep(180); Click(x,y); }
  public static void Drag(int x1,int y1,int x2,int y2){
    SetCursorPos(x1,y1); System.Threading.Thread.Sleep(50);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    for(int i=1;i<=16;i++){ SetCursorPos(x1+(x2-x1)*i/16, y1+(y2-y1)*i/16); System.Threading.Thread.Sleep(25); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Key(byte vk){ keybd_event(vk,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(40); keybd_event(vk,0,KEYUP,UIntPtr.Zero);}
}
'@

$exe = 'c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Git\Blop\build-check\Blop.exe'
$outDir = Join-Path $env:TEMP 'blop-megaplan-smoke5'
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Get-Process Blop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 900
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
  $r = New-Object WinS5+RECT
  [void][WinS5]::GetWindowRect($hwnd, [ref]$r)
  if (($r.Right - $r.Left) -gt 500) { break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'Blop window missing' }

[void][WinS5]::ShowWindow($hwnd, 9)
[void][WinS5]::MoveWindow($hwnd, 20, 20, 1600, 1000, $true)
Start-Sleep -Milliseconds 900
[void][WinS5]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 500

try { [void][WinS5]::BlockInput($true) } catch {}

function Cap([string]$name) {
  $script:hwnd = Get-BlopHwnd
  [void][WinS5]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 80
  $r = New-Object WinS5+RECT
  [void][WinS5]::GetWindowRect($script:hwnd, [ref]$r)
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $g.Dispose()
  $bmp.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output "$name ok ${w}x${h}"
}
function Rel([int]$rx, [int]$ry) {
  $script:hwnd = Get-BlopHwnd
  $r = New-Object WinS5+RECT
  [void][WinS5]::GetWindowRect($script:hwnd, [ref]$r)
  @{ x = $r.Left + $rx; y = $r.Top + $ry }
}
function ClickRel([int]$rx, [int]$ry) {
  $pt = Rel $rx $ry; [WinS5]::Click($pt.x, $pt.y)
}
function FindDenseBlue([int]$x0,[int]$y0,[int]$x1,[int]$y1,[int]$minScore=10) {
  $script:hwnd = Get-BlopHwnd
  $r = New-Object WinS5+RECT
  [void][WinS5]::GetWindowRect($script:hwnd, [ref]$r)
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $g.Dispose()
  $bestScore=0; $bestX=-1; $bestY=-1
  $sx=[Math]::Max(0,$x0); $sy=[Math]::Max(0,$y0)
  $ex=[Math]::Min($w-1,$x1); $ey=[Math]::Min($h-1,$y1)
  for ($y=$sy; $y -le $ey; $y+=3) {
    for ($x=$sx; $x -le $ex; $x+=3) {
      $c=$bmp.GetPixel($x,$y)
      if ($c.B -gt 185 -and $c.R -lt 130 -and $c.G -gt 120 -and $c.G -lt 220 -and ($c.B-$c.R) -gt 70) {
        $n=0
        for ($dy=-8; $dy -le 8; $dy+=2) {
          for ($dx=-14; $dx -le 14; $dx+=2) {
            $xx=$x+$dx; $yy=$y+$dy
            if ($xx -lt 0 -or $yy -lt 0 -or $xx -ge $w -or $yy -ge $h) { continue }
            $c2=$bmp.GetPixel($xx,$yy)
            if ($c2.B -gt 175 -and $c2.R -lt 140 -and ($c2.B-$c2.R) -gt 60) { $n++ }
          }
        }
        if ($n -gt $bestScore) { $bestScore=$n; $bestX=$x; $bestY=$y }
      }
    }
  }
  $bmp.Dispose()
  if ($bestX -lt 0 -or $bestScore -lt $minScore) { return $null }
  @{ x=$r.Left+$bestX; y=$r.Top+$bestY; rx=$bestX; ry=$bestY; score=$bestScore }
}

try {
  Cap '01_startup.png'
  [WinS5]::Key(0x1B); Start-Sleep -Milliseconds 250

  # Home (dashboard)
  ClickRel 28 95
  Start-Sleep -Milliseconds 1000
  Cap '02_home.png'

  # Library notes
  ClickRel 28 135
  Start-Sleep -Milliseconds 900
  Cap '03_library.png'

  # Rail "Neue Notiz" (compose) — third icon after home+library
  ClickRel 28 175
  Start-Sleep -Milliseconds 1200
  Cap '04_new_dialog.png'

  # DIN A4 = middle format card in pick row
  # Cards span modal; middle ~ window center + a bit right of Unendlich
  ClickRel 900 470
  Start-Sleep -Milliseconds 1100
  Cap '05_composer.png'

  # If still on pick (didn't expand), try A4 card again more centered
  $btnProbe = FindDenseBlue 1000 700 1500 980 8
  if (-not $btnProbe) {
    ClickRel 850 500
    Start-Sleep -Milliseconds 1000
    Cap '05b_composer.png'
  }

  # Kariert template (3rd chip in composer)
  ClickRel 860 430
  Start-Sleep -Milliseconds 300

  # Erstellen in modal footer (lower right of card, not Alle chip)
  $btn = FindDenseBlue 950 620 1520 970 8
  if (-not $btn) { $btn = FindDenseBlue 800 580 1550 990 6 }
  if ($btn) {
    Write-Output ("Erstellen dens={0} @ {1},{2}" -f $btn.score,$btn.rx,$btn.ry)
    [WinS5]::Click($btn.x, $btn.y)
  } else {
    Write-Output 'Erstellen fallback'
    # Primary button is rightmost in footer — modal ~680dp wide centered
    ClickRel 1100 860
    Start-Sleep -Milliseconds 200
    ClickRel 1180 900
  }
  Start-Sleep -Seconds 2.8
  Cap '06_editor.png'

  # Retry if Erstellen still visible
  $still = FindDenseBlue 1000 650 1520 970 15
  if ($still) {
    Write-Output ("retry Erstellen dens={0}" -f $still.score)
    [WinS5]::Click($still.x, $still.y)
    Start-Sleep -Seconds 2.5
    Cap '06b_editor.png'
  }

  # Write stroke
  $a = Rel 700 420; $b = Rel 1000 560
  [WinS5]::Drag($a.x,$a.y,$b.x,$b.y)
  Start-Sleep -Milliseconds 500
  Cap '07_write.png'

  # Back to library via rail library
  ClickRel 28 135
  Start-Sleep -Milliseconds 1200
  Cap '08_library_after.png'

  # Dashboard
  ClickRel 28 95
  Start-Sleep -Milliseconds 1200
  Cap '09_dashboard.png'

  # Anpassen (right rail top)
  ClickRel 1520 75
  Start-Sleep -Milliseconds 800
  Cap '10_anpassen.png'

  $d1 = Rel 500 350; $d2 = Rel 650 430
  [WinS5]::Drag($d1.x,$d1.y,$d2.x,$d2.y)
  Start-Sleep -Milliseconds 400
  Cap '11_moved.png'

  ClickRel 1520 75
  Start-Sleep -Milliseconds 700
  Cap '12_fertig.png'

  Write-Output "DONE $outDir"
}
finally {
  try { [void][WinS5]::BlockInput($false) } catch {}
}
