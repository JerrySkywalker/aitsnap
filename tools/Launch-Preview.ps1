$ErrorActionPreference = 'Stop'
$previewData = Join-Path $env:LOCALAPPDATA 'AITSnap-preview-0.8'
Start-Process -FilePath (Join-Path $PSScriptRoot 'aitsnap.exe') -ArgumentList '--show', '--data-dir', ('"' + $previewData + '"') -WindowStyle Normal
