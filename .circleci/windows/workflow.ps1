# Runs a workflow preset in a developer command prompt for the MSVC toolset named: runs
# vcvarsall.bat for x64 at that toolset, takes on the variables it sets or changes, all but
# VCPKG_ROOT, and runs `cmake --workflow` with the CMake and Ninja in CI_TOOLS ahead of Visual
# Studio's own.
#
#   .circleci/windows/workflow.ps1 -MsvcVersion 14.52 -Preset windows-msvc-release
param(
    # The MSVC toolset, such as 14.52. vcvarsall.bat would otherwise set up the one Visual Studio
    # makes its default, which may be older than aplomb's floor.
    [Parameter(Mandatory)]
    [ValidatePattern('^14\.\d{2}$')]
    [string] $MsvcVersion,

    [Parameter(Mandatory)]
    [string] $Preset
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installs = @(& $vswhere -all -sort -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
$vs = $installs | Where-Object { Test-Path (Join-Path $_ "VC\Tools\MSVC\$MsvcVersion.*") } | Select-Object -First 1
if (-not $vs) {
    throw "no Visual Studio carries MSVC $MsvcVersion; looked in $($installs -join ', ')"
}
$vcvarsall = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'

$before = @{}
Get-ChildItem env: | ForEach-Object { $before[$_.Name] = $_.Value }

$lines = cmd /c "`"$vcvarsall`" x64 -vcvars_ver=$MsvcVersion && set"
if ($LASTEXITCODE -ne 0) {
    $lines | Write-Output
    throw "vcvarsall.bat couldn't set up MSVC $MsvcVersion"
}

$toolset = $null
foreach ($line in $lines) {
    if ($line -notmatch '^([^=]+)=(.*)$') {
        continue
    }
    $name, $value = $Matches[1], $Matches[2]
    if ($name -eq 'VCToolsVersion') {
        $toolset = $value
    }
    # vcvarsall.bat points VCPKG_ROOT at the vcpkg Visual Studio bundles, over the job's own.
    if ($name -eq 'VCPKG_ROOT') {
        continue
    }
    # The names of environment variables don't depend on case on Windows, and neither does this
    # table's lookup.
    if (-not $before.ContainsKey($name) -or $before[$name] -cne $value) {
        Set-Item -Path "env:$name" -Value $value
    }
}
if (-not $toolset -or $toolset -notlike "$MsvcVersion.*") {
    throw "vcvarsall.bat set up MSVC $toolset, not $MsvcVersion"
}
Write-Output "MSVC $toolset, from $vs"

$env:PATH = "$(Join-Path $env:CI_TOOLS 'Scripts');$env:PATH"
cmake --workflow --preset $Preset
