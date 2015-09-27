# Script to upload the package to http://www.nuget.org/ and the symbols to http://mmaitre314.blob.core.windows.net/symbols/
# Add-AzureAccount must be run before the script
#
# References about source indexing:
#   https://github.com/Haemoglobin/GitHub-Source-Indexer/
#   http://sourcepack.codeplex.com/
#   https://docs.google.com/document/d/13VM59LEuNps66TK_vITqKd1TPlUtKDaOg9T2dLnFXnE/edit?hl=en_US
#   https://msdn.microsoft.com/en-us/library/windows/hardware/ff540151(v=vs.85).aspx

$version = "2.3.2"
$outputPath = "c:\NuGet\"
$product = "MMaitre.VideoEffects"
$storageAccountName = "mmaitre314"
$gitUserName = "mmaitre314"
$gitProjectName = "VideoEffect"

function Get-ProgramPath([string]$relativePath)
{
    if (Test-Path "${env:ProgramFiles(x86)}\${relativePath}")
    {
        $absolutePath = "${env:ProgramFiles(x86)}\${relativePath}"
    }
    elseif (Test-Path "${env:ProgramFiles}\${relativePath}")
    {
        $absolutePath = "${env:ProgramFiles}\${relativePath}"
    }
    else
    {
        throw "${relativePath} not found"
    }

    return $absolutePath
}

$gitPath = Get-ProgramPath "Git\cmd\git.exe"
$symstorePath = Get-ProgramPath "Windows Kits\8.1\Debuggers\x86\symstore.exe"
$srctoolPath = Get-ProgramPath "Windows Kits\8.1\Debuggers\x86\srcsrv\srctool.exe"
$pdbstrPath = Get-ProgramPath "Windows Kits\8.1\Debuggers\x86\srcsrv\pdbstr.exe"

# Move to the script folder so GIT knows the repo
cd $PSScriptRoot

# Root of the GIT repo (ex: C:\Users\Matthieu\Source\Repos\MediaCaptureReader\)
$rootPath = & $gitPath rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0)
{
    throw "GIT returned $LASTEXITCODE"
}
$rootPath = [System.IO.Path]::GetFullPath($rootPath) + "\"

function Find-Pdbs($zip)
{
    $PDBs = @()

    foreach ($item in $zip.items())
    {
        if ($item.IsFolder)
        {
            $PDBs += Find-PDBs $item.GetFolder
        }
        else
        {
            if ([System.IO.Path]::GetExtension($item.Name) -eq ".pdb")
            {
                $PDBs += $item
            }
        }
    }

    return $PDBs
}

