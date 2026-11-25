[CmdletBinding()]
param([switch]$FixFormat, [switch]$Install, [switch]$FormatOnly)
$ErrorActionPreference = 'Stop'
$qualityRoot = $PSScriptRoot
$qualityDeps = Join-Path $qualityRoot 'dependencies/quality'
$qualityFormat = Join-Path $qualityDeps 'python/clang_format/data/bin/clang-format.exe'
$qualityCheck = Join-Path $qualityDeps 'cppcheck/PFiles/Cppcheck/cppcheck.exe'
if ($Install) {
    New-Item -ItemType Directory -Force -Path $qualityDeps | Out-Null
    # Do not inherit unrelated packet-capture logging configuration.
    $qualityOldKeyLog = $env:SSLKEYLOGFILE
    try {
        $env:SSLKEYLOGFILE = ''
        & python -m pip install --target (Join-Path $qualityDeps 'python') clang-format==20.1.8 --upgrade --disable-pip-version-check
        if ($LASTEXITCODE) { throw 'Cannot install clang-format 20.1.8' }
        $qualityMsi = Join-Path $qualityDeps 'cppcheck.msi'
        Invoke-WebRequest -UseBasicParsing 'https://github.com/cppcheck-opensource/cppcheck/releases/download/2.18.0/cppcheck-2.18.0-x64-Setup.msi' -OutFile $qualityMsi
        if ((Get-FileHash $qualityMsi -Algorithm SHA256).Hash -ne '651C14395B8F8B2F18B7C08FF27533142ECAFD60B6BE0DC123F92FBD873CB638') { throw 'Cppcheck installer hash mismatch' }
        $qualityDestination = Join-Path $qualityDeps 'cppcheck'
        $qualityProcess = Start-Process msiexec.exe -ArgumentList @('/a', "`"$qualityMsi`"", '/qn', "TARGETDIR=`"$qualityDestination`"") -WindowStyle Hidden -Wait -PassThru
        if ($qualityProcess.ExitCode) { throw 'Cannot extract Cppcheck 2.18.0' }
    } finally { $env:SSLKEYLOGFILE = $qualityOldKeyLog }
}
if (!(Test-Path $qualityFormat) -or !(Test-Path $qualityCheck)) { throw 'Run .\quality.ps1 -Install to obtain the pinned development tools.' }
if ((& $qualityFormat --version) -notmatch '20\.1\.8\b') { throw 'Expected clang-format 20.1.8' }
if ((& $qualityCheck --version) -notmatch '2\.18\.0\b') { throw 'Expected Cppcheck 2.18.0' }
Push-Location $qualityRoot
try {
    $qualityPaths = @(& git ls-files --cached --others --exclude-standard)
    if ($LASTEXITCODE) { throw 'Cannot enumerate source files' }
    $qualityFiles = @($qualityPaths | Where-Object { $_ -match '^(accounts|builds|catalog|client|common|inject32|launcher|server)/.*\.(c|cpp|h|hpp|inl)$' } | Sort-Object -Unique)
    if (!$qualityFiles.Count) { throw 'No first-party C/C++ files found' }
    $qualityBuild = Join-Path $qualityRoot 'build/quality'
    New-Item -ItemType Directory -Force -Path $qualityBuild | Out-Null
    $qualityFormatLog = Join-Path $qualityBuild 'format.log'
    $qualityFormatArgs = @('--style=file', '--fallback-style=none')
    if ($FixFormat) { $qualityFormatArgs += '-i' } else { $qualityFormatArgs += @('--dry-run', '--Werror') }
    # Limit argument size on Windows.
    for ($qualityOffset = 0; $qualityOffset -lt $qualityFiles.Count; $qualityOffset += 40) {
        $qualityLast = [Math]::Min($qualityOffset + 39, $qualityFiles.Count - 1)
        $ErrorActionPreference = 'Continue'
        & $qualityFormat @qualityFormatArgs $qualityFiles[$qualityOffset..$qualityLast] 2> $qualityFormatLog
        $qualityFormatExit = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        if ($qualityFormatExit) { Get-Content $qualityFormatLog -TotalCount 30; throw 'C/C++ formatting failed; run .\quality.ps1 -FixFormat' }
    }
    Write-Host "Format OK: $($qualityFiles.Count) first-party files"
    if ($FormatOnly) { return }
    foreach ($qualityPlatform in @('Win32', 'x64')) {
        $qualityModel = if ($qualityPlatform -eq 'Win32') { 'win32W' } else { 'win64' }
        $qualityArchitecture = if ($qualityPlatform -eq 'Win32') { '-D_M_IX86=600' } else { '-D_M_X64=100' }
        foreach ($qualityProject in @('GlideAholic.sln')) {
            $qualityName = [IO.Path]::GetFileNameWithoutExtension($qualityProject)
            $qualityLog = Join-Path $qualityBuild "$qualityName-$qualityPlatform.log"
            $qualityCache = Join-Path $qualityBuild "$qualityName-$qualityPlatform-cache"
            New-Item -ItemType Directory -Force -Path $qualityCache | Out-Null
            & $qualityCheck "--project=$qualityProject" "--project-configuration=Release|$qualityPlatform" "--platform=$qualityModel" --std=c++14 $qualityArchitecture --library=windows --enable=warning,performance,portability --inline-suppr --suppress=normalCheckLevelMaxBranches --error-exitcode=1 -ivendor -j2 "--cppcheck-build-dir=$qualityCache" --quiet "--output-file=$qualityLog"
            if ($LASTEXITCODE) { Get-Content $qualityLog -TotalCount 60; throw "Cppcheck failed: $qualityProject ($qualityPlatform)" }
            Write-Host "Cppcheck OK: $qualityProject ($qualityPlatform)"
        }
    }
} finally { Pop-Location }
