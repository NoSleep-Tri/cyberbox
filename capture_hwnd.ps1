Add-Type -ReferencedAssemblies "System.Drawing" -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Drawing;
using System.Drawing.Imaging;
public class WinFinder {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowW(string lpClassName, string lpWindowName);

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
        public int Left; public int Top; public int Right; public int Bottom;
    }
}
"@

$hwnd = [WinFinder]::FindWindowW("CyberBoxAiMainWindowClass", $null)
if ($hwnd -eq [IntPtr]::Zero) {
    $hwnd = [WinFinder]::FindWindowW($null, "Muse - CyberBox AI Native C++")
}
Write-Host "Found HWND:" $hwnd

[WinFinder]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 300

$rc = New-Object WinFinder+RECT
[WinFinder]::GetClientRect($hwnd, [ref]$rc)
$clientH = $rc.Bottom

# Click Settings tab (dock icon 6: X = 24, Y = clientH - 70)
$setX = 24
$setY = $clientH - 70
$lparamSet = [IntPtr](($setY -shl 16) -bor ($setX -band 0xFFFF))
[WinFinder]::SendMessage($hwnd, 0x0201, [IntPtr]1, $lparamSet)
Start-Sleep -Milliseconds 100
[WinFinder]::SendMessage($hwnd, 0x0202, [IntPtr]0, $lparamSet)
Start-Sleep -Seconds 1

# Capture settings screenshot
$wrc = New-Object WinFinder+RECT
[WinFinder]::GetWindowRect($hwnd, [ref]$wrc)
$w = $wrc.Right - $wrc.Left
$h = $wrc.Bottom - $wrc.Top

$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($wrc.Left, $wrc.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
$bmp.Save("c:\Users\Tri\antigravity\joyful-faraday\cyberbox_settings_fixed.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "Saved cyberbox_settings_fixed.png"

# Now click on Arena icon (icon 5)
$startY = 44 + (($clientH - 44 - 364) / 2)
$arenaY = [int]($startY + (5 * 46) + 18)
$lparamArena = [IntPtr](($arenaY -shl 16) -bor (24 -band 0xFFFF))
[WinFinder]::SendMessage($hwnd, 0x0201, [IntPtr]1, $lparamArena)
Start-Sleep -Milliseconds 100
[WinFinder]::SendMessage($hwnd, 0x0202, [IntPtr]0, $lparamArena)
Start-Sleep -Seconds 1

$bmp2 = New-Object System.Drawing.Bitmap($w, $h)
$g2 = [System.Drawing.Graphics]::FromImage($bmp2)
$g2.CopyFromScreen($wrc.Left, $wrc.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
$bmp2.Save("c:\Users\Tri\antigravity\joyful-faraday\cyberbox_arena_fixed.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g2.Dispose()
$bmp2.Dispose()
Write-Host "Saved cyberbox_arena_fixed.png"
