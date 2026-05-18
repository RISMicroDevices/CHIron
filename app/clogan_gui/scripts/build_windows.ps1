[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release",
    [string]$BuildDir = "build-windows",
    [string]$Generator = "",
    [string]$QtRoot = "",
    [string]$Qt6Dir = "",
    [switch]$DeployQt,
    [switch]$NoDeployQt,
    [switch]$NoBenchmarks,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $PSCommandPath
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptDir ".."))

function Resolve-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $PathValue))
}

function Find-CommandPath {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names
    )

    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }
    }

    return $null
}

function Resolve-QtPrefixFromQt6Dir {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Qt6DirPath
    )

    $resolvedPath = [System.IO.Path]::GetFullPath($Qt6DirPath)

    for ($i = 0; $i -lt 3; $i++) {
        $parentPath = Split-Path -Parent $resolvedPath
        if (-not $parentPath -or $parentPath -eq $resolvedPath) {
            return $null
        }

        $resolvedPath = $parentPath
    }

    return $resolvedPath
}

function Find-QtToolPath {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names,
        [string]$QtRootPath,
        [string]$Qt6DirPath
    )

    $commandPath = Find-CommandPath -Names $Names
    if ($commandPath) {
        return $commandPath
    }

    $candidateRoots = @()

    if ($QtRootPath) {
        $candidateRoots += [System.IO.Path]::GetFullPath($QtRootPath)
    }

    if ($Qt6DirPath) {
        $qtPrefix = Resolve-QtPrefixFromQt6Dir -Qt6DirPath $Qt6DirPath
        if ($qtPrefix) {
            $candidateRoots += $qtPrefix
        }
    }

    foreach ($candidateRoot in ($candidateRoots | Select-Object -Unique)) {
        $binDir = Join-Path $candidateRoot "bin"
        if (-not (Test-Path $binDir)) {
            continue
        }

        foreach ($name in $Names) {
            foreach ($candidateName in @($name, "$name.exe")) {
                $candidatePath = Join-Path $binDir $candidateName
                if (Test-Path $candidatePath) {
                    return [System.IO.Path]::GetFullPath($candidatePath)
                }
            }
        }
    }

    return $null
}

function Find-CMakePath {
    param(
        [string]$QtRootPath,
        [string]$Qt6DirPath
    )

    $commandPath = Find-CommandPath -Names @("cmake")
    if ($commandPath) {
        return $commandPath
    }

    $candidateRoots = @()

    if ($QtRootPath) {
        $candidateRoots += [System.IO.Path]::GetFullPath($QtRootPath)
    }

    if ($Qt6DirPath) {
        $qtPrefix = Resolve-QtPrefixFromQt6Dir -Qt6DirPath $Qt6DirPath
        if ($qtPrefix) {
            $candidateRoots += $qtPrefix
        }
    }

    foreach ($candidateRoot in ($candidateRoots | Select-Object -Unique)) {
        $qtBaseDir = Split-Path (Split-Path $candidateRoot -Parent) -Parent
        if (-not $qtBaseDir) {
            continue
        }

        foreach ($relativePath in @(
            "Tools\CMake_64\bin\cmake.exe",
            "Tools\CMake\bin\cmake.exe"
        )) {
            $candidatePath = Join-Path $qtBaseDir $relativePath
            if (Test-Path $candidatePath) {
                return [System.IO.Path]::GetFullPath($candidatePath)
            }
        }
    }

    foreach ($candidatePath in @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\CMake\bin\cmake.exe"
    )) {
        if (Test-Path $candidatePath) {
            return [System.IO.Path]::GetFullPath($candidatePath)
        }
    }

    return $null
}

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CacheFile,
        [Parameter(Mandatory = $true)]
        [string]$VariableName
    )

    if (-not (Test-Path $CacheFile)) {
        return $null
    }

    $pattern = "^{0}:[^=]*=(.*)$" -f [Regex]::Escape($VariableName)
    $entry = Select-String -Path $CacheFile -Pattern $pattern | Select-Object -First 1

    if ($entry) {
        return $entry.Matches[0].Groups[1].Value.Trim()
    }

    return $null
}

