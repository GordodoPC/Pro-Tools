param([Parameter(Mandatory)][string]$Scanner,[Parameter(Mandatory)][string]$Plugin,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$scannerPath=(Resolve-Path -LiteralPath $Scanner).Path
$binary=(Resolve-Path -LiteralPath $Plugin).Path
$bundle=Split-Path (Split-Path (Split-Path $binary))
$directory=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ('caminho '+[char]0xE7+' '+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force|Out-Null
Copy-Item -LiteralPath $bundle -Destination $directory -Recurse
$copy=Join-Path $directory ((Split-Path $bundle -Leaf)+'/Contents/x86_64-win/'+(Split-Path $binary -Leaf))
$xmlFile=Join-Path $directory ('resultado '+[char]0xE7+'.xml')
$process=Start-Process -FilePath $scannerPath -ArgumentList @(('"{0}"' -f $copy),('"{0}"' -f $xmlFile)) -WindowStyle Hidden -PassThru
if(!$process.WaitForExit(20000) -or $process.ExitCode -ne 0){throw 'Scanner failed on Unicode input/output paths'}
Write-Output 'PASS: scanner exits normally with UTF16 command line'
[xml]$xml=Get-Content -LiteralPath $xmlFile -Raw -Encoding UTF8
if(!$xml.OuterXml.Contains('NOVA Test Synth')){throw 'Scanner returned no real instrument metadata'}
Write-Output 'PASS: real VST3 instrument discovered inside accented path'
if(!$xml.OuterXml.Contains([string][char]0xE7)){throw 'Scanner did not preserve Unicode module path in XML'}
Write-Output 'PASS: Unicode module identifier persists in real scanner XML'
Write-Output '3 scanner Unicode checks passed'
