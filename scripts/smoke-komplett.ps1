# Notiz Alltagspfad smoke — screenshots under $env:TEMP\blop-komplett-smoke
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinU {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public const uint LEFTDOWN=2, LEFTUP=4; public const uint KEYUP=2;
  public static void Click(int x,int y){ SetCursorPos(x,y); System.Threading.Thread.Sleep(50); mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(40); mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);}
  public static void DblClick(int x,int y){ Click(x,y); System.Threading.Thread.Sleep(90); Click(x,y);}
  public static void Drag(int x1,int y1,int x2,int y2){
    SetCursorPos(x1,y1); System.Threading.Thread.Sleep(40);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    for(int i=1;i<=10;i++){ SetCursorPos(x1+(x2-x1)*i/10, y1+(y2-y1)*i/10); System.Threading.Thread.Sleep(25); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Key(byte vk){ keybd_event(vk,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(30); keybd_event(vk,0,KEYUP,UIntPtr.Zero);}
}
'@

$exe = Join-Path $PSScriptRoot '..\build-check\Blop.exe' | Resolve-Path
$outDir = Join-Path $env:TEMP 'blop-komplett-smoke'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
Remove-Item (Join-Path $outDir '*.png') -Force -ErrorAction SilentlyContinue

Get-Process Blop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
Start-Process $exe.Path

$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
  Start-Sleep -Milliseconds 250
  $p = Get-Process Blop -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1
  if (-not $p) { continue }
  $r = New-Object WinU+RECT
  [void][WinU]::GetWindowRect($p.MainWindowHandle, [ref]$r)
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  if ($w -gt 400 -and $h -gt 300) {
    $hwnd = $p.MainWindowHandle
    Write-Output "ready ${w}x${h} hwnd=$hwnd"
    break
  }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'no Blop window' }

[void][WinU]::ShowWindow($hwnd, 9)
[void][WinU]::MoveWindow($hwnd, 40, 40, 1400, 900, $true)
Start-Sleep -Milliseconds 500
[void][WinU]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 400

function Get-Rect {
  $r = New-Object WinU+RECT
  [void][WinU]::GetWindowRect($script:hwnd, [ref]$r)
  $r
}
function Cap([string]$name) {
  $r = Get-Rect
  $w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
  if ($w -lt 80 -or $h -lt 80) { throw "$name FAIL size ${w}x${h}" }
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
  $g.Dispose()
  $path = Join-Path $outDir $name
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output "$name ${w}x${h} $((Get-Item $path).Length)b"
}
function Rel([int]$rx, [int]$ry) {
  $r = Get-Rect
  @{ x = $r.Left + $rx; y = $r.Top + $ry }
}

Cap '01_startup.png'
[WinU]::Key(0x1B); Start-Sleep -Milliseconds 200

$pt = Rel 55 28; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 700
Cap '02_burger.png'

$pt = Rel 110 150; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 900
Cap '03_library.png'

$pt = Rel 480 300; [WinU]::DblClick($pt.x, $pt.y); Start-Sleep -Seconds 2
Cap '04_struktur.png'

$pt = Rel 1240 28; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 900
Cap '05_theme1.png'

$pt = Rel 680 400; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Seconds 2
Cap '06_a4.png'

$pt = Rel 700 450; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 200
[WinU]::Key(0x32); Start-Sleep -Milliseconds 700
Cap '07_digit2.png'

$pt = Rel 1240 28; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 800
Cap '08_a4_theme.png'

$pt = Rel 200 110; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Seconds 1
Cap '09_back.png'

$pt = Rel 640 360; [WinU]::SetCursorPos($pt.x, $pt.y); Start-Sleep -Milliseconds 400
$pt2 = Rel 860 450; [WinU]::Drag($pt.x, $pt.y, $pt2.x, $pt2.y); Start-Sleep -Milliseconds 250
Cap '10_drag.png'

$pt = Rel 480 680; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 600
Cap '11_line.png'
$pt = Rel 400 720; [WinU]::Click($pt.x, $pt.y); Start-Sleep -Seconds 1
Cap '12_chip.png'

Get-ChildItem $outDir | Format-Table Name, Length -AutoSize
$root = 'C:\Users\NaqsZ\OneDrive\Documentos\BlopNotizen'
Write-Output ("MIGRATE root={0} embeds={1}" -f
  (Test-Path (Join-Path $root 'Eingebettete Notiz smoke.bnote')),
  (Test-Path (Join-Path $root '.blop-embeds\Eingebettete Notiz smoke.bnote')))
Write-Output "OUT=$outDir"
