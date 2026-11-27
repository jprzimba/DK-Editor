param([string]$Rom = "Donkey Kong Country (U) (V1.2) [!].smc", [string]$Out = "ui_shot.png", [string]$Level = "")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$exe = Join-Path (Get-Location) "build\Release\DKEditor.exe"
$args = @("`"$Rom`"")
if ($Level -ne "") { $args += $Level }
$p = Start-Process -FilePath $exe -ArgumentList $args -PassThru
Start-Sleep -Seconds 4
$p.Refresh()
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Start-Sleep -Seconds 3; $p.Refresh(); $h = $p.MainWindowHandle }
if ($h -eq [IntPtr]::Zero) { Write-Output "No window"; $p.Kill(); exit 1 }
[Win32]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 800
$r = New-Object Win32+RECT
[Win32]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $hgt = $r.Bottom - $r.Top
Write-Output "rect $($r.Left),$($r.Top) ${w}x${hgt}"
$bmp = New-Object System.Drawing.Bitmap $w, $hgt
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[Win32]::PrintWindow($h, $hdc, 2) | Out-Null
$g.ReleaseHdc($hdc)
$bmp.Save((Join-Path (Get-Location) $Out), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
$p.CloseMainWindow() | Out-Null
Start-Sleep -Milliseconds 500
if (!$p.HasExited) { $p.Kill() }
Write-Output "saved $Out"
