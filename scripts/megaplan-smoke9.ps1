# Smoke9: open existing note → write → library → dashboard Anpassen
# (create-dialog automation is flaky; JSON/badge fixes already verified)
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS9 {
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
  public static void DblClick(int x,int y){ Click(x,y); System.Threading.Thread.Sleep(160); Click(x,y); }
  public static void Drag(int x1,int y1,int x2,int y2){
    SetCursorPos(x1,y1); System.Threading.Thread.Sleep(40);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    for(int i=1;i<=16;i++){ SetCursorPos(x1+(x2-x1)*i/16, y1+(y2-y1)*i/16); System.Threading.Thread.Sleep(22); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Key(byte vk){ keybd_event(vk,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(40); keybd_event(vk,0,KEYUP,UIntPtr.Zero);}
}
'@

$exe = 'c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Git\Blop\build-check\Blop.exe'
$outDir = Join-Path $env:TEMP 'blop-megaplan-smoke9'
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
  $r=New-Object WinS9+RECT
  [void][WinS9]::GetWindowRect($hwnd,[ref]$r)
  if(($r.Right-$r.Left) -gt 500){break}
}
[void][WinS9]::ShowWindow($hwnd,9)
[void][WinS9]::MoveWindow($hwnd,20,20,1600,1000,$true)
Start-Sleep -Milliseconds 1000
[void][WinS9]::SetForegroundWindow($hwnd)
try{[void][WinS9]::BlockInput($true)}catch{}

function Cap([string]$n){
  $script:hwnd=Get-BlopHwnd
  [void][WinS9]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 80
  $r=New-Object WinS9+RECT
  [void][WinS9]::GetWindowRect($script:hwnd,[ref]$r)
  $w=$r.Right-$r.Left;$h=$r.Bottom-$r.Top
  $bmp=New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose(); $bmp.Save((Join-Path $outDir $n),[System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  Write-Output $n
}
function Rel([int]$rx,[int]$ry){
  $script:hwnd=Get-BlopHwnd; $r=New-Object WinS9+RECT
  [void][WinS9]::GetWindowRect($script:hwnd,[ref]$r)
  @{x=$r.Left+$rx;y=$r.Top+$ry}
}
function ClickRel([int]$rx,[int]$ry){ $p=Rel $rx $ry; [WinS9]::Click($p.x,$p.y) }
function DblRel([int]$rx,[int]$ry){ $p=Rel $rx $ry; [WinS9]::DblClick($p.x,$p.y) }

try {
  Cap '01_startup.png'
  [WinS9]::Key(0x1B); Start-Sleep -Milliseconds 200

  # Home
  ClickRel 28 95; Start-Sleep -Milliseconds 900
  Cap '02_home.png'

  # Library
  ClickRel 28 135; Start-Sleep -Milliseconds 900
  Cap '03_library.png'

  # Dblclick first note tile (grid card body)
  DblRel 620 320
  Start-Sleep -Seconds 2.5
  Cap '04_editor.png'

  # Draw
  $a=Rel 650 380; $b=Rel 980 520
  [WinS9]::Drag($a.x,$a.y,$b.x,$b.y)
  Start-Sleep -Milliseconds 500
  Cap '05_write.png'

  # Back via Escape / library rail
  [WinS9]::Key(0x1B)
  Start-Sleep -Milliseconds 500
  ClickRel 28 135
  Start-Sleep -Milliseconds 1100
  Cap '06_library_after.png'

  # Dashboard
  ClickRel 28 95; Start-Sleep -Milliseconds 1100
  Cap '07_dashboard.png'

  # Anpassen top-right
  ClickRel 1520 75; Start-Sleep -Milliseconds 800
  Cap '08_anpassen.png'

  $d1=Rel 480 340; $d2=Rel 640 420
  [WinS9]::Drag($d1.x,$d1.y,$d2.x,$d2.y)
  Start-Sleep -Milliseconds 400
  Cap '09_moved.png'

  ClickRel 1520 75; Start-Sleep -Milliseconds 700
  Cap '10_fertig.png'

  # Theme / settings via rail bottom
  ClickRel 28 900; Start-Sleep -Milliseconds 900
  Cap '11_settings.png'

  Write-Output "DONE $outDir"
} finally { try{[void][WinS9]::BlockInput($false)}catch{} }
