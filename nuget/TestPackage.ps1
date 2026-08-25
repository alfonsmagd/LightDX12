[CmdletBinding()]
param(
    [ValidatePattern( '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$' )]
    [string] $Version = '0.1.0-local',
    [string] $NuGetExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$packageDirectory = Join-Path $repositoryRoot 'packages'
$testBuildDirectory = Join-Path $repositoryRoot 'build\nuget-consumer'
$packageInstallDirectory = Join-Path $testBuildDirectory 'packages'
$testProject = Join-Path $repositoryRoot 'tests\nuget\Ldx12NuGetConsumer.vcxproj'

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

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if( -not ( Test-Path -LiteralPath $vswhere -PathType Leaf ) )
{
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if( [string]::IsNullOrWhiteSpace( $msbuild ) )
{
    throw 'MSBuild.exe was not found in a Visual Studio installation.'
}

New-Item -ItemType Directory -Force -Path $packageInstallDirectory | Out-Null
Invoke-Checked $NuGetExe @(
    'install', 'Ldx12',
    '-Version', $Version,
    '-Source', $packageDirectory,
    '-OutputDirectory', $packageInstallDirectory,
    '-DirectDownload',
    '-NoHttpCache',
    '-NonInteractive'
)

$extractedPackage = Join-Path $packageInstallDirectory "Ldx12.$Version"
if( -not ( Test-Path -LiteralPath $extractedPackage -PathType Container ) )
{
    throw "The extracted package was not found at '$extractedPackage'."
}

foreach( $configuration in @( 'Debug', 'Release' ) )
{
    Invoke-Checked $msbuild @(
        $testProject,
        '/m',
        '/t:Rebuild',
        "/p:Configuration=$configuration",
        '/p:Platform=x64',
        "/p:Ldx12ExtractedPackageRoot=$extractedPackage",
        '/verbosity:minimal'
    )

    $executable = Join-Path $testBuildDirectory "bin\$configuration\Ldx12NuGetConsumer.exe"
    if( -not ( Test-Path -LiteralPath $executable -PathType Leaf ) )
    {
        throw "The $configuration NuGet consumer executable was not generated."
    }
    Invoke-Checked $executable @()
}

Write-Host 'The Ldx12 NuGet package passed the Visual Studio 2022 Debug and Release consumer tests.'
