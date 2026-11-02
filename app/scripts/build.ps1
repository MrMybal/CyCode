param([switch]$CoreOnly, [string]$CyUI = '', [string]$Configuration = 'Release')
$buildArguments = @((Join-Path $PSScriptRoot 'build.py'), '--configuration', $Configuration)
if ($CoreOnly) { $buildArguments += '--core-only' }
if ($CyUI) { $buildArguments += @('--cyui', $CyUI) }
& python @buildArguments
exit $LASTEXITCODE
