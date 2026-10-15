param([Parameter(Mandatory)][string]$PackageDirectory,[Parameter(Mandatory)][string]$ReportDirectory)
$ErrorActionPreference='Stop'
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$report=[IO.Path]::GetFullPath($ReportDirectory)
if(Test-Path -LiteralPath $report){throw 'Use pasta de relatorio nova para preservar evidencias.'}
New-Item -ItemType Directory -Path $report|Out-Null
$install=Join-Path $env:LOCALAPPDATA 'Programs/NOVA DAW Alpha 0.7'
$registration='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{32C8E968-D103-4A70-9665-EE04BA06A707}'
$windowsInstaller=New-Object -ComObject WindowsInstaller.Installer
function Product-State([int]$Context){try{$sid=if($Context -eq 4){$null}else{''};$product=$windowsInstaller.GetType().InvokeMember('Product',[Reflection.BindingFlags]::GetProperty,$null,$windowsInstaller,@('{32C8E968-D103-4A70-9665-EE04BA06A707}',$sid,$Context));return $product.GetType().InvokeMember('State',[Reflection.BindingFlags]::GetProperty,$null,$product,$null)}catch{return -1}}
if((Product-State 2) -eq 5 -or (Product-State 4) -eq 5){throw 'Alpha ja instalada; nao remover instalacao preexistente do usuario.'}
if((Test-Path -LiteralPath $install) -and @(Get-ChildItem -LiteralPath $install -Force).Count){$existing=@(Get-ChildItem -LiteralPath $install -Force);if($existing.Count -ne 1 -or $existing[0].Name -ne 'test-user-source-preserve.txt' -or (Get-Content -LiteralPath $existing[0].FullName -Raw) -ne 'Source outside MSI manifest: preserve'){throw 'Pasta preexistente nao pertence ao ensaio; preservar conteudo.'}}
$checks=New-Object 'Collections.Generic.List[object]'
$utf8=New-Object Text.UTF8Encoding($false)
function Check([bool]$Condition,[string]$Name){$checks.Add(@{name=$Name;passed=$Condition});Write-Output "$Condition : $Name";if(!$Condition){throw $Name}}
function Process([string]$File,[string[]]$Arguments){$p=Start-Process -FilePath $File -ArgumentList $Arguments -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru;if(!$p.WaitForExit(120000)){throw "Timeout: $File"};$p.Refresh();return $p.ExitCode}
try {
 $setup=Join-Path $package 'NOVA-DAW-Alpha-0.7-Public-Test-x64-Setup.exe'
 $code=Process $setup @('/quiet','/log',('"{0}"' -f (Join-Path $report 'install.log')))
 Check ($code -eq 0) 'EXE installs per-user successfully'
 Check ((Product-State 2) -eq 5 -and (Product-State 4) -ne 5 -and (Test-Path -LiteralPath (Join-Path $install 'NOVA DAW.exe'))) 'Windows Installer registers USERUNMANAGED context2, not MACHINE context4, at expected user path'
 $manifest=Get-Content (Join-Path $install 'installed-files.json') -Encoding UTF8 -Raw|ConvertFrom-Json
 $invalid=@();foreach($entry in $manifest){$path=[IO.Path]::GetFullPath((Join-Path $install $entry.path));if(!$path.StartsWith($install+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Manifest path outside install directory'};if(!(Test-Path -LiteralPath $path) -or (Get-FileHash $path -Algorithm SHA256).Hash -ne $entry.sha256){$invalid+=$entry.path}}
 Check ($invalid.Count -eq 0) 'Every packaged file matches SHA256 after installation'
 $shell=New-Object -ComObject WScript.Shell
 $desktop=Join-Path ([Environment]::GetFolderPath('Desktop')) 'NOVA DAW Alpha 0.7.lnk'
 $start=Join-Path ([Environment]::GetFolderPath('Programs')) 'NOVA DAW Alpha 0.7'
 $app=Join-Path $install 'NOVA DAW.exe'
 Check ((Test-Path -LiteralPath $desktop) -and $shell.CreateShortcut($desktop).TargetPath -eq $app) 'Desktop shortcut targets installed Alpha'
 Check ((Test-Path -LiteralPath (Join-Path $start 'NOVA DAW Alpha 0.7.lnk')) -and $shell.CreateShortcut((Join-Path $start 'NOVA DAW Alpha 0.7.lnk')).TargetPath -eq $app) 'Start Menu shortcut targets installed Alpha'
 $data=Join-Path $report 'user-data'
 $process=Start-Process -FilePath $app -ArgumentList @('--data-dir',('"{0}"' -f $data)) -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru
 $deadline=[DateTime]::UtcNow.AddSeconds(20);do{$process.Refresh();if($process.MainWindowHandle -ne 0){break};Start-Sleep -Milliseconds 100}while(!$process.HasExited -and [DateTime]::UtcNow -lt $deadline)
 Check (!$process.HasExited -and $process.MainWindowTitle -match 'Alpha 0.7') 'Installed application starts from unrelated cwd with Alpha title'
 Check ((Get-Item -LiteralPath $app).VersionInfo.FileVersion -eq '0.7.0') 'Windows executable resource has current 0.7.0 version'
 $busy=Process $setup @('/quiet')
 Check ($busy -eq 1618 -and !$process.HasExited) 'Setup refuses repair while installed DAW runs and does not close user session'
 $busy=Process (Join-Path $install 'Desinstalar NOVA DAW Alpha 0.7.exe') @('/quiet')
 Check ($busy -eq 1618 -and !$process.HasExited) 'Uninstaller refuses removal while DAW runs and does not close user session'
 $busy=Process "$env:SystemRoot/System32/msiexec.exe" @('/x','{32C8E968-D103-4A70-9665-EE04BA06A707}','/qn','/norestart','/L*v',('"{0}"' -f (Join-Path $report 'busy-windows-uninstall.log')))
 Check ($busy -ne 0 -and !$process.HasExited -and (Product-State 2) -eq 5) 'Windows MSI/Apps uninstall also refuses to remove a running DAW'
 $modules=@($process.Modules|Where-Object ModuleName -Match '^(msvcp140|vcruntime140).*\.dll$'|Select-Object ModuleName,FileName)
 [IO.File]::WriteAllText((Join-Path $report 'loaded-runtime.json'),($modules|ConvertTo-Json -Depth 4),$utf8)
 Check ($modules.Count -ge 3 -and @($modules|Where-Object {!$_.FileName.StartsWith($install+'\',[StringComparison]::OrdinalIgnoreCase)}).Count -eq 0) 'Application loads MSVC runtime from its own package'
 [void]$process.CloseMainWindow();Check ($process.WaitForExit(20000)) 'Empty installed session closes normally'
 Check ([bool](Get-Content (Join-Path $data 'Diagnostics/last-run.json') -Raw|ConvertFrom-Json).cleanExit) 'Installed application creates user diagnostics and clean shutdown marker'
 $scan=Join-Path $report 'scan.xml';$plugin=Join-Path $install 'Plugins/NOVA Test Synth.vst3/Contents/x86_64-win/NOVA Test Synth.vst3'
 $code=Process (Join-Path $install 'NovaPluginScanner.exe') @(('"{0}"' -f $plugin),('"{0}"' -f $scan))
 Check ($code -eq 0 -and (Get-Content $scan -Raw).Contains('NOVA Test Synth')) 'Installed child scanner loads bundled real VST3 instrument'
 foreach($mode in @('midi','recording','advanced','mastering')){$dir=Join-Path $report "ui-$mode";$code=Process $app @('--data-dir',('"{0}"' -f (Join-Path $dir 'data')),"--validate-$mode-ui",('"{0}"' -f $dir));$result=Get-Content (Join-Path $dir 'ui-result.json') -Raw|ConvertFrom-Json;Check ($code -eq 0 -and $result.passed) "Installed $mode UI and real audio/file workflow"}
 $crash=Join-Path $report 'crash-test';$code=Process $app @('--data-dir',('"{0}"' -f $crash),'--test-crash-report');$fatal=@(Get-ChildItem (Join-Path $crash 'Diagnostics') -Filter 'crash-*.txt');Check ($code -ne 0 -and $fatal.Count -eq 1 -and (Get-Content $fatal[0].FullName -Raw).Contains('E04E4F56')) 'Installed binary writes real controlled Windows exception report'
 $code=Process $app @('--data-dir',('"{0}"' -f $crash),'--diagnostics-smoke');Check ($code -eq 0 -and (Get-Content (Join-Path $crash 'Diagnostics/nova.log') -Raw).Contains('Previous run did not close cleanly')) 'Next launch detects previous abnormal exit'
 # Move only a verified file owned by this newly created test installation.
 $scanner=Join-Path $install 'NovaPluginScanner.exe';$scannerHash=(Get-FileHash $scanner -Algorithm SHA256).Hash
 Move-Item -LiteralPath $scanner -Destination (Join-Path $report 'scanner-before-repair.exe')
 $code=Process $setup @('/quiet','/log',('"{0}"' -f (Join-Path $report 'repair.log')))
 Check ($code -eq 0 -and (Get-FileHash $scanner -Algorithm SHA256).Hash -eq $scannerHash) 'Running EXE again repairs missing owned scanner without user-data changes'
 $sentinel=Join-Path $install 'test-user-source-preserve.txt';if(!(Test-Path -LiteralPath $sentinel)){[IO.File]::WriteAllText($sentinel,'Source outside MSI manifest: preserve',$utf8)}
 $logBefore=(Get-FileHash (Join-Path $data 'Diagnostics/last-run.json') -Algorithm SHA256).Hash
 $uninstaller=Join-Path $install 'Desinstalar NOVA DAW Alpha 0.7.exe';$code=Process $uninstaller @('/quiet','/log',('"{0}"' -f (Join-Path $report 'uninstall.log')))
 # The launcher exits before msiexec removes it. Registration can disappear
 # before the engine finishes deleting files; wait for actual completion too.
 $deadline=[DateTime]::UtcNow.AddSeconds(90)
 do {$ownedRemain=@($manifest|Where-Object {Test-Path -LiteralPath (Join-Path $install $_.path)}).Count;$uninstallLog=Join-Path $report 'uninstall.log';$complete=(Test-Path -LiteralPath $uninstallLog) -and [bool](Select-String -LiteralPath $uninstallLog -Pattern 'MainEngineThread is returning 0' -Quiet);if((Product-State 2) -ne 5 -and $ownedRemain -eq 0 -and $complete){break};Start-Sleep -Milliseconds 200}while([DateTime]::UtcNow -lt $deadline)
 Check ($code -eq 0 -and (Product-State 2) -ne 5 -and (Product-State 4) -ne 5) 'Packaged uninstaller removes Windows product registration'
 Check $complete 'Windows Installer finishes uninstallation successfully before acceptance'
 Check (@($manifest|Where-Object {Test-Path -LiteralPath (Join-Path $install $_.path)}).Count -eq 0) 'Uninstall removes all manifest-owned payload files'
 Check (!(Test-Path -LiteralPath $desktop) -and !(Test-Path -LiteralPath (Join-Path $start 'NOVA DAW Alpha 0.7.lnk'))) 'Uninstall removes only package shortcuts'
 Check ((Get-Content $sentinel -Raw) -eq 'Source outside MSI manifest: preserve' -and (Get-FileHash (Join-Path $data 'Diagnostics/last-run.json') -Algorithm SHA256).Hash -eq $logBefore) 'Uninstall preserves unknown sources and user diagnostics'
 # Preserve the sentinel as evidence of source retention. No recursive cleanup.
 $passed=$true
} catch {$passed=$false;$failure=$_.Exception.Message;Write-Output "FAIL: $failure"} finally {
 $result=@{passed=$passed;checks=@($checks.ToArray());failure=$failure;computer=$env:COMPUTERNAME;cleanMachine=$false;physicalRecordingTested=$false;externalMidiTested=$false;thirdPartyPluginsTested=$false;installerSignatureTestedOnOtherComputer=$false}
 [IO.File]::WriteAllText((Join-Path $report 'installation-result.json'),($result|ConvertTo-Json -Depth 8),$utf8)
}
if(!$passed){exit 1}