function Add-SourceInfoToPdb([string]$tempPath, [string]$pdbName)
{
    $pdbPath = "${tempPath}${pdbName}"
    $streamPath = "${tempPath}${pdbName}.srcsrv.txt"

    # Get the list of source files in the PDB
    $pdbSourcePaths = @(& "$srctoolPath" "$pdbPath" -r)
    Write-Host "Source files in PDB: $($pdbSourcePaths.Count)"

    # Get the list of source files checked in
    $gitSourceRelativePaths = @(& "$gitPath" ls-tree --name-only --full-tree -r v$version)
    Write-Host "Source files in Git: $($gitSourceRelativePaths.Count)"
    if ($gitSourceRelativePaths.Count -eq 0)
    {
        throw "Git did not return any source file"
    }

    # Write the PDB source stream to file
    "SRCSRV: ini ------------------------------------------------" | Out-File -Encoding ascii -FilePath $streamPath -Force
    "VERSION=2" | Out-File -Encoding ascii -FilePath $streamPath -Append
    "VERCTRL=http" | Out-File -Encoding ascii -FilePath $streamPath -Append
    "SRCSRV: variables ------------------------------------------" | Out-File -Encoding ascii -FilePath $streamPath -Append
    "HTTP_ALIAS=https://raw.githubusercontent.com/${gitUserName}/${gitProjectName}/v${version}/" | Out-File -Encoding ascii -FilePath $streamPath -Append
    "HTTP_EXTRACT_TARGET=%HTTP_ALIAS%%var2%" | Out-File -Encoding ascii -FilePath $streamPath -Append
    "SRCSRVTRG=%HTTP_EXTRACT_TARGET%" | Out-File -Encoding utf8 -FilePath $streamPath -Append
    "SRCSRV: source files ---------------------------------------" | Out-File -Encoding ascii -FilePath $streamPath -Append
    $sourceCount = 0
    foreach ($pdbSourcePath in $pdbSourcePaths)
    {
        if (-not $pdbSourcePath.EndsWith(".h") -and -not $pdbSourcePath.EndsWith(".cpp") -and -not $pdbSourcePath.EndsWith(".cs"))
        {
            continue
        }
        $sourcePathLower = $pdbSourcePath.ToLower().Replace("\", "/")

        # Git (and GitHub) are picky about case, so use the Git relative path
        $gitSourceRelativePath = $gitSourceRelativePaths | ? { ($pdbSourcePath.Length -ge $_.Length) -and ($_ -eq $sourcePathLower.Substring($sourcePathLower.Length - $_.Length, $_.Length)) }
        if ($gitSourceRelativePath -eq $null)
        {
            continue
        }

        "${pdbSourcePath}*${gitSourceRelativePath}" | Out-File -Encoding ascii -FilePath $streamPath -Append
        $sourceCount++
    }
    "SRCSRV: end ------------------------------------------------" | Out-File -Encoding ascii -FilePath $streamPath -Append
    Write-Host "Added $sourceCount source files to PDB"
    
    if ($sourceCount -gt 0)
    {
        # Add stream to PDB
        & "$pdbstrPath" -w -s:srcsrv -p:"$pdbPath" -i:"$streamPath"
    }
    else
    {
        Write-Host "WARNING: no source file added to PDB" -ForegroundColor Yellow
    }
}

# Create a temp folder
$nupkgPath = "${outputPath}Symbols\${product}.Symbols.${version}.nupkg"
$tempPath = "${nupkgPath}.temp\"
$tempSymbolPath = "${tempPath}Symbols\"
Write-Host "Creating temporary folder ${tempPath}" -ForegroundColor Cyan
if (Test-Path $tempPath -PathType Container)
{
    Get-ChildItem $tempPath -File | Remove-Item -Force
}
New-Item -ItemType Directory -Path $tempPath -Force | Out-Null

Write-Host "Uploading NuGet package" -ForegroundColor Cyan
& "${outputPath}nuget.exe" push "${outputPath}Packages\${product}.${version}.nupkg"
if ($LASTEXITCODE -ne 0)
{
    throw "symstore returned $LASTEXITCODE"
}

# SymbolSource.org currently does not support native PDBs, so publishing to Azure blob instead
# & "${outputPath}nuget.exe" push "${outputPath}Symbols\${product}.Symbols.${version}.nupkg" -Source http://nuget.gw.symbolsource.org/Public/NuGet

Write-Host "Publishing GIT tag" -ForegroundColor Cyan
& $gitPath push origin "v${version}"
if ($LASTEXITCODE -ne 0)
{
    throw "git returned $LASTEXITCODE"
}

# Copy the NuGet symbol package to a file with a .zip extension so Shell accepts to open it
$zipPath = "${tempPath}symbols.zip"
Copy-Item -Path $nupkgPath -Destination $zipPath -Force

Write-Host "Looking for PDBs in ${nupkgPath}" -ForegroundColor Cyan
$shell = New-Object -COM Shell.Application
$zip = $shell.NameSpace($zipPath)
$PDBs = Find-Pdbs $zip

foreach ($PDB in $PDBs)
{
    $shell.Namespace($tempPath).CopyHere($PDB)
    $pdbPath = "${tempPath}$($PDB.Name)"

    # Add source info
    Write-Host "Adding source info to ${pdbPath}" -ForegroundColor Cyan
    Add-SourceInfoToPdb $tempPath $PDB.Name

    # Index and compress
    Write-Host "Indexing and compressing ${pdbPath}" -ForegroundColor Cyan
    & $symstorePath add /f "${pdbPath}" /compress /s "$tempSymbolPath" /t ${product} /o
    if ($LASTEXITCODE -ne 0)
    {
        throw "symstore returned $LASTEXITCODE"
    }
    Remove-Item "${tempPath}*.pdb"
}

# Upload to Azure blobs
$storageAccountKey = Get-AzureStorageKey $storageAccountName | %{ $_.Primary }
$context = New-AzureStorageContext -StorageAccountName $storageAccountName -StorageAccountKey $storageAccountKey
foreach ($file in Get-ChildItem "${tempSymbolPath}*.pd_" -Recurse)
{
    $blob = $file.FullName.Substring($tempSymbolPath.Length).Replace("\", "/")

    Write-Host "Uploading ${blob}" -ForegroundColor Cyan
    Set-AzureStorageBlobContent -BlobType Block -Blob $blob -Container "symbols" -File $file.FullName -Context $context -Force
}

# Clean up
Get-ChildItem $tempPath -File | Remove-Item -Force
Remove-Item $tempPath -Recurse -Force
