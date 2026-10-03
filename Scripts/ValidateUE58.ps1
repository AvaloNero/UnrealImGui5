param(
    [Parameter(Mandatory = $true)][string]$EnginePath,
    [string]$ValidationRoot = (Join-Path $env:TEMP ('UnrealImGui5-UE58-' + [guid]::NewGuid().ToString('N'))),
    [switch]$SkipGPU
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$engineRoot = (Resolve-Path -LiteralPath $EnginePath).Path
$buildVersion = Get-Content -LiteralPath (Join-Path $engineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
if ($buildVersion.MajorVersion -ne 5 -or $buildVersion.MinorVersion -ne 8) { throw 'This validation fixture targets UE 5.8.' }
$validationPath = [System.IO.Path]::GetFullPath($ValidationRoot)
if (Test-Path -LiteralPath $validationPath) { throw 'Use a new validation directory to preserve earlier evidence.' }
if ($validationPath.StartsWith($repoRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Validation output must be outside the source checkout.'
}
New-Item -ItemType Directory -Path $validationPath | Out-Null
Copy-Item -Path (Join-Path $repoRoot 'Tests\UE58\*') -Destination $validationPath -Recurse
# Keep fixture rules as templates in the plugin so UBT cannot discover duplicate rules.
Get-ChildItem -LiteralPath (Join-Path $validationPath 'Source') -Filter '*.cs.in' -Recurse -File | ForEach-Object {
    Rename-Item -LiteralPath $_.FullName -NewName ([System.IO.Path]::GetFileNameWithoutExtension($_.Name))
}
New-Item -ItemType Directory -Path (Join-Path $validationPath 'Plugins') | Out-Null
New-Item -ItemType Junction -Path (Join-Path $validationPath 'Plugins\ImGui') -Target $repoRoot | Out-Null
$evidencePath = Join-Path $validationPath 'Saved\Validation'
New-Item -ItemType Directory -Path $evidencePath -Force | Out-Null
$projectPath = Join-Path $validationPath 'ImGuiValidation.uproject'
$buildTool = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

& $buildTool ImGuiValidationEditor Win64 Development "-Project=$projectPath" -UsePrecompiled -NoHotReloadFromIDE *> (Join-Path $evidencePath 'editor-build.log')
if ($LASTEXITCODE -ne 0) { throw "Editor build failed. See $evidencePath\editor-build.log" }

$renderModes = @('NullRHI')
if (!$SkipGPU) { $renderModes += 'D3D12' }
foreach ($renderMode in $renderModes) {
    $reportPath = Join-Path $evidencePath ('Automation-' + $renderMode)
    $logPath = Join-Path $evidencePath ('automation-' + $renderMode + '.log')
    $renderArguments = if ($renderMode -eq 'NullRHI') { @('-NullRHI') } else { @('-d3d12', '-RenderOffscreen') }
    & $editor $projectPath -unattended -nosplash -nosound @renderArguments '-ExecCmds=Automation RunTests ImGui.Integration' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportPath" "-abslog=$logPath" -stdout -FullStdOutLogOutput *> (Join-Path $evidencePath ('console-' + $renderMode + '.log'))
    if ($LASTEXITCODE -ne 0) { throw "Unreal exited with an error in $renderMode. See $logPath" }
    # TestExit can return zero even when tests fail; inspect the exported report as well.
    $report = Get-Content -LiteralPath (Join-Path $reportPath 'index.json') -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.succeeded -ne 6) { throw "Expected all 6 ImGui tests to pass in $renderMode. See $reportPath" }
    Write-Output "$renderMode : $($report.succeeded)/6 tests passed."
}

if (!$SkipGPU) {
    $smokeLog = Join-Path $evidencePath 'slate-smoke.log'
    & $editor $projectPath /Engine/Maps/Entry -game -d3d12 -RenderOffscreen -Windowed -ResX=1280 -ResY=720 -unattended -nosplash -nosound -ImGuiValidationSmoke "-abslog=$smokeLog" -stdout -FullStdOutLogOutput *> (Join-Path $evidencePath 'slate-smoke-console.log')
    if ($LASTEXITCODE -ne 0) { throw "Slate smoke run failed. See $smokeLog" }
    $screenshot = Join-Path $evidencePath 'UE58-ImGui-Slate.png'
    if (!(Test-Path -LiteralPath $screenshot)) { throw 'The Slate smoke run did not produce its screenshot.' }
    Write-Output "Inspect the Slate screenshot: $screenshot"
}
Write-Output "Validation evidence: $evidencePath"
