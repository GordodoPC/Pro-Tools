param([Parameter(Mandatory)][string]$Stage,[Parameter(Mandatory)][string]$Output,[Parameter(Mandatory)][string]$SafetyDll)
$ErrorActionPreference='Stop'
$Stage=(Resolve-Path -LiteralPath $Stage).Path
$Output=[IO.Path]::GetFullPath($Output)
if(Test-Path -LiteralPath $Output){throw 'O MSI de destino ja existe.'}
$work=Split-Path $Output
$installer=New-Object -ComObject WindowsInstaller.Installer
function Invoke-Com($Object,[string]$Name,[object[]]$Arguments=@()) {$Object.GetType().InvokeMember($Name,[Reflection.BindingFlags]::InvokeMethod,$null,$Object,$Arguments)}
function Set-Com($Object,[string]$Name,[object[]]$Arguments) {$Object.GetType().InvokeMember($Name,[Reflection.BindingFlags]::SetProperty,$null,$Object,$Arguments)}
$database=Invoke-Com $installer 'OpenDatabase' @($Output,3)
function Sql([string]$Statement){$view=Invoke-Com $database 'OpenView' @($Statement);[void](Invoke-Com $view 'Execute');[void](Invoke-Com $view 'Close')}
function Row([string]$Table,[string[]]$Columns,[object[]]$Values){
 $names=($Columns|ForEach-Object {'`'+$_+'`'}) -join ','
 $marks=($Columns|ForEach-Object {'?'}) -join ','
 $view=Invoke-Com $database 'OpenView' @("INSERT INTO ``$Table`` ($names) VALUES ($marks)")
 $record=Invoke-Com $installer 'CreateRecord' @($Values.Count)
 for($i=0;$i -lt $Values.Count;$i++){if($null -eq $Values[$i]){continue};if($Values[$i] -is [int]){[void](Set-Com $record 'IntegerData' @(($i+1),$Values[$i]))}else{[void](Set-Com $record 'StringData' @(($i+1),[string]$Values[$i]))}}
 [void](Invoke-Com $view 'Execute' @($record));[void](Invoke-Com $view 'Close')
}
Sql 'CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) LOCALIZABLE PRIMARY KEY `Property`)'
Sql 'CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)'
Sql 'CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)'
Sql 'CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)'
Sql 'CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)'
Sql 'CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` LONG NOT NULL PRIMARY KEY `File`)'
Sql 'CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` LONG NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)'
Sql 'CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)'
Sql 'CREATE TABLE `Shortcut` (`Shortcut` CHAR(72) NOT NULL, `Directory_` CHAR(72) NOT NULL, `Name` CHAR(128) NOT NULL LOCALIZABLE, `Component_` CHAR(72) NOT NULL, `Target` CHAR(255) NOT NULL, `Arguments` CHAR(255), `Description` CHAR(255) LOCALIZABLE, `Hotkey` SHORT, `Icon_` CHAR(72), `IconIndex` SHORT, `ShowCmd` SHORT, `WkDir` CHAR(72) PRIMARY KEY `Shortcut`)'
Sql 'CREATE TABLE `CreateFolder` (`Directory_` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Directory_`, `Component_`)'
Sql 'CREATE TABLE `LaunchCondition` (`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Condition`)'
Sql 'CREATE TABLE `AppSearch` (`Property` CHAR(72) NOT NULL, `Signature_` CHAR(72) NOT NULL PRIMARY KEY `Property`, `Signature_`)'
Sql 'CREATE TABLE `RegLocator` (`Signature_` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255), `Type` SHORT PRIMARY KEY `Signature_`)'
Sql 'CREATE TABLE `Signature` (`Signature` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `MinVersion` CHAR(20), `MaxVersion` CHAR(20), `MinSize` LONG, `MaxSize` LONG, `MinDate` LONG, `MaxDate` LONG, `Languages` CHAR(255) PRIMARY KEY `Signature`)'
Sql 'CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)'
Sql 'CREATE TABLE `Binary` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL PRIMARY KEY `Name`)'
Sql 'CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` SHORT NOT NULL, `Source` CHAR(72), `Target` CHAR(255) PRIMARY KEY `Action`)'
Sql 'CREATE TABLE `Upgrade` (`UpgradeCode` CHAR(38) NOT NULL, `VersionMin` CHAR(20), `VersionMax` CHAR(20), `Language` CHAR(255), `Attributes` LONG NOT NULL, `Remove` CHAR(255), `ActionProperty` CHAR(72) NOT NULL PRIMARY KEY `UpgradeCode`,`VersionMin`,`VersionMax`,`Language`,`Attributes`)'
$properties=@{ProductCode='{32C8E968-D103-4A70-9665-EE04BA06A707}';UpgradeCode='{AD72DC98-8A24-4D1D-A0BA-FA4B4282C707}';ProductName='NOVA DAW Alpha 0.7 - Public Test';ProductVersion='0.7.0';ProductLanguage='1033';Manufacturer='Projeto SOM';INSTALLLEVEL='1';ARPNOMODIFY='1';ARPCOMMENTS='Candidato para revisao local. Licenca publica e estabilidade pendentes.';ARPPRODUCTICON='';SecureCustomProperties='OLDPRODUCTS;NEWERPRODUCT;NOVAWIN10';REBOOT='ReallySuppress'}
$properties['ARPNOREPAIR']='1'
foreach($entry in $properties.GetEnumerator()){if($entry.Value -ne ''){Row 'Property' @('Property','Value') @($entry.Key,$entry.Value)}}
Row 'RegLocator' @('Signature_','Root','Key','Name','Type') @('WindowsMajor',2,'SOFTWARE\Microsoft\Windows NT\CurrentVersion','CurrentMajorVersionNumber',18)
Row 'AppSearch' @('Property','Signature_') @('NOVA_OS_MAJOR','WindowsMajor')
Row 'LaunchCondition' @('Condition','Description') @('Installed OR (VersionNT64 AND NOVA_OS_MAJOR = "#10")','Windows 10/11 x64 e necessario.')
Row 'CustomAction' @('Action','Type','Source','Target') @('ValidateNovaIdle',1,'SafetyActions','ValidatePlatformAndIdle')
Row 'LaunchCondition' @('Condition','Description') @('NOT ALLUSERS','Instalacao somente para o usuario atual; nao use ALLUSERS.')
Row 'LaunchCondition' @('Condition','Description') @('NOT NEWERPRODUCT','Uma versao mais recente ja esta instalada.')
$directoryColumns=@('Directory','Directory_Parent','DefaultDir')
Row 'Directory' $directoryColumns @('TARGETDIR',$null,'SourceDir')
Row 'Directory' $directoryColumns @('LocalAppDataFolder','TARGETDIR','.')
Row 'Directory' $directoryColumns @('ProgramsDir','LocalAppDataFolder','Programs')
Row 'Directory' $directoryColumns @('INSTALLDIR','ProgramsDir','NOVA DAW Alpha 0.7')
Row 'Directory' $directoryColumns @('ProgramMenuFolder','TARGETDIR','.')
Row 'Directory' $directoryColumns @('DesktopFolder','TARGETDIR','.')
Row 'Directory' $directoryColumns @('StartMenuDir','ProgramMenuFolder','NOVA DAW Alpha 0.7')
Row 'Feature' @('Feature','Title','Description','Display','Level','Directory_','Attributes') @('Main','NOVA DAW Alpha 0.7','Aplicativo, scanner, instrumento de teste, runtime e avisos',1,1,'INSTALLDIR',0)
$directories=@{''='INSTALLDIR'}
$files=Get-ChildItem -LiteralPath $Stage -File -Recurse | Sort-Object FullName
$ddf=New-Object 'Collections.Generic.List[string]'
foreach($line in @('.OPTION EXPLICIT','.Set Cabinet=ON','.Set Compress=ON','.Set CompressionType=LZX','.Set CompressionMemory=21','.Set MaxDiskSize=0','.Set CabinetNameTemplate=payload.cab',('.Set DiskDirectoryTemplate="'+$work+'"'),('.Set InfFileName="'+(Join-Path $work 'payload.inf')+'"'),('.Set RptFileName="'+(Join-Path $work 'payload.rpt')+'"'))){$ddf.Add($line)}
$index=0;$appId='';$uninstallId='';$mainComponent=''
foreach($file in $files){
 $index++;$relative=$file.FullName.Substring($Stage.Length+1);$folder=Split-Path $relative
 $digest=[Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes('NOVA DAW Alpha 0.7/'+$relative.ToLowerInvariant()))
 $identity=([BitConverter]::ToString($digest).Replace('-','')).Substring(0,24)
 $fileId='F'+$identity;$component='C'+$identity;$reg='R'+$identity
 $cursor='';$parent='INSTALLDIR';foreach($part in @($folder.Split('\')|Where-Object {$_})){if($cursor){$cursor+='\'};$cursor+=$part;if(!$directories.ContainsKey($cursor)){$id='D'+$directories.Count.ToString('D5');Row 'Directory' $directoryColumns @($id,$parent,$part);$directories[$cursor]=$id};$parent=$directories[$cursor]}
 $guid='{'+[guid]::new([byte[]]$digest[0..15]).ToString().ToUpperInvariant()+'}'
 Row 'Component' @('Component','ComponentId','Directory_','Attributes','KeyPath') @($component,$guid,$parent,260,$reg)
 Row 'Registry' @('Registry','Root','Key','Name','Value','Component_') @($reg,1,'Software\NOVA DAW\Alpha 0.7\Installer',$fileId,$relative,$component)
 Row 'FeatureComponents' @('Feature_','Component_') @('Main',$component)
 $version=if($file.Extension -in @('.exe','.dll')){$file.VersionInfo.FileVersion -replace '[^0-9.].*$',''}else{$null};if($version -notmatch '^\d+\.\d+\.\d+(\.\d+)?$'){$version=$null}
 Row 'File' @('File','Component_','FileName','FileSize','Version','Attributes','Sequence') @($fileId,$component,$file.Name,[int]$file.Length,$version,16384,$index)
 $ddf.Add('"'+$file.FullName+'" '+$fileId)
 if($relative -eq 'NOVA DAW.exe'){$appId=$fileId;$mainComponent=$component}
 if($relative -eq 'Desinstalar NOVA DAW Alpha 0.7.exe'){$uninstallId=$fileId}
}
if(!$appId -or !$uninstallId){throw 'Aplicativo ou desinstalador ausente.'}
foreach($id in @($directories.Values)+@('StartMenuDir')){Row 'CreateFolder' @('Directory_','Component_') @($id,$mainComponent)}
$shortcutColumns=@('Shortcut','Directory_','Name','Component_','Target','Description','ShowCmd','WkDir')
Row 'Shortcut' $shortcutColumns @('StartApp','StartMenuDir','NOVA DAW Alpha 0.7',$mainComponent,('[#'+$appId+']'),'NOVA DAW Alpha 0.7 - Public Test',1,'INSTALLDIR')
Row 'Shortcut' $shortcutColumns @('DesktopApp','DesktopFolder','NOVA DAW Alpha 0.7',$mainComponent,('[#'+$appId+']'),'NOVA DAW Alpha 0.7 - Public Test',1,'INSTALLDIR')
Row 'Shortcut' $shortcutColumns @('Uninstall','StartMenuDir','Desinstalar NOVA DAW Alpha 0.7',$mainComponent,('[#'+$uninstallId+']'),'Remover aplicativo preservando sessoes e dados',1,'INSTALLDIR')
Row 'Media' @('DiskId','LastSequence','Cabinet') @(1,$index,'#payload.cab')
Row 'Upgrade' @('UpgradeCode','VersionMax','Attributes','ActionProperty') @($properties.UpgradeCode,'0.7.0',512,'OLDPRODUCTS')
Row 'Upgrade' @('UpgradeCode','VersionMin','Attributes','ActionProperty') @($properties.UpgradeCode,'0.7.0',258,'NEWERPRODUCT')
$sequence=@{FindRelatedProducts=25;LaunchConditions=100;CostInitialize=800;FileCost=900;CostFinalize=1000;InstallValidate=1400;InstallInitialize=1500;RemoveExistingProducts=1501;ProcessComponents=1600;UnpublishFeatures=1800;RemoveRegistryValues=2600;RemoveShortcuts=3200;RemoveFiles=3500;RemoveFolders=3600;CreateFolders=3700;InstallFiles=4000;CreateShortcuts=4500;WriteRegistryValues=5000;RegisterUser=6000;RegisterProduct=6100;PublishFeatures=6300;PublishProduct=6400;InstallFinalize=6600}
foreach($entry in $sequence.GetEnumerator()){$condition=if($entry.Key -eq 'RemoveExistingProducts'){'OLDPRODUCTS'}else{$null};Row 'InstallExecuteSequence' @('Action','Condition','Sequence') @($entry.Key,$condition,[int]$entry.Value)}
Row 'InstallExecuteSequence' @('Action','Sequence') @('ValidateNovaIdle',1100)
Row 'InstallExecuteSequence' @('Action','Sequence') @('AppSearch',50)
$definition=Join-Path $work 'payload.ddf';[IO.File]::WriteAllLines($definition,$ddf,[Text.Encoding]::Default)
& "$env:SystemRoot\System32\makecab.exe" /F $definition | Out-Null
if($LASTEXITCODE -ne 0){throw 'makecab falhou.'}
$view=Invoke-Com $database 'OpenView' @('INSERT INTO `_Streams` (`Name`,`Data`) VALUES (?,?)')
$record=Invoke-Com $installer 'CreateRecord' @(2)
[void](Set-Com $record 'StringData' @(1,'payload.cab'))
[void](Invoke-Com $record 'SetStream' @(2,[string](Join-Path $work 'payload.cab')))
[void](Invoke-Com $view 'Execute' @($record))
[void](Invoke-Com $view 'Close')
$view=Invoke-Com $database 'OpenView' @('INSERT INTO `Binary` (`Name`,`Data`) VALUES (?,?)')
$record=Invoke-Com $installer 'CreateRecord' @(2)
[void](Set-Com $record 'StringData' @(1,'SafetyActions'))
[void](Invoke-Com $record 'SetStream' @(2,[string](Resolve-Path -LiteralPath $SafetyDll).Path))
[void](Invoke-Com $view 'Execute' @($record))
[void](Invoke-Com $view 'Close')
[void](Invoke-Com $database 'Commit')
$summary=$database.GetType().InvokeMember('SummaryInformation',[Reflection.BindingFlags]::GetProperty,$null,$database,@([int]20))
foreach($item in @(@(2,'NOVA DAW Alpha 0.7 - Public Test'),@(3,'Candidato de instalacao por usuario x64'),@(4,'Projeto SOM'),@(7,'x64;1033'),@(9,('{'+[guid]::NewGuid().ToString().ToUpperInvariant()+'}')),@(14,500),@(15,10),@(18,'NOVA DAW packaging'))){[void](Set-Com $summary 'Property' @($item[0],$item[1]))}
[void](Invoke-Com $summary 'Persist')
[void](Invoke-Com $database 'Commit')
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($summary)|Out-Null
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($database)|Out-Null
[Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)|Out-Null
Write-Output "MSI criado com $index arquivos: $Output"
