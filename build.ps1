# Builds the Nexus version of GW2Radial (Release x64) with MSBuild + a standalone vcpkg.
#   .\build.ps1                      -> bin\x64\Release\GW2Radial.dll
#   .\build.ps1 -GameDir "D:\GW2"    -> also copies the dll to <GameDir>\addons
param(
    [string]$Configuration = "Release",
    [string]$VcpkgRoot = (Join-Path $PSScriptRoot "..\vcpkg"),
    [string]$GameDir
)

$ErrorActionPreference = "Stop"
$VcpkgRoot = (Resolve-Path $VcpkgRoot).Path
$env:VCPKG_ROOT = $VcpkgRoot
$env:VCPKG_DISABLE_METRICS = "1"

$msbuild = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\amd64\MSBuild.exe" | Select-Object -First 1
}

Push-Location $PSScriptRoot
try {
    # Dependencies from vcpkg.json (static triplet, matches VcpkgUseStatic in the projects).
    & "$VcpkgRoot\vcpkg.exe" install --triplet x64-windows-static --x-install-root=vcpkg_installed
    if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed" }

    # The vcpkg MSBuild integration is not auto-loaded with a standalone vcpkg, so import it explicitly.
    & $msbuild GW2Radial.sln "/p:Configuration=$Configuration" /p:Platform=x64 /m /v:minimal /nologo `
        "/p:ForceImportAfterCppTargets=$VcpkgRoot\scripts\buildsystems\msbuild\vcpkg.targets" "/p:VcpkgRoot=$VcpkgRoot\"
    if ($LASTEXITCODE -ne 0) { throw "msbuild failed" }

    $dll = Join-Path $PSScriptRoot "bin\x64\$Configuration\GW2Radial.dll"
    Write-Host "Built $dll"

    if ($GameDir) {
        Copy-Item $dll (Join-Path $GameDir "addons\GW2Radial.dll") -Force
        Write-Host "Copied to $GameDir\addons"
    }
}
finally {
    Pop-Location
}
