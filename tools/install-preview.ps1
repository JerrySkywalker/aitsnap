param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$sourceRoot = $PSScriptRoot
$targetRoot = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $targetRoot) { throw 'Choose a new destination directory; existing content is never replaced.' }
$manifest = Get-Content -LiteralPath (Join-Path $sourceRoot 'package-manifest.json') -Raw | ConvertFrom-Json
foreach ($entry in $manifest.files) {
    $source = [IO.Path]::GetFullPath((Join-Path $sourceRoot $entry.path))
    if (!$source.StartsWith($sourceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package path' }
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $entry.sha256) { throw "Package hash mismatch: $($entry.path)" }
}
New-Item -ItemType Directory -Path $targetRoot | Out-Null
foreach ($entry in $manifest.files) {
    $target = Join-Path $targetRoot $entry.path
    New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($target)) | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRoot $entry.path) -Destination $target
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'package-manifest.json') -Destination $targetRoot
Write-Output "Installed local preview: $targetRoot"
Write-Output 'Run aitsnap.exe. Settings and evidence remain outside the installation unless --data-dir is explicitly supplied.'
