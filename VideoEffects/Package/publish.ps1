# Script to upload the package to http://www.nuget.org/ and the symbols to http://mmaitre314.blob.core.windows.net/symbols/
# Add-AzureAccount must be run before the script

$version = "2.3.0"
$outputPath = "c:\NuGet\"
$product = "MMaitre.VideoEffects"
$storageAccountName = "mmaitre314"

if (Test-Path "${env:ProgramFiles(x86)}\Windows Kits\8.1\Debuggers\x86\symstore.exe")
{
    $symstorePath = "${env:ProgramFiles(x86)}\Windows Kits\8.1\Debuggers\x86\symstore.exe"
}
elseif (Test-Path "${env:ProgramFiles}\Windows Kits\8.1\Debuggers\x86\symstore.exe")
{
    $symstorePath = "${env:ProgramFiles}\Windows Kits\8.1\Debuggers\x86\symstore.exe"
}
else
{
    throw "symstore.exe not found. Please instead the Windows 8.1 SDK."
}

function Find-PDBs($zip)
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

Write-Host "Uploading NuGet package" -ForegroundColor Cyan
& "${outputPath}nuget.exe" push "${outputPath}Packages\${product}.${version}.nupkg"

# SymbolSource.org currently does not support native PDBs, so publishing to Azure blob instead
# & "${outputPath}nuget.exe" push "${outputPath}Symbols\${product}.Symbols.${version}.nupkg" -Source http://nuget.gw.symbolsource.org/Public/NuGet

# Create a temp folder
$nupkgPath = "${outputPath}Symbols\${product}.Symbols.${version}.nupkg"
$tempPath = "${nupkgPath}.temp\"
$tempSymbolPath = "${tempPath}Symbols\"
Write-Host "Creating temporary folder ${tempPath}" -ForegroundColor Cyan
if (Test-Path ${tempPath} -PathType Container)
{
    Get-ChildItem ${tempPath} -File | Remove-Item -Force
}
New-Item -ItemType Directory -Path $tempPath -Force | Out-Null

# Copy the NuGet symbol package to a file with a .zip extension so Shell accepts to open it
$zipPath = "${tempPath}symbols.zip"
Copy-Item -Path $nupkgPath -Destination $zipPath -Force

Write-Host "Looking for PDBs in ${nupkgPath}" -ForegroundColor Cyan
$shell = New-Object -COM Shell.Application
$zip = $shell.NameSpace($zipPath)
$PDBs = Find-PDBs $zip

foreach ($PDB in $PDBs)
{
    Write-Host "Indexing $($PDB.Name)" -ForegroundColor Cyan
    $shell.Namespace($tempPath).CopyHere($PDB)
    & $symstorePath add /f "${tempPath}$($PDB.Name)" /compress /s "$tempSymbolPath" /t ${product} /o
    if ($LASTEXITCODE -ne 0)
    {
        throw "symstore returned $LASTEXITCODE"
    }
    Remove-Item "${tempPath}*.pdb"
}

# Upload PDBs to Azure blobs
$storageAccountKey = Get-AzureStorageKey $storageAccountName | %{ $_.Primary }
$context = New-AzureStorageContext -StorageAccountName $storageAccountName -StorageAccountKey $storageAccountKey
foreach ($file in Get-ChildItem "${tempPath}Symbols\*.pd_" -Recurse)
{
    $blob = $file.FullName.Substring($tempSymbolPath.Length).Replace("\", "/")

    Write-Host "Uploading ${blob}" -ForegroundColor Cyan
    Set-AzureStorageBlobContent -BlobType Block -Blob $blob -Container "symbols" -File $file.FullName -Context $context -Force
}

# Clean up
Remove-Item $tempPath -Recurse -Force
