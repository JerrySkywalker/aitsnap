# SPDX-License-Identifier: Apache-2.0
# Synthetic-only interactive STA feasibility probe. Never opens an existing report.
param([Parameter(Mandatory=$true)][string]$OutputRoot)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
function Get-ProbeSha([string]$path) {
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hasher.ComputeHash([IO.File]::ReadAllBytes($path))).Replace('-','') }
    finally { $hasher.Dispose() }
}
if ([Threading.Thread]::CurrentThread.ApartmentState -ne 'STA') { throw 'Run with powershell.exe -NoProfile -STA -File' }
$programType = [Type]::GetTypeFromProgID('Word.Application')
if (-not $programType) { Write-Output 'WORD_COM_HOLD: Word.Application is not registered'; exit 2 }
$runDirectory = Join-Path ([IO.Path]::GetFullPath($OutputRoot)) ([Guid]::NewGuid().ToString())
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$documentPath = Join-Path $runDirectory 'synthetic-r02.docx'
$picturePath = Join-Path $runDirectory 'synthetic-pattern.png'
$markerName = 'AITSnap_R02_' + [Guid]::NewGuid().ToString('N')
function Write-ProbeStage([string]$stage) {
    [IO.File]::AppendAllText((Join-Path $runDirectory 'stages.log'),([DateTime]::UtcNow.ToString('o') + ' ' + $stage + [Environment]::NewLine))
    Write-Output ('WORD_STAGE=' + $stage)
}
$word = $null; $document = $null
try {
    Add-Type -AssemblyName System.Drawing
    $bitmap = New-Object Drawing.Bitmap(320,160)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([Drawing.Color]::Navy)
        $graphics.FillRectangle([Drawing.Brushes]::Lime,160,0,160,160)
        $bitmap.Save($picturePath,[Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
    $sourceHash = Get-ProbeSha $picturePath
    Write-ProbeStage 'creating-application'
    $word = [Activator]::CreateInstance($programType)
    Write-ProbeStage 'application-created'
    $word.Visible = $false
    Write-ProbeStage 'visible-false'
    $word.DisplayAlerts = 0
    Write-ProbeStage 'alerts-disabled'
    $version = [string]$word.Version
    Write-ProbeStage ('version-' + $version)
    $document = $word.Documents.Add()
    Write-ProbeStage 'document-added'
    $document.Content.InsertAfter("Synthetic AITSnap R02 report`r")
    for ($ordinal=1; $ordinal -le 2; $ordinal++) {
        $end = $document.Content.End - 1
        $range = $document.Range($end,$end)
        $range.InsertAfter("Synthetic figure $ordinal`r")
        $end = $document.Content.End - 1
        $imageRange = $document.Range($end,$end)
        $shape = $document.InlineShapes.AddPicture($picturePath,$false,$true,$imageRange)
        Write-ProbeStage ('picture-' + $ordinal)
        $shape.LockAspectRatio = -1
        $maxWidth = $document.PageSetup.PageWidth - $document.PageSetup.LeftMargin - $document.PageSetup.RightMargin
        if ($shape.Width -gt $maxWidth) { $shape.Width = $maxWidth }
        $end = $document.Content.End - 1
        $document.Range($end,$end).InsertAfter("`rSynthetic caption $ordinal; no operational data.`r")
    }
    $document.Variables.Add($markerName,'COMMITTED') | Out-Null
    if (Test-Path -LiteralPath $documentPath) { throw 'No-overwrite probe guard' }
    $document.SaveAs2($documentPath,16)
    Write-ProbeStage 'document-saved'
    $document.Close(0); $document = $null
    $document = $word.Documents.Open($documentPath,$false,$false,$false)
    Write-ProbeStage 'document-reopened'
    $countBefore = $document.InlineShapes.Count
    $found = $false
    foreach ($variable in $document.Variables) {
        if ($variable.Name -eq $markerName -and $variable.Value -eq 'COMMITTED') { $found = $true }
    }
    if (-not $found -or $countBefore -ne 2) { throw 'Reopen image count/marker verification failed' }
    # Retry sees the saved marker and performs no insertion.
    $countAfterRetry = $document.InlineShapes.Count
    if ($countAfterRetry -ne 2) { throw 'Duplicate insertion detected' }
    $text = [string]$document.Content.Text
    if ($text.IndexOf('Synthetic figure 1') -ge $text.IndexOf('Synthetic figure 2')) { throw 'Caption order failed' }
    if ((Get-ProbeSha $picturePath) -ne $sourceHash) { throw 'Original image changed' }
    $receipt = [ordered]@{ status='PASS'; wordVersion=$version; inlineShapes=$countAfterRetry; markerReopened=$found; retryInserted=0; originalSha256=$sourceHash; document=$documentPath }
    [IO.File]::WriteAllText((Join-Path $runDirectory 'receipt.json'),($receipt | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
    Write-Output ('WORD_SYNTHETIC_COM_PASS ' + ($receipt | ConvertTo-Json -Compress))
} catch {
    Write-Output ('WORD_COM_FAIL_OR_HOLD ' + $_.Exception.Message)
    throw
} finally {
    if ($document) { $document.Close(0) }
    if ($word) { $word.Quit(0); [Runtime.InteropServices.Marshal]::FinalReleaseComObject($word) | Out-Null }
}
