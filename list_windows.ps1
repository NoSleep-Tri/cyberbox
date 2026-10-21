Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class WinList {
    public delegate bool EnumThreadDelegate(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumThreadWindows(int dwThreadId, EnumThreadDelegate lpfn, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
}
"@

$p = Get-Process cyberbox -ErrorAction SilentlyContinue
if (-not $p) { Write-Host "No cyberbox process!"; exit }

foreach ($t in $p.Threads) {
    [WinList]::EnumThreadWindows($t.Id, {
        param($h, $l)
        $sbClass = New-Object System.Text.StringBuilder 256
        [WinList]::GetClassName($h, $sbClass, 256)
        $sbText = New-Object System.Text.StringBuilder 256
        [WinList]::GetWindowText($h, $sbText, 256)
        Write-Host "Thread $($t.Id) HWND: $h | Class: $($sbClass.ToString()) | Text: $($sbText.ToString())"
        return $true
    }, [IntPtr]::Zero)
}
