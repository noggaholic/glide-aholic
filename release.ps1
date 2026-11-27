param(
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,
    [switch]$CheckOnly,
    [switch]$VerifyPackages,
    [switch]$RefreshVerification,
    [switch]$Candidate
)
$ErrorActionPreference = 'Stop'
$releaseRoot = $PSScriptRoot
function AssertInstructionData([string[]]$Packages = @()) {
    $auditArguments = @((Join-Path $releaseRoot 'scripts/check_distribution.py'), '--source', $releaseRoot)
    foreach ($package in $Packages) { $auditArguments += @('--zip', $package) }
    & python @auditArguments
    if ($LASTEXITCODE) { throw 'Publication blocked: instruction-pattern audit failed.' }
}
AssertInstructionData
if ($CheckOnly -and $VerifyPackages) { throw 'Choose input checking or finished-package verification' }
if ($Candidate -and $Version -notmatch '-candidate(?:\.\d+)?$') { throw 'Local candidates require a -candidate version suffix' }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function FileHash([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function WriteZip([string]$Path, [string]$Root, [string[]]$Files, [string]$Prefix) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew)
    $archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($relative in $Files) {
            $entry = $archive.CreateEntry($Prefix + $relative)
            $output = $entry.Open()
            $input = [IO.File]::OpenRead((Join-Path $Root $relative))
            try { $input.CopyTo($output) } finally { $input.Dispose(); $output.Dispose() }
        }
    } finally { $archive.Dispose(); $stream.Dispose() }
}
function AssertZip([string]$Path, [hashtable]$Expected, [string]$Prefix) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing finished package: $Path" }
    $archive = [IO.Compression.ZipFile]::OpenRead($Path)
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $expectedNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($name in $Expected.Keys) { [void]$expectedNames.Add($name) }
    try {
        foreach ($entry in $archive.Entries) {
            if (!$entry.FullName.StartsWith($Prefix, [StringComparison]::Ordinal)) { throw 'Unexpected ZIP root' }
            $relative = $entry.FullName.Substring($Prefix.Length)
            if (!$expectedNames.Contains($relative) -or !$seen.Add($relative)) { throw "Unexpected or duplicate ZIP entry: $relative" }
            $stream = $entry.Open(); $hasher = [Security.Cryptography.SHA256]::Create()
            try { $hash = ([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
            finally { $stream.Dispose(); $hasher.Dispose() }
            if ($hash -cne $Expected[$relative]) { throw "ZIP content mismatch: $relative" }
        }
        if ($seen.Count -ne $Expected.Count) { throw 'ZIP omits approved files' }
    } finally { $archive.Dispose() }
}
$releaseProblems = [System.Collections.Generic.List[string]]::new()
$releaseFiles = [System.Collections.Generic.List[string]]::new()
$releaseBuilds = @(14075,14422,14505,14812,49284,167409,204489)
foreach ($releasePlatform in @('Win32','x64')) {
    foreach ($releaseProduct in @('glide-server.exe','glide-launcher.exe','glide-accounts.exe','glide-catalog.exe','glide-client.dll')) {
        $releaseFiles.Add("bin/$releasePlatform/Release/$releaseProduct")
    }
}
$releaseFiles.Add('bin/Win32/Release/inject32.exe')
foreach ($releaseDocument in @('LICENSE','THIRD_PARTY_NOTICES.md','README.md','docs/assets/promo.png','server/server.cfg','vendor/minhook/LICENSE.txt')) {
    $releaseFiles.Add($releaseDocument)
}
foreach ($releaseProtocolDocument in (Get-Content -LiteralPath (Join-Path $releaseRoot 'release-files.txt') | Where-Object { $_ -match '^docs/protocol/' })) {
    $releaseFiles.Add($releaseProtocolDocument)
}
foreach ($releaseBuild in $releaseBuilds) {
    $releaseFiles.Add("docs/screenshots/$releaseBuild/character-selection.png")
    $releaseFiles.Add("docs/screenshots/$releaseBuild/in-game.png")
    $releaseFiles.Add("builds/$releaseBuild/runtime-manifest.txt")
}

foreach ($releaseRelative in $releaseFiles) {
    $releaseSource = Join-Path $releaseRoot $releaseRelative
    if (!(Test-Path -LiteralPath $releaseSource -PathType Leaf)) {
        $releaseProblems.Add("Missing: $releaseRelative")
    } elseif ((Get-Item -Force -LiteralPath $releaseSource).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        $releaseProblems.Add("Release input cannot be a reparse point: $releaseRelative")
    }
}
$releaseProofPath = Join-Path $releaseRoot 'dist/release-verification.json'
if (!(Test-Path -LiteralPath $releaseProofPath -PathType Leaf)) {
    $releaseProblems.Add('Missing current seven-build verification: dist/release-verification.json')
} else {
    $releaseProof = Get-Content -LiteralPath $releaseProofPath -Raw | ConvertFrom-Json
    foreach ($releaseBuild in $releaseBuilds) {
        $releaseEvidence = @($releaseProof.builds | Where-Object { $_.build -eq $releaseBuild -and $_.prepared -eq $true -and $_.started -eq $true -and $_.login -eq $true -and $_.enteredWorld -eq $true -and $_.movement -eq $true -and $_.jump -eq $true })
        if ($releaseEvidence.Count -ne 1) { $releaseProblems.Add("Build ${releaseBuild}: missing preparation, login, world entry or movement evidence") }
    }
    foreach ($releaseRelative in $releaseFiles) {
        $releaseSource = Join-Path $releaseRoot $releaseRelative
        $releasePinned = @($releaseProof.artifacts | Where-Object { $_.path -ceq $releaseRelative })
        if ($releasePinned.Count -ne 1 -or !(Test-Path -LiteralPath $releaseSource -PathType Leaf)) {
            if (!$RefreshVerification -or $releaseRelative -match '^bin/' -or !(Test-Path -LiteralPath $releaseSource -PathType Leaf)) {
                $releaseProblems.Add("Unverified release input: $releaseRelative")
            }
        } elseif ((Get-FileHash -LiteralPath $releaseSource -Algorithm SHA256).Hash -ine $releasePinned[0].sha256) {
            if (!$RefreshVerification -or $releaseRelative -match '^bin/') {
                $releaseProblems.Add("Release input changed after verification: $releaseRelative")
            }
        }
    }
}
if ($releaseProblems.Count) {
    foreach ($releaseProblem in $releaseProblems) { Write-Output $releaseProblem }
    exit 1
}
if (!(Test-Path -LiteralPath (Join-Path $releaseRoot '.git') -PathType Container)) { throw 'Release source must be an independent Git checkout' }
$releaseTop = & git -C $releaseRoot rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0 -or [IO.Path]::GetFullPath($releaseTop.Trim()) -ine [IO.Path]::GetFullPath($releaseRoot)) { throw 'Source must be the independent project repository' }
$releaseManifest = Join-Path $releaseRoot 'release-files.txt'
if (!(Test-Path -LiteralPath $releaseManifest -PathType Leaf)) { throw 'Missing explicit source file inventory: release-files.txt' }
[string[]]$releaseTracked = @(Get-Content -LiteralPath $releaseManifest | Where-Object { $_.Length -gt 0 })
$releaseApproved = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($releasePath in $releaseTracked) {
    if ($releasePath -match '(^|/)\.\.?(/|$)|\\|^/|//|[:\x00-\x1f]' -or !$releaseApproved.Add($releasePath)) {
        throw "Invalid or duplicate source inventory entry: $releasePath"
    }
    if (!(Test-Path -LiteralPath (Join-Path $releaseRoot $releasePath) -PathType Leaf)) {
        throw "Missing listed source file: $releasePath"
    }
}
[string[]]$releaseObserved = @(& git -c core.quotepath=false -C $releaseRoot ls-files --cached --others --exclude-standard | Select-Object -Unique)
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect release source list' }
foreach ($releasePath in $releaseObserved) {
    if (!$releaseApproved.Contains($releasePath)) { throw "Source file is not in release-files.txt: $releasePath" }
}
[Array]::Sort($releaseTracked, [StringComparer]::Ordinal)
$releaseSourceHashes = @{}
$releaseSourceLines = [System.Collections.Generic.List[string]]::new()
foreach ($releasePath in $releaseTracked) {
    $releaseName = [IO.Path]::GetFileName($releasePath)
    $releasePrivateName = $releaseName -match '^(?:\.env(?:\..*)?|\.npmrc|\.pypirc|\.netrc|credentials(?:\..*)?|secrets?(?:\..*)?|id_rsa.*|id_ed25519.*|AGENTS\.md|MEMORY\.md)$'
    if (($releasePrivateName -and $releaseName -notin @('.env.example','.env.sample')) -or
        $releasePath -match '(?:^|/)\.(?:ssh|aws|azure|gcloud|codex|claude|agents|idea|vscode)(?:/|$)' -or
        $releasePath -match '\.(?:pem|key|p12|pfx|dmp|pcap|pcapng|har|db|sqlite|sqlite3)$') {
        throw "Private configuration, credentials or local data cannot be packaged: $releasePath"
    }
    if ($releasePath -notmatch '^(?:(?:common|builds|server|client|launcher|catalog|accounts|inject32|vendor|\.github)/|(?:scripts/check_distribution\.py|docs/assets/promo\.png|docs/protocol/(?:(?:README|encoding|transport)\.md|(?:14075|14422|14505|14812|49284|167409|204489)/(?:README\.md|(?:auth|game|sts)/(?:c2s|s2c)/[a-z0-9-]+\.md))|docs/screenshots/(?:14075|14422|14505|14812|49284|167409|204489)/(?:character-selection|in-game)\.png|quality\.ps1|\.clang-format|README\.md|LICENSE|THIRD_PARTY_NOTICES\.md|build\.bat|release\.ps1|release-files\.txt|glide\.props|GlideAholic\.sln|\.gitignore|\.gitattributes)$)') {
        throw "Unapproved source path: $releasePath"
    }
    if ($releasePath -match '\.(?:exe|dll|dat|raw|compressed|bxml|enc|bin|pdb|zip)$' -or $releasePath -match '^(?:acceptance|research|runtime|state|data|tools|bin|build)/') {
        throw "Generated, private or original binary payload in source: $releasePath"
    }
    $releaseSourceFile = Join-Path $releaseRoot $releasePath
    if ((Get-Item -Force -LiteralPath $releaseSourceFile).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Source links are not packaged: $releasePath" }
    $releaseSourceHash = (Get-FileHash -LiteralPath $releaseSourceFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $releaseSourceHashes[$releasePath] = $releaseSourceHash
    $releaseSourceLines.Add("$releaseSourceHash  $releasePath")
}
if (!$releaseTracked.Count) { throw 'Source snapshot is empty' }
$releaseSourceBytes = [Text.Encoding]::UTF8.GetBytes(($releaseSourceLines -join "`n") + "`n")
$releaseHasher = [Security.Cryptography.SHA256]::Create()
try { $releaseSnapshot = ([BitConverter]::ToString($releaseHasher.ComputeHash($releaseSourceBytes))).Replace('-','').ToLowerInvariant() }
finally { $releaseHasher.Dispose() }
if ($RefreshVerification) {
    $releaseProof.sourceSnapshot = $releaseSnapshot
    $releaseProof.artifacts = @($releaseFiles | ForEach-Object { @{path=$_;sha256=(FileHash (Join-Path $releaseRoot $_))} })
    $releaseProof | Add-Member -NotePropertyName sourceRefresh -NotePropertyValue @{at=[DateTime]::UtcNow.ToString('o');basis='Existing native evidence retained; binary hashes unchanged. This operation does not run native QA.'} -Force
    $releaseProof | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $releaseProofPath -Encoding UTF8
}
if (!$Candidate) {
    $reviewPath = Join-Path $releaseRoot 'dist/publication-review.json'
    if (!(Test-Path -LiteralPath $reviewPath)) { throw 'Publication review is pending; provide the source-bound dist/publication-review.json record. Use a -candidate version and -Candidate only for local review.' }
    $review = Get-Content -LiteralPath $reviewPath -Raw | ConvertFrom-Json
    if ($review.sourceSnapshot -cne $releaseSnapshot) { throw 'Publication review does not identify this source snapshot' }
    foreach ($name in @('inflaterLicense','clientAdaptations','documentationArtwork','toolchainRedistribution')) {
        $item = $review.$name
        if (!$item -or $item.status -cne 'approved' -or [string]::IsNullOrWhiteSpace($item.responsible) -or [string]::IsNullOrWhiteSpace($item.evidence)) { throw "Publication decision pending: $name" }
    }
}
if ($releaseProof.sourceSnapshot -cne $releaseSnapshot) { throw 'Source changed after verification; source snapshot does not match' }
if ($CheckOnly) { Write-Output "Inputs and seven-build native evidence checked; finished ZIPs have NOT been verified: $releaseSnapshot"; exit 0 }
$releaseDist = Join-Path $releaseRoot 'dist'
$releaseStage = Join-Path $releaseDist ('stage-' + [guid]::NewGuid().ToString('N'))
$releaseBundle = Join-Path $releaseStage "glide-aholic-$Version"
$releaseZip = Join-Path $releaseDist "glide-aholic-$Version-windows.zip"
$releaseSourceZip = Join-Path $releaseDist "glide-aholic-$Version-source.zip"
$checksumPath = Join-Path $releaseDist "glide-aholic-$Version-SHA256SUMS.txt"
if (!$VerifyPackages -and ((Test-Path -LiteralPath $releaseZip) -or (Test-Path -LiteralPath $releaseSourceZip) -or (Test-Path -LiteralPath $checksumPath))) { throw 'A package with this version already exists; existing packages are preserved' }
if (!$VerifyPackages) {
New-Item -ItemType Directory -Path $releaseBundle -Force | Out-Null
foreach ($releaseRelative in $releaseFiles) {
    $releaseTarget = Join-Path $releaseBundle $releaseRelative
    New-Item -ItemType Directory -Path (Split-Path -Parent $releaseTarget) -Force | Out-Null
    Copy-Item -Force -LiteralPath (Join-Path $releaseRoot $releaseRelative) -Destination $releaseTarget
    $releasePinned = @($releaseProof.artifacts | Where-Object { $_.path -ceq $releaseRelative })
    if ((Get-FileHash -LiteralPath $releaseTarget -Algorithm SHA256).Hash -ine $releasePinned[0].sha256) {
        throw "Release input changed during packaging: $releaseRelative"
    }
}
$releaseHashes = foreach ($releaseRelative in $releaseFiles) {
    $releaseHash = (Get-FileHash -LiteralPath (Join-Path $releaseBundle $releaseRelative) -Algorithm SHA256).Hash.ToLowerInvariant()
    "$releaseHash  $releaseRelative"
}
$releaseHashes | Set-Content -LiteralPath (Join-Path $releaseBundle 'SHA256SUMS.txt') -Encoding ascii
$releaseSourceBundle = Join-Path (Join-Path $releaseStage 'source') "glide-aholic-$Version"
New-Item -ItemType Directory -Path $releaseSourceBundle -Force | Out-Null
foreach ($releasePath in $releaseTracked) {
    $releaseTarget = Join-Path $releaseSourceBundle $releasePath
    New-Item -ItemType Directory -Path (Split-Path -Parent $releaseTarget) -Force | Out-Null
    Copy-Item -Force -LiteralPath (Join-Path $releaseRoot $releasePath) -Destination $releaseTarget
    if ((Get-FileHash -LiteralPath $releaseTarget -Algorithm SHA256).Hash -ine $releaseSourceHashes[$releasePath]) { throw "Source changed during packaging: $releasePath" }
}
$releaseSourceLines | Set-Content -LiteralPath (Join-Path $releaseSourceBundle 'SOURCE-SHA256SUMS.txt') -Encoding ascii
"Source snapshot SHA256: $releaseSnapshot" | Set-Content -LiteralPath (Join-Path $releaseSourceBundle 'SOURCE-SNAPSHOT.txt') -Encoding ascii
$sourceHash = ''
$sourceTemporary = Join-Path $releaseStage 'source.zip'
WriteZip $sourceTemporary $releaseSourceBundle @($releaseTracked + @('SOURCE-SHA256SUMS.txt','SOURCE-SNAPSHOT.txt')) "glide-aholic-$Version/"
$sourceHash = FileHash $sourceTemporary
$status = if ($Candidate) { 'Local review candidate; publication is not approved.' } else { 'Publication review recorded separately.' }
$sourceInstructions = "Corresponding source: glide-aholic-$Version-source.zip`nSHA256: $sourceHash`nObtain the source beside this Windows download, at no additional charge.`n$status`n"
[IO.File]::WriteAllText((Join-Path $releaseBundle 'SOURCE.txt'), $sourceInstructions, [Text.UTF8Encoding]::new($false))
$runtimeTemporary = Join-Path $releaseStage 'windows.zip'
WriteZip $runtimeTemporary $releaseBundle @($releaseFiles.ToArray() + @('SHA256SUMS.txt','SOURCE.txt')) "glide-aholic-$Version/"
$sourceExpected = $releaseSourceHashes.Clone()
$sourceExpected['SOURCE-SHA256SUMS.txt'] = FileHash (Join-Path $releaseSourceBundle 'SOURCE-SHA256SUMS.txt')
$sourceExpected['SOURCE-SNAPSHOT.txt'] = FileHash (Join-Path $releaseSourceBundle 'SOURCE-SNAPSHOT.txt')
$runtimeExpected = @{}
foreach ($file in @($releaseFiles.ToArray() + @('SHA256SUMS.txt','SOURCE.txt'))) { $runtimeExpected[$file] = FileHash (Join-Path $releaseBundle $file) }
AssertZip $sourceTemporary $sourceExpected "glide-aholic-$Version/"
AssertZip $runtimeTemporary $runtimeExpected "glide-aholic-$Version/"
AssertInstructionData @($sourceTemporary, $runtimeTemporary)
$created = @()
try {
    Move-Item -LiteralPath $sourceTemporary -Destination $releaseSourceZip; $created += $releaseSourceZip
    Move-Item -LiteralPath $runtimeTemporary -Destination $releaseZip; $created += $releaseZip
} catch {
    foreach ($file in $created) { Remove-Item -LiteralPath $file }
    throw
}
$releasePackageHashes = foreach ($releasePackage in @($releaseZip,$releaseSourceZip)) {
    $releaseHash = (Get-FileHash -LiteralPath $releasePackage -Algorithm SHA256).Hash.ToLowerInvariant()
    "$releaseHash  $(Split-Path -Leaf $releasePackage)"
}
$releasePackageHashes | Set-Content -LiteralPath (Join-Path $releaseDist "glide-aholic-$Version-SHA256SUMS.txt") -Encoding ascii
Write-Output "Created runtime and corresponding source packages for source snapshot $releaseSnapshot"

} # Package creation; the verification path below also works on existing ZIPs.
$sourceExpected = $releaseSourceHashes.Clone()
$shaText = ($releaseSourceLines -join "`r`n") + "`r`n"
$snapshotText = "Source snapshot SHA256: $releaseSnapshot`r`n"
function TextHash([string]$Value) {
    $h = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($h.ComputeHash([Text.Encoding]::ASCII.GetBytes($Value)))).Replace('-','').ToLowerInvariant() }
    finally { $h.Dispose() }
}
$sourceExpected['SOURCE-SHA256SUMS.txt'] = TextHash $shaText
$sourceExpected['SOURCE-SNAPSHOT.txt'] = TextHash $snapshotText
AssertZip $releaseSourceZip $sourceExpected "glide-aholic-$Version/"
$runtimeExpected = @{}
$runtimeLines = @()
foreach ($file in $releaseFiles) { $hash = FileHash (Join-Path $releaseRoot $file); $runtimeExpected[$file]=$hash; $runtimeLines += "$hash  $file" }
$runtimeExpected['SHA256SUMS.txt'] = TextHash (($runtimeLines -join "`r`n") + "`r`n")
$status = if ($Candidate) { 'Local review candidate; publication is not approved.' } else { 'Publication review recorded separately.' }
$sourceInstructions = "Corresponding source: glide-aholic-$Version-source.zip`nSHA256: $(FileHash $releaseSourceZip)`nObtain the source beside this Windows download, at no additional charge.`n$status`n"
$runtimeExpected['SOURCE.txt'] = TextHash $sourceInstructions
AssertZip $releaseZip $runtimeExpected "glide-aholic-$Version/"
AssertInstructionData @($releaseSourceZip, $releaseZip)
$checksumPath = Join-Path $releaseDist "glide-aholic-$Version-SHA256SUMS.txt"
if (!(Test-Path -LiteralPath $checksumPath)) { throw 'Missing completed package checksum record' }
$expectedChecksumLines = @($releaseZip,$releaseSourceZip) | ForEach-Object { "$(FileHash $_)  $(Split-Path -Leaf $_)" }
if ((Get-Content -LiteralPath $checksumPath -Raw).Trim() -cne ($expectedChecksumLines -join "`r`n")) { throw 'Package checksum record mismatch' }
Write-Output "Both finished ZIPs verified against their exact inventories and hashes: $Version"
