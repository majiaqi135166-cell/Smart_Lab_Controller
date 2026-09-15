# 截取指定进程的主窗口
param(
    [string]$ProcessName = "Smart_Lab_Controller",
    [string]$OutputPath = "D:\qt\qtproject\Smart_Lab_Controller\screenshot.png"
)

Add-Type -AssemblyName System.Drawing

# Win32 API
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@

$proc = Get-Process -Name $ProcessName -ErrorAction Stop
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host "[ERROR] 找不到主窗口" -ForegroundColor Red
    exit 1
}

# 置顶窗口确保不被遮挡
[Win32]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 500

# 获取窗口矩形
$rect = New-Object Win32+RECT
[Win32]::GetWindowRect($hwnd, [ref]$rect) | Out-Null

$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
Write-Host "窗口区域: ($($rect.Left), $($rect.Top)) 尺寸: ${width}x${height}"

if ($width -le 0 -or $height -le 0) {
    Write-Host "[ERROR] 窗口尺寸无效" -ForegroundColor Red
    exit 1
}

# 截取窗口区域
$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
$bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$bitmap.Dispose()

Write-Host "[OK] 截图已保存: $OutputPath" -ForegroundColor Green
