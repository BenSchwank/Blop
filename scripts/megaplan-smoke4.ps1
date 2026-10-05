# Megaplan smoke4 — Dashboard "Neue Notiz" → DIN A4 → Erstellen → write → Anpassen
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS4 {
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
    SetCursorPos(x,y); System.Threading.Thread.Sleep(90);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(70);
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
$outDir = Join-Path $env:TEMP 'blop-megaplan-smoke4'
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
  $r = New-Object WinS4+RECT
  [void][WinS4]::GetWindowRect($hwnd, [ref]$r)
  if (($r.Right - $r.Left) -gt 500) { break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'Blop window missing' }

[void][WinS4]::ShowWindow($hwnd, 9)
[void][WinS4]::MoveWindow($hwnd, 20, 20, 1600, 1000, $true)
Start-Sleep -Milliseconds 800
[void][WinS4]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 500

try { [void][WinS4]::BlockInput($true) } catch {}

function Cap([string]$name) {
  $script:hwnd = Get-BlopHwnd
  if ($script:hwnd -eq [IntPtr]::Zero) { Write-Output "$name SKIP"; return $false }
  [void][WinS4]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 80
  $r = New-Object WinS4+RECT
  [void][WinS4]::GetWindowRect($script:hwnd, [ref]$r)
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $g.Dispose()
  $bmp.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output "$name ok ${w}x${h}"
  return $true
}
function Rel([int]$rx, [int]$ry) {
  $script:hwnd = Get-BlopHwnd
  $r = New-Object WinS4+RECT
  [void][WinS4]::GetWindowRect($script:hwnd, [ref]$r)
  @{ x = $r.Left + $rx; y = $r.Top + $ry }
}
function ClickRel([int]$rx, [int]$ry) {
  $pt = Rel $rx $ry; [WinS4]::Click($pt.x, $pt.y)
}
function GrabBmp {
  $script:hwnd = Get-BlopHwnd
  $r = New-Object WinS4+RECT
  [void][WinS4]::GetWindowRect($script:hwnd, [ref]$r)
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $g.Dispose()
  @{ bmp = $bmp; left = $r.Left; top = $r.Top; w = $w; h = $h }
}
function FindTextishBlueBtn([int]$x0,[int]$y0,[int]$x1,[int]$y1) {
  # Find dense blue primary button (Erstellen) — prefer lower-right clusters
  $g = GrabBmp
  $bmp = $g.bmp
  $bestScore = 0; $bestX = -1; $bestY = -1
  $sx=[Math]::Max(0,$x0); $sy=[Math]::Max(0,$y0)
  $ex=[Math]::Min($g.w-1,$x1); $ey=[Math]::Min($g.h-1,$y1)
  for ($y=$sy; $y -le $ey; $y+=4) {
    for ($x=$sx; $x -le $ex; $x+=4) {
      $c = $bmp.GetPixel($x,$y)
      if ($c.B -gt 190 -and $c.R -lt 120 -and $c.G -gt 130 -and $c.G -lt 210 -and ($c.B-$c.R) -gt 80) {
        # neighborhood density
        $n=0
        for ($dy=-6; $dy -le 6; $dy+=3) {
          for ($dx=-10; $dx -le 10; $dx+=3) {
            $xx=$x+$dx; $yy=$y+$dy
            if ($xx -lt 0 -or $yy -lt 0 -or $xx -ge $g.w -or $yy -ge $g.h) { continue }
            $c2=$bmp.GetPixel($xx,$yy)
            if ($c2.B -gt 180 -and $c2.R -lt 130 -and ($c2.B-$c2.R) -gt 70) { $n++ }
          }
        }
        if ($n -gt $bestScore) { $bestScore=$n; $bestX=$x; $bestY=$y }
      }
    }
  }
  $bmp.Dispose()
  if ($bestX -lt 0 -or $bestScore -lt 8) { return $null }
  @{ x = $g.left + $bestX; y = $g.top + $bestY; rx=$bestX; ry=$bestY; score=$bestScore }
}

try {
  Cap '01_startup.png'
  [WinS4]::Key(0x1B); Start-Sleep -Milliseconds 200

  # Ensure Home / Dashboard
  ClickRel 28 118
  Start-Sleep -Milliseconds 900
  Cap '02_dashboard.png'

  # "Neue Notiz" CTA under greeting (left of three chips)
  # From smoke3 01: buttons sit under "Guten Morgen" block
  ClickRel 160 620
  Start-Sleep -Milliseconds 1200
  Cap '03_after_neue.png'

  # If still dashboard, try slightly different y
  $stillDash = $false
  $chk = GrabBmp
  # crude: if greeting white text area still dominant left — try alt click
  $c = $chk.bmp.GetPixel(200, 520)
  $chk.bmp.Dispose()

  # Pick stage: three format cards. DIN A4 is middle card.
  # Modal is centered-ish; middle card ~ center of window
  ClickRel 800 480
  Start-Sleep -Milliseconds 1000
  Cap '04_after_a4_pick.png'

  # Composer: ensure DIN A4 segment selected (middle of Unendlich|DIN A4|Struktur)
  ClickRel 780 240
  Start-Sleep -Milliseconds 350
  # Kariert template
  ClickRel 820 420
  Start-Sleep -Milliseconds 350
  Cap '05_composer.png'

  # Erstellen — dense blue in lower-right of modal (avoid Alle chip at ~left)
  $btn = FindTextishBlueBtn 850 600 1500 980
  if (-not $btn) { $btn = FindTextishBlueBtn 700 550 1550 990 }
  if ($btn) {
    Write-Output ("Erstellen dens={0} @ {1},{2}" -f $btn.score, $btn.rx, $btn.ry)
    [WinS4]::Click($btn.x, $btn.y)
  } else {
    Write-Output 'Erstellen miss — click footer right'
    ClickRel 1200 880
  }
  Start-Sleep -Seconds 2.5
  Cap '06_editor.png'

  # If dialog still up (blue footer still dense), retry
  $btn2 = FindTextishBlueBtn 900 650 1500 980
  if ($btn2 -and $btn2.score -gt 12) {
    Write-Output 'retry Erstellen'
    [WinS4]::Click($btn2.x, $btn2.y)
    Start-Sleep -Seconds 2.5
    Cap '06b_editor.png'
  }

  # Draw stroke
  $a = Rel 650 400; $b = Rel 980 560
  [WinS4]::Drag($a.x, $a.y, $b.x, $b.y)
  Start-Sleep -Milliseconds 500
  Cap '07_write.png'

  # Escape / back to library via Notes rail
  [WinS4]::Key(0x1B)
  Start-Sleep -Milliseconds 400
  ClickRel 28 175
  Start-Sleep -Milliseconds 1000
  Cap '08_library.png'

  # Home dashboard Anpassen
  ClickRel 28 118
  Start-Sleep -Milliseconds 1100
  Cap '09_dashboard.png'

  # Anpassen — top-right of dash right rail
  ClickRel 1520 80
  Start-Sleep -Milliseconds 700
  Cap '10_anpassen.png'

  $d1 = Rel 480 340; $d2 = Rel 640 420
  [WinS4]::Drag($d1.x, $d1.y, $d2.x, $d2.y)
  Start-Sleep -Milliseconds 400
  Cap '11_moved.png'

  ClickRel 1520 80
  Start-Sleep -Milliseconds 600
  Cap '12_fertig.png'

  Write-Output "DONE $outDir"
}
finally {
  try { [void][WinS4]::BlockInput($false) } catch {}
}
