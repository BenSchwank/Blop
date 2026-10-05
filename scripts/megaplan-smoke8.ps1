# Smoke8: open composer via Unendlich, A4 chip, Enter to create (no backdrop click)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS8 {
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
    SetCursorPos(x,y); System.Threading.Thread.Sleep(110);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(80);
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Drag(int x1,int y1,int x2,int y2){
    SetCursorPos(x1,y1); System.Threading.Thread.Sleep(40);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    for(int i=1;i<=14;i++){ SetCursorPos(x1+(x2-x1)*i/14, y1+(y2-y1)*i/14); System.Threading.Thread.Sleep(22); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Key(byte vk){ keybd_event(vk,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(45); keybd_event(vk,0,KEYUP,UIntPtr.Zero);}
}
'@

$exe = 'c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Git\Blop\build-check\Blop.exe'
$outDir = Join-Path $env:TEMP 'blop-megaplan-smoke8'
if (Test-Path $outDir) { Remove-Item $outDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Get-Process Blop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
Start-Process $exe

function Get-BlopHwnd {
  $p = Get-Process Blop -ErrorAction SilentlyContinue | Where-Object {
    $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like '*Blop*'
  } | Select-Object -First 1
  if ($p) { return $p.MainWindowHandle } else { return [IntPtr]::Zero }
}
$hwnd=[IntPtr]::Zero
for($i=0;$i -lt 60;$i++){
  Start-Sleep -Milliseconds 200
  $hwnd=Get-BlopHwnd
  if($hwnd -eq [IntPtr]::Zero){continue}
  $r=New-Object WinS8+RECT
  [void][WinS8]::GetWindowRect($hwnd,[ref]$r)
  if(($r.Right-$r.Left) -gt 500){break}
}
[void][WinS8]::ShowWindow($hwnd,9)
[void][WinS8]::MoveWindow($hwnd,20,20,1600,1000,$true)
Start-Sleep -Milliseconds 1000
[void][WinS8]::SetForegroundWindow($hwnd)
try{[void][WinS8]::BlockInput($true)}catch{}

function Cap([string]$n){
  $script:hwnd=Get-BlopHwnd
  [void][WinS8]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 90
  $r=New-Object WinS8+RECT
  [void][WinS8]::GetWindowRect($script:hwnd,[ref]$r)
  $w=$r.Right-$r.Left;$h=$r.Bottom-$r.Top
  $bmp=New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose(); $bmp.Save((Join-Path $outDir $n),[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  Write-Output $n
}
function Rel([int]$rx,[int]$ry){
  $script:hwnd=Get-BlopHwnd; $r=New-Object WinS8+RECT
  [void][WinS8]::GetWindowRect($script:hwnd,[ref]$r)
  @{x=$r.Left+$rx;y=$r.Top+$ry}
}
function ClickRel([int]$rx,[int]$ry){ $p=Rel $rx $ry; [WinS8]::Click($p.x,$p.y) }
function FindErstellen {
  $script:hwnd=Get-BlopHwnd; $r=New-Object WinS8+RECT
  [void][WinS8]::GetWindowRect($script:hwnd,[ref]$r)
  $w=$r.Right-$r.Left;$h=$r.Bottom-$r.Top
  $bmp=New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose()
  $best=0;$bx=-1;$by=-1
  for($y=650;$y -le 960;$y+=2){
    for($x=900;$x -le 1500;$x+=2){
      $c=$bmp.GetPixel($x,$y)
      if($c.B -gt 180 -and $c.R -lt 140 -and $c.G -gt 120 -and $c.G -lt 210 -and ($c.B-$c.R) -gt 70){
        $n=0
        for($dy=-5;$dy -le 5;$dy+=2){for($dx=-10;$dx -le 10;$dx+=2){
          $xx=$x+$dx;$yy=$y+$dy
          if($xx -lt 0 -or $yy -lt 0 -or $xx -ge $w -or $yy -ge $h){continue}
          $c2=$bmp.GetPixel($xx,$yy)
          if($c2.B -gt 170 -and $c2.R -lt 150 -and ($c2.B-$c2.R) -gt 60){$n++}
        }}
        if($n -gt $best){$best=$n;$bx=$x;$by=$y}
      }
    }
  }
  $bmp.Dispose()
  if($bx -lt 0 -or $best -lt 8){return $null}
  @{x=$r.Left+$bx;y=$r.Top+$by;rx=$bx;ry=$by;score=$best}
}

try {
  Cap '01.png'
  [WinS8]::Key(0x1B); Start-Sleep -Milliseconds 200

  ClickRel 28 135; Start-Sleep -Milliseconds 800
  Cap '02_lib.png'

  # Rail Neue Notiz
  ClickRel 28 175; Start-Sleep -Milliseconds 1300
  Cap '03_pick.png'

  # Unendlich card is leftmost in pick row — pick card sits right (~x1180+)
  ClickRel 1280 480
  Start-Sleep -Milliseconds 1400
  Cap '04_composer.png'

  # DIN A4 chip (middle)
  ClickRel 1020 300
  Start-Sleep -Milliseconds 500
  Cap '05_a4.png'

  # Kariert
  ClickRel 1000 480
  Start-Sleep -Milliseconds 400
  Cap '06_ready.png'

  # Prefer Erstellen button; else Enter in title field (returnPressed → accept)
  $btn=FindErstellen
  if($btn){
    Write-Output ("Erstellen dens={0} @{1},{2}" -f $btn.score,$btn.rx,$btn.ry)
    [WinS8]::Click($btn.x,$btn.y)
  } else {
    Write-Output 'Enter on title (no backdrop click)'
    # Click title field then Enter
    ClickRel 1100 400
    Start-Sleep -Milliseconds 200
    [WinS8]::Key(0x0D)
  }
  Start-Sleep -Seconds 3
  Cap '07_editor.png'

  $a=Rel 700 400; $b=Rel 1000 550
  [WinS8]::Drag($a.x,$a.y,$b.x,$b.y)
  Start-Sleep -Milliseconds 500
  Cap '08_write.png'

  ClickRel 28 135; Start-Sleep -Milliseconds 1200
  Cap '09_lib.png'

  ClickRel 28 95; Start-Sleep -Milliseconds 1200
  Cap '10_dash.png'

  ClickRel 1520 75; Start-Sleep -Milliseconds 800
  Cap '11_anpassen.png'
  $d1=Rel 500 350;$d2=Rel 640 420
  [WinS8]::Drag($d1.x,$d1.y,$d2.x,$d2.y)
  Start-Sleep -Milliseconds 400
  Cap '12_moved.png'
  ClickRel 1520 75; Start-Sleep -Milliseconds 700
  Cap '13_fertig.png'

  Write-Output "DONE $outDir"
} finally { try{[void][WinS8]::BlockInput($false)}catch{} }
