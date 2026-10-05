# Smoke6: taller modal + Enter on Erstellen + DIN A4 segment
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS6 {
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
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(70);
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Drag(int x1,int y1,int x2,int y2){
    SetCursorPos(x1,y1); System.Threading.Thread.Sleep(40);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    for(int i=1;i<=14;i++){ SetCursorPos(x1+(x2-x1)*i/14, y1+(y2-y1)*i/14); System.Threading.Thread.Sleep(22); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Key(byte vk){ keybd_event(vk,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(40); keybd_event(vk,0,KEYUP,UIntPtr.Zero);}
}
'@

$exe = 'c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Git\Blop\build-check\Blop.exe'
$outDir = Join-Path $env:TEMP 'blop-megaplan-smoke6'
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
for ($i=0; $i -lt 60; $i++) {
  Start-Sleep -Milliseconds 200
  $hwnd = Get-BlopHwnd
  if ($hwnd -eq [IntPtr]::Zero) { continue }
  $r = New-Object WinS6+RECT
  [void][WinS6]::GetWindowRect($hwnd, [ref]$r)
  if (($r.Right-$r.Left) -gt 500) { break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'no hwnd' }
[void][WinS6]::ShowWindow($hwnd, 9)
[void][WinS6]::MoveWindow($hwnd, 20, 20, 1600, 1000, $true)
Start-Sleep -Milliseconds 900
[void][WinS6]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 400
try { [void][WinS6]::BlockInput($true) } catch {}

function Cap([string]$name) {
  $script:hwnd = Get-BlopHwnd
  [void][WinS6]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 70
  $r = New-Object WinS6+RECT
  [void][WinS6]::GetWindowRect($script:hwnd, [ref]$r)
  $w=$r.Right-$r.Left; $h=$r.Bottom-$r.Top
  $bmp = New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose()
  $bmp.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output $name
}
function Rel([int]$rx,[int]$ry) {
  $script:hwnd=Get-BlopHwnd
  $r=New-Object WinS6+RECT
  [void][WinS6]::GetWindowRect($script:hwnd,[ref]$r)
  @{x=$r.Left+$rx;y=$r.Top+$ry}
}
function ClickRel([int]$rx,[int]$ry){ $p=Rel $rx $ry; [WinS6]::Click($p.x,$p.y) }
function FindErstellen {
  # Prefer dense blue in LOWER third of right modal — footer primary
  $script:hwnd=Get-BlopHwnd
  $r=New-Object WinS6+RECT
  [void][WinS6]::GetWindowRect($script:hwnd,[ref]$r)
  $w=$r.Right-$r.Left; $h=$r.Bottom-$r.Top
  $bmp=New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose()
  $best=0; $bx=-1; $by=-1
  for ($y=700; $y -le 960; $y+=3) {
    for ($x=1000; $x -le 1500; $x+=3) {
      $c=$bmp.GetPixel($x,$y)
      if ($c.B -gt 190 -and $c.R -lt 110 -and $c.G -gt 130 -and $c.G -lt 200 -and ($c.B-$c.R) -gt 90) {
        $n=0
        for ($dy=-6;$dy -le 6;$dy+=2){ for($dx=-12;$dx -le 12;$dx+=2){
          $xx=$x+$dx;$yy=$y+$dy
          if($xx -lt 0 -or $yy -lt 0 -or $xx -ge $w -or $yy -ge $h){continue}
          $c2=$bmp.GetPixel($xx,$yy)
          if($c2.B -gt 180 -and $c2.R -lt 120 -and ($c2.B-$c2.R) -gt 80){$n++}
        }}
        if ($n -gt $best) { $best=$n; $bx=$x; $by=$y }
      }
    }
  }
  $bmp.Dispose()
  if ($bx -lt 0 -or $best -lt 10) { return $null }
  @{x=$r.Left+$bx;y=$r.Top+$by;rx=$bx;ry=$by;score=$best}
}

try {
  Cap '01_startup.png'
  [WinS6]::Key(0x1B); Start-Sleep -Milliseconds 200

  ClickRel 28 95; Start-Sleep -Milliseconds 800
  Cap '02_home.png'

  ClickRel 28 135; Start-Sleep -Milliseconds 700
  Cap '03_library.png'

  # Neue Notiz rail
  ClickRel 28 175; Start-Sleep -Milliseconds 1100
  Cap '04_pick.png'

  # DIN A4 = middle card. Unendlich left, A4 center, Struktur right.
  # Modal centered ~800-1400; middle card center ~1100? Unendlich visible left of modal.
  # From smoke5 04: Unendlich left of dialog, A4 to its right. Click further right of Unendlich center.
  ClickRel 1050 480
  Start-Sleep -Milliseconds 1200
  Cap '05_composer.png'

  # Force DIN A4 segment (middle of three chips under title bar)
  ClickRel 1080 280
  Start-Sleep -Milliseconds 400
  # Kariert
  ClickRel 1000 450
  Start-Sleep -Milliseconds 300
  Cap '06_a4_ready.png'

  $btn = FindErstellen
  if ($btn) {
    Write-Output ("Erstellen dens={0} @{1},{2}" -f $btn.score,$btn.rx,$btn.ry)
    [WinS6]::Click($btn.x,$btn.y)
  } else {
    Write-Output 'Enter fallback'
    [WinS6]::Key(0x0D)
  }
  Start-Sleep -Seconds 2.5
  Cap '07_editor.png'

  # If still dialog, Enter again
  $btn2 = FindErstellen
  if ($btn2) {
    Write-Output 'still dialog — Enter'
    [WinS6]::Key(0x0D)
    Start-Sleep -Seconds 2.2
    Cap '07b_editor.png'
  }

  $a=Rel 720 400; $b=Rel 1020 540
  [WinS6]::Drag($a.x,$a.y,$b.x,$b.y)
  Start-Sleep -Milliseconds 450
  Cap '08_write.png'

  ClickRel 28 135; Start-Sleep -Milliseconds 1100
  Cap '09_library.png'

  ClickRel 28 95; Start-Sleep -Milliseconds 1100
  Cap '10_dashboard.png'

  ClickRel 1520 75; Start-Sleep -Milliseconds 700
  Cap '11_anpassen.png'
  $d1=Rel 480 340; $d2=Rel 640 420
  [WinS6]::Drag($d1.x,$d1.y,$d2.x,$d2.y)
  Start-Sleep -Milliseconds 350
  Cap '12_moved.png'
  ClickRel 1520 75; Start-Sleep -Milliseconds 600
  Cap '13_fertig.png'

  Write-Output "DONE $outDir"
}
finally { try { [void][WinS6]::BlockInput($false) } catch {} }
