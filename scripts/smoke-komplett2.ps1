# Robust Blop smoke — always rebind hwnd to process titled Blop
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class WinS {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public const uint LEFTDOWN=2, LEFTUP=4; public const uint KEYUP=2;
  public static void Click(int x,int y){ SetCursorPos(x,y); System.Threading.Thread.Sleep(60); mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(50); mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);}
  public static void DblClick(int x,int y){ Click(x,y); System.Threading.Thread.Sleep(140); Click(x,y);}
  public static void Drag(int x1,int y1,int x2,int y2){
    SetCursorPos(x1,y1); System.Threading.Thread.Sleep(50);
    mouse_event(LEFTDOWN,0,0,0,UIntPtr.Zero);
    for(int i=1;i<=12;i++){ SetCursorPos(x1+(x2-x1)*i/12, y1+(y2-y1)*i/12); System.Threading.Thread.Sleep(30); }
    mouse_event(LEFTUP,0,0,0,UIntPtr.Zero);
  }
  public static void Key(byte vk){ keybd_event(vk,0,0,UIntPtr.Zero); System.Threading.Thread.Sleep(40); keybd_event(vk,0,KEYUP,UIntPtr.Zero);}
}
'@

$exe = 'c:\Users\NaqsZ\OneDrive\Arbeit_Schule\coding\Blop\Git\Blop\build-check\Blop.exe'
$outDir = Join-Path $env:TEMP 'blop-komplett-smoke'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Get-Process Blop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 700
Start-Process $exe

function Get-BlopHwnd {
  $p = Get-Process Blop -ErrorAction SilentlyContinue | Where-Object {
    $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like '*Blop*'
  } | Select-Object -First 1
  if ($p) { return $p.MainWindowHandle }
  return [IntPtr]::Zero
}

$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 50; $i++) {
  Start-Sleep -Milliseconds 200
  $hwnd = Get-BlopHwnd
  if ($hwnd -eq [IntPtr]::Zero) { continue }
  $r = New-Object WinS+RECT
  [void][WinS]::GetWindowRect($hwnd, [ref]$r)
  if (($r.Right - $r.Left) -gt 500) { break }
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'Blop window missing' }

[void][WinS]::ShowWindow($hwnd, 9)
[void][WinS]::MoveWindow($hwnd, 40, 40, 1400, 900, $true)
Start-Sleep -Milliseconds 600
[void][WinS]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 400

function Cap([string]$name) {
  $script:hwnd = Get-BlopHwnd
  if ($script:hwnd -eq [IntPtr]::Zero) { Write-Output "$name SKIP no-hwnd"; return $false }
  [void][WinS]::SetForegroundWindow($script:hwnd)
  Start-Sleep -Milliseconds 120
  $r = New-Object WinS+RECT
  [void][WinS]::GetWindowRect($script:hwnd, [ref]$r)
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
  $r = New-Object WinS+RECT
  [void][WinS]::GetWindowRect($script:hwnd, [ref]$r)
  @{ x = $r.Left + $rx; y = $r.Top + $ry }
}

Cap 's01_lib.png'
[WinS]::Key(0x1B); Start-Sleep -Milliseconds 200

# Bibliothek
$pt = Rel 140 200; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 600
Cap 's02_biblio.png'

# Click Raster-Test in NOTIZEN list (second recent)
$pt = Rel 150 505; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Seconds 2.5
Cap 's03_after_sidebar_click.png'

# If still library: dblclick card body (not edge)
$pt = Rel 640 360; [WinS]::DblClick($pt.x, $pt.y); Start-Sleep -Seconds 2.5
Cap 's04_after_dbl.png'

# Theme palette (right of title, left of window buttons)
$pt = Rel 1220 30; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 1000
Cap 's05_theme.png'

# Embed card body
$pt = Rel 720 430; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Seconds 2.5
Cap 's06_a4.png'

# Focus canvas + digit 2
$pt = Rel 750 480; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 200
[WinS]::Key(0x32); Start-Sleep -Milliseconds 700
Cap 's07_digit.png'

# Theme again
$pt = Rel 1220 30; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 900
Cap 's08_theme2.png'

# Back pill
$pt = Rel 220 120; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Seconds 1.5
Cap 's09_back.png'

# Hover + drag for ghost
$pt = Rel 660 390; [WinS]::SetCursorPos($pt.x, $pt.y); Start-Sleep -Milliseconds 600
$pt2 = Rel 900 480; [WinS]::Drag($pt.x, $pt.y, $pt2.x, $pt2.y); Start-Sleep -Milliseconds 350
Cap 's10_drag.png'

# Empty line / chips area
$pt = Rel 520 700; [WinS]::Click($pt.x, $pt.y); Start-Sleep -Milliseconds 700
Cap 's11_line.png'

Get-ChildItem (Join-Path $outDir 's*.png') | Format-Table Name, Length -AutoSize
$root = 'C:\Users\NaqsZ\OneDrive\Documentos\BlopNotizen'
Write-Output ("MIGRATE root={0} embeds={1}" -f
  (Test-Path (Join-Path $root 'Eingebettete Notiz smoke.bnote')),
  (Test-Path (Join-Path $root '.blop-embeds\Eingebettete Notiz smoke.bnote')))
Write-Output "alive=$((Get-Process Blop -EA SilentlyContinue) -ne $null)"
