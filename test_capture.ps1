Add-Type -ReferencedAssemblies "System.Drawing" -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;

public class Win32Helper {
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static IntPtr MakeLParam(int LoWord, int HiWord) {
        return (IntPtr)((HiWord << 16) | (LoWord & 0xFFFF));
    }
}
"@

Stop-Process -Name cyberbox -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$proc = Start-Process -FilePath ".\cyberbox.exe" -PassThru
Start-Sleep -Seconds 2

$hwnd = [Win32Helper]::FindWindow("CyberBoxAiMainWindowClass", $null)
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Host "Window not found!"
    Stop-Process -Id $proc.Id -Force
    exit 1
}

[Win32Helper]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 500

$rc = New-Object Win32Helper+RECT
[Win32Helper]::GetClientRect($hwnd, [ref]$rc)
$clientH = $rc.Bottom

# Click on Settings icon: icon 6 (Y = clientH - 70, X = 24)
$setX = 24
$setY = $clientH - 70
$lparamSet = [Win32Helper]::MakeLParam($setX, $setY)

# WM_LBUTTONDOWN = 0x0201, WM_LBUTTONUP = 0x0202
[Win32Helper]::SendMessage($hwnd, 0x0201, [IntPtr]1, $lparamSet)
Start-Sleep -Milliseconds 100
[Win32Helper]::SendMessage($hwnd, 0x0202, [IntPtr]0, $lparamSet)
Start-Sleep -Seconds 1

# Capture window screenshot
$wrc = New-Object Win32Helper+RECT
[Win32Helper]::GetWindowRect($hwnd, [ref]$wrc)
$w = $wrc.Right - $wrc.Left
$h = $wrc.Bottom - $wrc.Top

$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($wrc.Left, $wrc.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
$bmp.Save("c:\Users\Tri\antigravity\joyful-faraday\cyberbox_settings_fixed.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "Captured cyberbox_settings_fixed.png"

# Now click on Arena icon (icon 5)
$startY = 44 + (($clientH - 44 - 364) / 2)
$arenaY = [int]($startY + (5 * 46) + 18)
$lparamArena = [Win32Helper]::MakeLParam(24, $arenaY)
[Win32Helper]::SendMessage($hwnd, 0x0201, [IntPtr]1, $lparamArena)
Start-Sleep -Milliseconds 100
[Win32Helper]::SendMessage($hwnd, 0x0202, [IntPtr]0, $lparamArena)
Start-Sleep -Seconds 1

$bmp2 = New-Object System.Drawing.Bitmap($w, $h)
$g2 = [System.Drawing.Graphics]::FromImage($bmp2)
$g2.CopyFromScreen($wrc.Left, $wrc.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
$bmp2.Save("c:\Users\Tri\antigravity\joyful-faraday\cyberbox_arena_fixed.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g2.Dispose()
$bmp2.Dispose()
Write-Host "Captured cyberbox_arena_fixed.png"

Stop-Process -Id $proc.Id -Force
Write-Host "Done!"
