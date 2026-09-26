# Regenerates the two embedded logo assets from images/Logo.png:
#   - src/web/logo_png.h    128x128 PNG, served at /logo.png by the admin page
#   - src/ui/logo_bitmap.h  72x72 RGB565, drawn once on the TFT boot splash
#
# Run whenever images/Logo.png changes:
#   pwsh -File scripts/gen_logo.ps1
#
# Uses .NET's System.Drawing directly - no ImageMagick/PIL/ffmpeg needed,
# which is why this is PowerShell rather than a build-time Python step like
# upstream's embed_readme.py.

Add-Type -AssemblyName System.Drawing

$root   = Split-Path -Parent $PSScriptRoot
$srcPng = Join-Path $root "images\Logo.png"
$src    = [System.Drawing.Bitmap]::new($srcPng)

function Resize-Bitmap($bmp, $w, $h) {
    $dst = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($bmp, 0, 0, $w, $h)
    $g.Dispose()
    return $dst
}

function Write-ByteArrayHeader($bytes, $path, $varName, $lenName, $comment) {
    $sb = New-Object System.Text.StringBuilder
    $sb.AppendLine("#pragma once")   | Out-Null
    $sb.AppendLine("#include <stdint.h>") | Out-Null
    $sb.AppendLine("#include <stddef.h>") | Out-Null
    $sb.AppendLine()                 | Out-Null
    foreach ($line in $comment) { $sb.AppendLine("// $line") | Out-Null }
    $sb.AppendLine("static const uint8_t $varName[] PROGMEM = {") | Out-Null

    $line = "    "
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $line += "0x{0:x2}," -f $bytes[$i]
        if (($i + 1) % 16 -eq 0) { $sb.AppendLine($line) | Out-Null; $line = "    " }
        else { $line += " " }
    }
    if ($line.Trim().Length -gt 0) { $sb.AppendLine($line) | Out-Null }
    $sb.AppendLine("};") | Out-Null
    $sb.AppendLine("static const size_t $lenName = sizeof($varName);") | Out-Null

    [System.IO.File]::WriteAllText($path, $sb.ToString())
}

# --- Web asset: 128x128 PNG, re-encoded (keeps alpha, the browser composites it) ---
$web = Resize-Bitmap $src 128 128
$ms  = New-Object System.IO.MemoryStream
$web.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
$webBytes = $ms.ToArray()

Write-ByteArrayHeader $webBytes (Join-Path $root "src\web\logo_png.h") "LOGO_PNG" "LOGO_PNG_LEN" @(
    "128x128 PNG re-encoded from images/Logo.png, served at /logo.png so every"
    "page can reference it with a plain <img> tag instead of re-embedding the"
    "bytes inline on every render - the browser fetches and caches it once."
    "Regenerate with scripts/gen_logo.ps1 if the source logo changes."
)
Write-Output "wrote src/web/logo_png.h ($($webBytes.Length) bytes)"

$web.Dispose(); $ms.Dispose()

# --- TFT asset: 72x72 RGB565, alpha-composited against Ui::COLOR_BG (8,10,16) ---
# RGB565 has no alpha channel, so blending against the actual boot-screen
# background ahead of time keeps the logo's glow/rounded edges clean instead
# of a hard-edged color key.
$W = 72; $H = 72
$bgR = 8; $bgG = 10; $bgB = 16

$tft = Resize-Bitmap $src $W $H

$sb = New-Object System.Text.StringBuilder
$sb.AppendLine("#pragma once") | Out-Null
$sb.AppendLine("#include <stdint.h>") | Out-Null
$sb.AppendLine() | Out-Null
$sb.AppendLine("// ${W}x${H} RGB565, resized from images/Logo.png and alpha-composited") | Out-Null
$sb.AppendLine("// against Ui::COLOR_BG (8,10,16) ahead of time - see the note in") | Out-Null
$sb.AppendLine("// scripts/gen_logo.ps1. Regenerate with that script if the logo changes.") | Out-Null
$sb.AppendLine("static const int LOGO_BITMAP_W = $W;") | Out-Null
$sb.AppendLine("static const int LOGO_BITMAP_H = $H;") | Out-Null
$sb.AppendLine("static const uint16_t LOGO_BITMAP[] PROGMEM = {") | Out-Null

$line = "    "; $count = 0
for ($y = 0; $y -lt $H; $y++) {
    for ($x = 0; $x -lt $W; $x++) {
        $px = $tft.GetPixel($x, $y)
        $a  = $px.A / 255.0
        $r  = [int]([math]::Round($px.R * $a + $bgR * (1 - $a)))
        $gr = [int]([math]::Round($px.G * $a + $bgG * (1 - $a)))
        $b  = [int]([math]::Round($px.B * $a + $bgB * (1 - $a)))
        $rgb565 = (($r -band 0xF8) -shl 8) -bor (($gr -band 0xFC) -shl 3) -bor ($b -shr 3)
        $line += "0x{0:x4}," -f $rgb565
        $count++
        if ($count % 12 -eq 0) { $sb.AppendLine($line) | Out-Null; $line = "    " }
        else { $line += " " }
    }
}
if ($line.Trim().Length -gt 0) { $sb.AppendLine($line) | Out-Null }
$sb.AppendLine("};") | Out-Null

[System.IO.File]::WriteAllText((Join-Path $root "src\ui\logo_bitmap.h"), $sb.ToString())
Write-Output "wrote src/ui/logo_bitmap.h (${W}x${H})"

$tft.Dispose(); $src.Dispose()
Remove-Item (Join-Path $root "images\logo-web-128.png") -ErrorAction SilentlyContinue
Write-Output "done"
