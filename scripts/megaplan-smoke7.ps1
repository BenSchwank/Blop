# Smoke7: Unendlich pick → switch DIN A4 → Erstellen (taller modal)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS7 {
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
$outDir = Join-Path $env:TEMP 'blop-megaplan-smoke7'
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
  $r=New-Object WinS7+RECT
  [void][WinS7]::GetWindowRect($hwnd,[ref]$r)
  if(($r.Right-$r.Left) -gt 500){break}
}
[void][WinS7]::ShowWindow($hwnd,9)
[void][WinS7]::MoveWindow($hwnd,20,20,1600,1000,$true)
Start-Sleep -Milliseconds 900
[void][WinS7]::SetForegroundWindow($hwnd)
try{[void][WinS7]::BlockInput($true)}catch{}

function Cap([string]$n){
  $script:hwnd=Get-BlopHwnd
  [void][WinS7]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 80
  $r=New-Object WinS7+RECT
  [void][WinS7]::GetWindowRect($script:hwnd,[ref]$r)
  $w=$r.Right-$r.Left;$h=$r.Bottom-$r.Top
  $bmp=New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose(); $bmp.Save((Join-Path $outDir $n),[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  Write-Output $n
}
function Rel([int]$rx,[int]$ry){
  $script:hwnd=Get-BlopHwnd; $r=New-Object WinS7+RECT
  [void][WinS7]::GetWindowRect($script:hwnd,[ref]$r)
  @{x=$r.Left+$rx;y=$r.Top+$ry}
}
function ClickRel([int]$rx,[int]$ry){ $p=Rel $rx $ry; [WinS7]::Click($p.x,$p.y) }
function FindErstellen {
  $script:hwnd=Get-BlopHwnd; $r=New-Object WinS7+RECT
  [void][WinS7]::GetWindowRect($script:hwnd,[ref]$r)
  $w=$r.Right-$r.Left;$h=$r.Bottom-$r.Top
  $bmp=New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose()
  $best=0;$bx=-1;$by=-1
  # Footer zone — lower part of taller modal
  for($y=720;$y -le 960;$y+=2){
    for($x=1050;$x -le 1520;$x+=2){
      $c=$bmp.GetPixel($x,$y)
      # primary #5B9DFF-ish OR solid accent fill
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
  [WinS7]::Key(0x1B); Start-Sleep -Milliseconds 200

  # Library then Neue Notiz
  ClickRel 28 135; Start-Sleep -Milliseconds 800
  Cap '02_lib.png'
  ClickRel 28 175; Start-Sleep -Milliseconds 1200
  Cap '03_pick.png'

  # Click LEFT card Unendlich (clearly visible) to open composer
  ClickRel 980 470
  Start-Sleep -Milliseconds 1300
  Cap '04_composer.png'

  # Switch to DIN A4 segment (middle chip)
  ClickRel 1100 275
  Start-Sleep -Milliseconds 450
  Cap '05_a4.png'

  # Kariert template
  ClickRel 1020 460
  Start-Sleep -Milliseconds 350
  Cap '06_template.png'

  $btn=FindErstellen
  if($btn){
    Write-Output ("Erstellen dens={0} @{1},{2}" -f $btn.score,$btn.rx,$btn.ry)
    [WinS7]::Click($btn.x,$btn.y)
  } else {
    Write-Output 'no blue — click footer then Enter'
    ClickRel 1280 880
    Start-Sleep -Milliseconds 200
    [WinS7]::Key(0x0D)
  }
  Start-Sleep -Seconds 2.8
  Cap '07_after_create.png'

  $btn2=FindErstellen
  if($btn2 -and $btn2.score -ge 10){
    Write-Output 'retry create'
    [WinS7]::Click($btn2.x,$btn2.y)
    Start-Sleep -Seconds 2.5
    Cap '07b.png'
  }

  $a=Rel 700 400; $b=Rel 1000 550
  [WinS7]::Drag($a.x,$a.y,$b.x,$b.y)
  Start-Sleep -Milliseconds 400
  Cap '08_write.png'

  ClickRel 28 135; Start-Sleep -Milliseconds 1100
  Cap '09_lib.png'

  ClickRel 28 95; Start-Sleep -Milliseconds 1100
  Cap '10_dash.png'

  ClickRel 1520 75; Start-Sleep -Milliseconds 700
  Cap '11_edit.png'
  $d1=Rel 500 350;$d2=Rel 640 420
  [WinS7]::Drag($d1.x,$d1.y,$d2.x,$d2.y)
  Start-Sleep -Milliseconds 350
  Cap '12_moved.png'
  ClickRel 1520 75; Start-Sleep -Milliseconds 600
  Cap '13_done.png'

  Write-Output "DONE $outDir"
} finally { try{[void][WinS7]::BlockInput($false)}catch{} }
