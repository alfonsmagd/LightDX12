[CmdletBinding()]
param(
    [ValidatePattern( '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$' )]
    [string] $Version = '0.1.0-local',
    [string] $LicenseExpression = 'UNLICENSED',
    [string] $NuGetExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$packageBuildDirectory = Join-Path $repositoryRoot 'build\nuget-package'
$installDirectory = Join-Path $repositoryRoot 'build\nuget-install'
$outputDirectory = Join-Path $repositoryRoot 'packages'
$nuspecPath = Join-Path $PSScriptRoot 'Ldx12.nuspec'

function Invoke-Checked( [string] $Program, [string[]] $Arguments )
{
    & $Program @Arguments
    if( $LASTEXITCODE -ne 0 )
    {
        throw "Command failed with exit code $LASTEXITCODE`: $Program $($Arguments -join ' ')"
    }
}

if( [string]::IsNullOrWhiteSpace( $NuGetExe ) )
{
    $nugetCommand = Get-Command nuget.exe -ErrorAction SilentlyContinue
    if( $null -ne $nugetCommand )
    {
        $NuGetExe = $nugetCommand.Source
    }
    else
    {
        $NuGetExe = Join-Path $repositoryRoot 'build\tools\nuget.exe'
    }
}

if( -not ( Test-Path -LiteralPath $NuGetExe -PathType Leaf ) )
{
    throw "NuGet.exe was not found. Pass -NuGetExe or place it at '$NuGetExe'."
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

Invoke-Checked 'cmake' @(
    '-S', $repositoryRoot,
    '-B', $packageBuildDirectory,
    '-A', 'x64',
    '-DLDX12_BUILD_APP=OFF',
    '-DLDX12_BUILD_EXAMPLES=OFF',
    '-DLDX12_BUILD_TESTS=OFF',
    '-DLDX12_INSTALL=ON'
)
Invoke-Checked 'cmake' @( '--build', $packageBuildDirectory, '--config', 'Debug', '--target', 'Ldx12', '--parallel' )
Invoke-Checked 'cmake' @( '--build', $packageBuildDirectory, '--config', 'Release', '--target', 'Ldx12', '--parallel' )
Invoke-Checked 'cmake' @( '--install', $packageBuildDirectory, '--config', 'Debug', '--prefix', $installDirectory )
Invoke-Checked 'cmake' @( '--install', $packageBuildDirectory, '--config', 'Release', '--prefix', $installDirectory )

Invoke-Checked $NuGetExe @(
    'pack', $nuspecPath,
    '-BasePath', $repositoryRoot,
    '-OutputDirectory', $outputDirectory,
    '-Version', $Version,
    '-Properties', "licenseExpression=$LicenseExpression",
    '-NonInteractive'
)

$packagePath = Join-Path $outputDirectory "Ldx12.$Version.nupkg"
if( -not ( Test-Path -LiteralPath $packagePath -PathType Leaf ) )
{
    throw "NuGet did not generate the expected package '$packagePath'."
}

Write-Host "NuGet package created: $packagePath"