function Resolve-BuiltExecutable {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDirectory,
        [Parameter(Mandatory = $true)]
        [string]$BuildConfiguration,
        [Parameter(Mandatory = $true)]
        [string]$TargetName
    )

    $candidatePaths = @(
        (Join-Path (Join-Path $BuildDirectory "release") "$TargetName.exe"),
        (Join-Path $BuildDirectory "$TargetName.exe"),
        (Join-Path (Join-Path $BuildDirectory $BuildConfiguration) "$TargetName.exe")
    )

    foreach ($candidatePath in $candidatePaths) {
        if (Test-Path $candidatePath) {
            return [System.IO.Path]::GetFullPath($candidatePath)
        }
    }

    $fallbackMatch = Get-ChildItem -Path $BuildDirectory -Filter "$TargetName.exe" -Recurse -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1

    if ($fallbackMatch) {
        return $fallbackMatch.FullName
    }

    throw "Built executable '$TargetName.exe' was not found under '$BuildDirectory'."
}

function Invoke-CheckedCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Executable,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    Write-Host ""
    Write-Host "> $Executable $($Arguments -join ' ')"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE."
    }
}

function Invoke-QtDeployment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExecutablePath,
        [Parameter(Mandatory = $true)]
        [string]$BuildConfiguration,
        [string]$QtRootPath,
        [string]$Qt6DirPath
    )

    $windeployqtPath = Find-QtToolPath -Names @("windeployqt6", "windeployqt") -QtRootPath $QtRootPath -Qt6DirPath $Qt6DirPath
    $qtpathsPath = Find-QtToolPath -Names @("qtpaths6", "qtpaths") -QtRootPath $QtRootPath -Qt6DirPath $Qt6DirPath

    if (-not $windeployqtPath -or -not $qtpathsPath) {
        throw "Qt deployment tools were not found. Pass -QtRoot or -Qt6Dir for a Qt installation that provides windeployqt and qtpaths."
    }

    $deployMode = "release"
    if ($BuildConfiguration -eq "Debug") {
        $resolvedQtRoot = $QtRootPath
        if (-not $resolvedQtRoot -and $Qt6DirPath) {
            $resolvedQtRoot = Resolve-QtPrefixFromQt6Dir -Qt6DirPath $Qt6DirPath
        }

        $hasDebugQtRuntime = $false
        if ($resolvedQtRoot) {
            $hasDebugQtRuntime =
                (Test-Path (Join-Path $resolvedQtRoot "bin\Qt6Cored.dll")) -and
                (Test-Path (Join-Path $resolvedQtRoot "bin\Qt6Guid.dll")) -and
                (Test-Path (Join-Path $resolvedQtRoot "bin\Qt6Widgetsd.dll")) -and
                (Test-Path (Join-Path $resolvedQtRoot "plugins\platforms\qwindowsd.dll"))
        }

        if ($hasDebugQtRuntime) {
            $deployMode = "debug"
        } else {
            Write-Warning "The selected Qt installation does not provide debug runtime/plugins. Deploying release Qt runtime for the Debug build."
        }
    }

    $deployArgs = @(
        "--qtpaths", $qtpathsPath,
        "--dir", (Split-Path -Parent $ExecutablePath),
        "--compiler-runtime"
    )

    if ($deployMode -eq "debug") {
        $deployArgs += "--debug"
    } else {
        $deployArgs += "--release"
    }

    $deployArgs += $ExecutablePath

    Invoke-CheckedCommand -Executable $windeployqtPath -Arguments $deployArgs
}

function Assert-QtDeployment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExecutablePath,
        [Parameter(Mandatory = $true)]
        [string]$BuildDirectory
    )

    $exeDir = Split-Path -Parent $ExecutablePath
    $cacheFile = Join-Path $BuildDirectory "CMakeCache.txt"
    $compilerPath = Get-CMakeCacheValue -CacheFile $cacheFile -VariableName "CMAKE_CXX_COMPILER"
    $generatorName = Get-CMakeCacheValue -CacheFile $cacheFile -VariableName "CMAKE_GENERATOR"
    $missingPaths = @()

    $qtRuntimeCandidates = @(
        @("Qt6Core.dll", "Qt6Cored.dll"),
        @("Qt6Gui.dll", "Qt6Guid.dll"),
        @("Qt6Widgets.dll", "Qt6Widgetsd.dll"),
        @("platforms\\qwindows.dll", "platforms\\qwindowsd.dll")
    )
    foreach ($candidateGroup in $qtRuntimeCandidates) {
        $groupSatisfied = $false
        foreach ($relativePath in $candidateGroup) {
            if (Test-Path (Join-Path $exeDir $relativePath)) {
                $groupSatisfied = $true
                break
            }
        }

        if (-not $groupSatisfied) {
            $missingPaths += ($candidateGroup -join " | ")
        }
    }

    $isMinGWBuild = $false
    if ($compilerPath -match "mingw" -or $compilerPath -match "g\+\+(\.exe)?$" -or $generatorName -match "MinGW") {
        $isMinGWBuild = $true
    }

    if ($isMinGWBuild) {
        foreach ($relativePath in @(
            "libstdc++-6.dll",
            "libwinpthread-1.dll"
        )) {
            if (-not (Test-Path (Join-Path $exeDir $relativePath))) {
                $missingPaths += $relativePath
            }
        }

        $gccRuntime = Get-ChildItem -Path $exeDir -Filter "libgcc_s*-1.dll" -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $gccRuntime) {
            $missingPaths += "libgcc_s*-1.dll"
        }
    }

    if ($missingPaths.Count -gt 0) {
        throw "Qt deployment is incomplete. Missing runtime files under '$exeDir': $($missingPaths -join ', ')"
    }
}

