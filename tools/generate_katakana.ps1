Add-Type -AssemblyName System.Drawing
$matrixFontCollection = New-Object System.Drawing.Text.PrivateFontCollection
$matrixFontCollection.AddFontFile('C:/Windows/Fonts/NotoSansJP-VF.ttf')
$matrixFont = New-Object System.Drawing.Font($matrixFontCollection.Families[0],22,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
$matrixChars = 'アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン'
$matrixData = New-Object System.Collections.Generic.List[string]
$matrixData.Add('// Rasterized Katakana from Noto Sans JP (SIL OFL 1.1). See third_party/NotoSansJP-OFL.txt.')
$matrixData.Add('#pragma once')
$matrixData.Add("static constexpr unsigned MATRIX_GLYPHS=$($matrixChars.Length);")
$matrixData.Add('static const unsigned char matrixGlyphMasks[MATRIX_GLYPHS][512]={')
foreach($matrixChar in $matrixChars.ToCharArray()) {
    $matrixBitmap = New-Object System.Drawing.Bitmap(32,32)
    $matrixGraphics = [System.Drawing.Graphics]::FromImage($matrixBitmap)
    $matrixGraphics.Clear([System.Drawing.Color]::Transparent)
    $matrixGraphics.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $matrixFormat=[System.Drawing.StringFormat]::GenericTypographic.Clone()
    $matrixGraphics.DrawString([string]$matrixChar,$matrixFont,[System.Drawing.Brushes]::White,4,1,$matrixFormat)
    $matrixBytes=New-Object System.Collections.Generic.List[string]
    for($matrixY=0;$matrixY -lt 32;$matrixY++) {
        for($matrixX=0;$matrixX -lt 32;$matrixX+=2) {
            $matrixA=[int][Math]::Round($matrixBitmap.GetPixel($matrixX,$matrixY).A/17)
            $matrixB=[int][Math]::Round($matrixBitmap.GetPixel($matrixX+1,$matrixY).A/17)
            $matrixBytes.Add([string](($matrixA -shl 4) -bor $matrixB))
        }
    }
    $matrixData.Add('{'+($matrixBytes -join ',')+'},')
    $matrixGraphics.Dispose(); $matrixBitmap.Dispose(); $matrixFormat.Dispose()
}
$matrixData.Add('};')
[IO.File]::WriteAllLines((Join-Path $PSScriptRoot '../graphics/matrix_katakana.h'),$matrixData)
$matrixFont.Dispose(); $matrixFontCollection.Dispose()
