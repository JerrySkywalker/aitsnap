$ErrorActionPreference = 'Stop'
$previewData = Join-Path $env:LOCALAPPDATA 'AITSnap-preview-0.6'
Start-Process -FilePath (Join-Path $PSScriptRoot 'aitsnap.exe') -ArgumentList '--data-dir', ('"' + $previewData + '"') -WindowStyle Normal