function Remove-LegacyReleaseArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDirectory
    )

    foreach ($directoryName in @(
        "generic",
        "iconengines",
        "imageformats",
        "networkinformation",
        "platforms",
        "styles",
        "tls",
        "translations"
    )) {
        $directoryPath = Join-Path $BuildDirectory $directoryName
        if (Test-Path $directoryPath) {
            Remove-Item -LiteralPath $directoryPath -Recurse -Force
        }
    }

    foreach ($filePattern in @(
        "CloganGUI.exe",
        "CloganGUILoaderBench.exe",
        "Qt6*.dll",
        "D3Dcompiler_47.dll",
        "opengl32sw.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll",
        "libgcc_s*-1.dll",
        "qt.conf"
    )) {
        Get-ChildItem -Path $BuildDirectory -Filter $filePattern -File -ErrorAction SilentlyContinue |
            Remove-Item -Force
    }

    foreach ($platformDir in @("x64", "x86")) {
        $platformRoot = Join-Path $BuildDirectory $platformDir
        foreach ($subDir in @("bin", "lib")) {
            $artifactDir = Join-Path $platformRoot $subDir
            if (Test-Path $artifactDir) {
                Remove-Item -LiteralPath $artifactDir -Recurse -Force
            }
        }

        if ((Test-Path $platformRoot) -and -not (Get-ChildItem -LiteralPath $platformRoot -Force | Select-Object -First 1)) {
            Remove-Item -LiteralPath $platformRoot -Force
        }
    }
}

$cmakePathFromEnvironment = Find-CommandPath -Names @("cmake")
$resolvedBuildDir = Resolve-FullPath -BasePath $projectRoot -PathValue $BuildDir
$resolvedQtRoot = $null
$resolvedQt6Dir = $null

if ($QtRoot) {
    $resolvedQtRoot = Resolve-FullPath -BasePath $projectRoot -PathValue $QtRoot
}

if ($Qt6Dir) {
    $resolvedQt6Dir = Resolve-FullPath -BasePath $projectRoot -PathValue $Qt6Dir
}

if ($DeployQt -and $NoDeployQt) {
    throw "Pass either -DeployQt or -NoDeployQt, not both."
}

if (-not $resolvedQtRoot -and $resolvedQt6Dir) {
    $resolvedQtRoot = Resolve-QtPrefixFromQt6Dir -Qt6DirPath $resolvedQt6Dir
}

if (-not $resolvedQtRoot -and -not $resolvedQt6Dir) {
    $qtpaths = Find-CommandPath -Names @("qtpaths6", "qtpaths")
    if ($qtpaths) {
        $detectedQtRoot = & $qtpaths --query QT_INSTALL_PREFIX 2>$null | Select-Object -First 1
        if ($detectedQtRoot) {
            $resolvedQtRoot = [System.IO.Path]::GetFullPath($detectedQtRoot.Trim())
        }
    }
}

if (-not $resolvedQtRoot -and -not $resolvedQt6Dir) {
    $qmake = Find-CommandPath -Names @("qmake6", "qmake")
    if ($qmake) {
        $detectedQtRoot = & $qmake -query QT_INSTALL_PREFIX 2>$null | Select-Object -First 1
        if ($detectedQtRoot) {
            $resolvedQtRoot = [System.IO.Path]::GetFullPath($detectedQtRoot.Trim())
        }
    }
}

