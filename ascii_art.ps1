Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile('C:/Users/Tri/.gemini/antigravity/brain/615beb12-d545-47d8-b39d-53c9d81a79e0/.user_uploaded/media_1792102896966_d5a47fb6.png')
Write-Host "Width: $($bmp.Width), Height: $($bmp.Height)"
for ($y = 0; $y -lt $bmp.Height; $y += 3) {
    $line = ""
    for ($x = 0; $x -lt $bmp.Width; $x += 4) {
        $c = $bmp.GetPixel($x, $y)
        if ($c.R -gt 230 -and $c.G -gt 230 -and $c.B -gt 230) {
            $line += "#"
        } elseif ($c.R -gt 150) {
            $line += "+"
        } elseif ($c.R -gt 50) {
            $line += "."
        } else {
            $line += " "
        }
    }
    if ($line.Trim().Length -gt 0) {
        Write-Host $line
    }
}
$bmp.Dispose()
