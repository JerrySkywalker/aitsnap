$ErrorActionPreference = 'Stop'
$targetRoot = [IO.Path]::GetFullPath($PSScriptRoot)
if (Test-Path -LiteralPath (Join-Path $targetRoot 'msi-install.json')) { throw 'This directory is managed by Windows Installer. Uninstall AITSnap Preview through Windows Apps/Programs; do not remove its files with the portable script.' }
$manifestPath = Join-Path $targetRoot 'package-manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.product -ne 'AITSnap modern local preview') { throw 'Unrecognized installation manifest' }
$directories = @()
foreach ($entry in $manifest.files) {
    $target = [IO.Path]::GetFullPath((Join-Path $targetRoot $entry.path))
    if (!$target.StartsWith($targetRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid manifest path' }
    $parent = [IO.Path]::GetDirectoryName($target)
    for ($cursor=$parent; $cursor.Length -ge $targetRoot.Length; $cursor=[IO.Path]::GetDirectoryName($cursor)) {
        if ((Get-Item -LiteralPath $cursor).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Refusing redirected installation paths' }
        if ($cursor -eq $targetRoot) { break }
        $directories += $cursor
    }
    if ((Test-Path -LiteralPath $target) -and (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -eq $entry.sha256) { Remove-Item -LiteralPath $target }
}
Remove-Item -LiteralPath $manifestPath
foreach ($directory in ($directories | Sort-Object Length -Descending -Unique)) {
    if ((Test-Path -LiteralPath $directory) -and !(Get-ChildItem -LiteralPath $directory -Force | Select-Object -First 1)) { Remove-Item -LiteralPath $directory }
}
Write-Output 'Known unchanged package files removed. User data, unknown files and modified files preserved.'