if ($resolvedQtRoot) {
    $qtBinDir = Join-Path $resolvedQtRoot "bin"
    if (Test-Path $qtBinDir) {
        $env:PATH = "$qtBinDir;$env:PATH"
    }

    $qtBaseDir = Split-Path (Split-Path $resolvedQtRoot -Parent) -Parent
    if ($qtBaseDir) {
        foreach ($cmakeToolsSubdir in @("Tools\CMake_64\bin", "Tools\CMake\bin")) {
            $cmakeBinDir = Join-Path $qtBaseDir $cmakeToolsSubdir
            if (Test-Path $cmakeBinDir) {
                $env:PATH = "$cmakeBinDir;$env:PATH"
            }
        }

        $mingwToolDir = Get-ChildItem (Join-Path $qtBaseDir "Tools") -Directory -Filter "mingw*" -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1
        if ($mingwToolDir) {
            $mingwBinDir = Join-Path $mingwToolDir.FullName "bin"
            if (Test-Path $mingwBinDir) {
                $env:PATH = "$mingwBinDir;$env:PATH"
            }
        }
    }
}

if (-not $resolvedQt6Dir -and $resolvedQtRoot) {
    $candidateQt6Dir = Join-Path $resolvedQtRoot "lib\cmake\Qt6"
    if (Test-Path (Join-Path $candidateQt6Dir "Qt6Config.cmake")) {
        $resolvedQt6Dir = $candidateQt6Dir
    }
}

$cmakePath = Find-CMakePath -QtRootPath $resolvedQtRoot -Qt6DirPath $resolvedQt6Dir
if (-not $cmakePath) {
    throw "cmake was not found. Install CMake or pass -QtRoot for a Qt Online Installer layout that includes CMake."
}

if (-not $cmakePathFromEnvironment) {
    Write-Warning "cmake was not found in PATH."
    Write-Host "Using fallback CMake: $cmakePath"
}

if (-not $Generator) {
    if (Find-CommandPath -Names @("ninja")) {
        $Generator = "Ninja"
    } elseif (Find-CommandPath -Names @("mingw32-make")) {
        $Generator = "MinGW Makefiles"
    }
}

if ($Clean -and (Test-Path $resolvedBuildDir)) {
    Remove-Item -LiteralPath $resolvedBuildDir -Recurse -Force
}

$benchmarksValue = if ($NoBenchmarks) { "OFF" } else { "ON" }
$shouldDeployQt = -not $NoDeployQt
$releaseOutputDir = Join-Path $resolvedBuildDir "release"

$configureArgs = @(
    "-S", $projectRoot,
    "-B", $resolvedBuildDir,
    "-D", "CMAKE_BUILD_TYPE=$BuildType",
    "-D", "CHIRON_GUI_BUILD_BENCHMARKS=$benchmarksValue",
    "-D", "CHIRON_GUI_ENABLE_WINDEPLOYQT=OFF",
    "-D", "CHIRON_GUI_OUTPUT_DIRECTORY=$releaseOutputDir"
)

if ($Generator) {
    $configureArgs += @("-G", $Generator)
}

if ($resolvedQt6Dir) {
    $configureArgs += @("-D", "Qt6_DIR=$resolvedQt6Dir")
} elseif ($resolvedQtRoot) {
    $configureArgs += @("-D", "CMAKE_PREFIX_PATH=$resolvedQtRoot")
}

Invoke-CheckedCommand -Executable $cmakePath -Arguments $configureArgs
Invoke-CheckedCommand -Executable $cmakePath -Arguments @(
    "--build", $resolvedBuildDir,
    "--target", "CloganGUI",
    "--config", $BuildType,
    "--parallel"
)

$builtExecutable = Resolve-BuiltExecutable -BuildDirectory $resolvedBuildDir -BuildConfiguration $BuildType -TargetName "CloganGUI"

if ($shouldDeployQt) {
    Invoke-QtDeployment -ExecutablePath $builtExecutable -BuildConfiguration $BuildType -QtRootPath $resolvedQtRoot -Qt6DirPath $resolvedQt6Dir
    Assert-QtDeployment -ExecutablePath $builtExecutable -BuildDirectory $resolvedBuildDir
}

Remove-LegacyReleaseArtifacts -BuildDirectory $resolvedBuildDir

Write-Host ""
Write-Host "Build complete: $builtExecutable"
