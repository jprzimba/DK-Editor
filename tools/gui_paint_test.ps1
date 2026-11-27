param(
    [string]$Rom = "Donkey Kong Country (U) (V1.2) [!].smc",
    [string]$Out = "ui_paint.png"
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, IntPtr dwExtraInfo);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$leftDown = 0x0002
$leftUp = 0x0004
$rightDown = 0x0008
$rightUp = 0x0010

$tmp = Join-Path $env:TEMP "dkc1_ui_test.smc"
Copy-Item -LiteralPath $Rom -Destination $tmp -Force
$exe = Join-Path (Get-Location) "build\Release\DKEditor.exe"
$p = Start-Process -FilePath $exe -ArgumentList @("`"$tmp`"") -PassThru
Start-Sleep -Seconds 4
$p.Refresh()
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Start-Sleep -Seconds 3; $p.Refresh(); $h = $p.MainWindowHandle }
if ($h -eq [IntPtr]::Zero) { Write-Output "No window"; $p.Kill(); exit 1 }
[Win32]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 700
$r = New-Object Win32+RECT
[Win32]::GetWindowRect($h, [ref]$r) | Out-Null
$L = $r.Left; $T = $r.Top
Write-Output "window $L,$T"

# Paint a horizontal stroke in the sky and erase a stroke on the ground.
[Win32]::SetCursorPos($L + 600, $T + 680) | Out-Null
Start-Sleep -Milliseconds 200
[Win32]::mouse_event($leftDown, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 200
for ($i = 0; $i -le 10; $i++) {
    [Win32]::SetCursorPos($L + 600 + $i * 8, $T + 680) | Out-Null
    Start-Sleep -Milliseconds 25
}
[Win32]::mouse_event($leftUp, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 300

[Win32]::SetCursorPos($L + 600, $T + 700) | Out-Null
Start-Sleep -Milliseconds 200
[Win32]::mouse_event($rightDown, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 200
for ($i = 0; $i -le 10; $i++) {
    [Win32]::SetCursorPos($L + 600 + $i * 8, $T + 700) | Out-Null
    Start-Sleep -Milliseconds 25
}
[Win32]::mouse_event($rightUp, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400

# Screenshot after painting.
$bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
$bmp.Save((Join-Path (Get-Location) $Out), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

# Save via the toolbar.
[Win32]::SetCursorPos($L + 77, $T + 70) | Out-Null
Start-Sleep -Milliseconds 200
[Win32]::mouse_event($leftDown, 0, 0, 0, [IntPtr]::Zero)
[Win32]::mouse_event($leftUp, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Seconds 2

$p.CloseMainWindow() | Out-Null
Start-Sleep -Milliseconds 800
if (!$p.HasExited) { $p.Kill() }
Write-Output "temp rom: $tmp"
Write-Output "saved $Out"
