param([switch]$CoreOnly, [string]$CyUI = '', [string]$Configuration = 'Release',
      [switch]$BuildTests, [switch]$RunTests, [switch]$NoInstall, [switch]$Plan,
      [string]$TestFilter = '')
$buildArguments = @((Join-Path $PSScriptRoot 'build.py'), '--configuration', $Configuration)
if ($CoreOnly) { $buildArguments += '--core-only' }
if ($CyUI) { $buildArguments += @('--cyui', $CyUI) }
if ($BuildTests) { $buildArguments += '--build-tests' }
if ($RunTests) { $buildArguments += '--run-tests' }
if ($NoInstall) { $buildArguments += '--no-install' }
if ($Plan) { $buildArguments += '--plan' }
if ($TestFilter) { $buildArguments += @('--test-filter', $TestFilter) }
& python @buildArguments
exit $LASTEXITCODE
