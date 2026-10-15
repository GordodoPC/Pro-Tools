param([string]$OutputDirectory='build/releases/alpha-0.7-review')
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location -LiteralPath $root
$output=[IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
if(Test-Path -LiteralPath $output){throw "Destino ja existe; escolha outra pasta para preservar a revisao: $output"}
$work=Join-Path $root ('build/alpha-packaging/'+[guid]::NewGuid().ToString('N'))
$stage=Join-Path $work 'app';New-Item -ItemType Directory -Path $stage -Force|Out-Null
$utf8=New-Object Text.UTF8Encoding($false)
function Run-Dev([string[]]$Arguments){& "$root/scripts/dev-shell.cmd" @Arguments;if($LASTEXITCODE -ne 0){throw "Build nativo falhou: $Arguments"}}
$nativeRelative='build/alpha-packaging/'+(Split-Path $work -Leaf)+'/native'
Run-Dev @('cmake','-S','packaging/windows/native','-B',$nativeRelative,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release')
Run-Dev @('cmake','--build',$nativeRelative,'--target','NovaUninstall','NovaInstallSafety')
$release=Join-Path $root 'build/ninja-release/NovaDAW_artefacts/Release'
foreach($name in @('NOVA DAW.exe','NovaPluginScanner.exe')){Copy-Item -LiteralPath (Join-Path $release $name) -Destination $stage}
Copy-Item -LiteralPath (Join-Path $work 'native/Desinstalar NOVA DAW Alpha 0.7.exe') -Destination $stage
$plugins=Join-Path $stage 'Plugins';New-Item -ItemType Directory -Path $plugins|Out-Null
Copy-Item -LiteralPath 'build/ninja-release/NovaTestSynth_artefacts/Release/VST3/NOVA Test Synth.vst3' -Destination $plugins -Recurse
$redist=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/18/BuildTools/VC/Redist/MSVC'
$crt=Get-ChildItem -LiteralPath $redist -Directory | Where-Object Name -Match '^\d+\.' | Sort-Object {[version]$_.Name} -Descending | ForEach-Object {Join-Path $_.FullName 'x64/Microsoft.VC145.CRT'} | Where-Object {Test-Path -LiteralPath $_} | Select-Object -First 1
if(!$crt){throw 'Redistribuiveis MSVC x64 nao encontrados; nao baixar/instalar automaticamente.'}
$runtime=@();foreach($file in Get-ChildItem -LiteralPath $crt -Filter '*.dll'){$signature=Get-AuthenticodeSignature -LiteralPath $file.FullName;if($signature.Status -ne 'Valid'){throw "Assinatura de runtime invalida: $($file.Name)"};Copy-Item -LiteralPath $file.FullName -Destination $stage;$runtime+=@{name=$file.Name;version=$file.VersionInfo.FileVersion;sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash;signer=$signature.SignerCertificate.Subject}}
Copy-Item -LiteralPath packaging/README-TESTADORES.md,packaging/RELEASE-NOTES.md -Destination $stage
$licenses=Join-Path $stage 'Licenses';New-Item -ItemType Directory -Path $licenses|Out-Null
Copy-Item -LiteralPath external/JUCE/LICENSE.md,external/JUCE/JUCE.spdx.json -Destination $licenses
$licenseFiles=Get-ChildItem external/JUCE/modules -File -Recurse | Where-Object {$_.Name -match '^(LICENSE.*|LICENCE.*|COPYING.*|NOTICE.*|README|JUCE_UPSTREAM\.txt)$'}
foreach($file in $licenseFiles){$relative=$file.FullName.Substring((Join-Path $root 'external/JUCE/').Length);$target=Join-Path $licenses $relative;New-Item -ItemType Directory -Path (Split-Path $target) -Force|Out-Null;Copy-Item -LiteralPath $file.FullName -Destination $target}
$sbom=Get-Content external/JUCE/JUCE.spdx.json -Encoding UTF8 -Raw|ConvertFrom-Json
$notes=@('# Licencas e dependencias - candidato de revisao local','','Licenca do codigo NOVA DAW: pendente, conforme decisao do autor. Nenhuma licenca foi atribuida automaticamente. Publicacao bloqueada.','','JUCE9.0.4: AGPL-3.0-only OU licenca comercial JUCE. A modalidade publica ainda nao foi escolhida. Inventario completo original: Licenses/JUCE.spdx.json. Os avisos originais foram preservados; nem todo SDK do inventario e compilado neste alvo Windows.','','| Componente JUCE vendorizado | Versao | Licenca declarada |','|---|---|---|')
foreach($package in $sbom.packages){$notes+=('| '+$package.name+' | '+$package.versionInfo+' | '+$package.licenseConcluded+' |')}
$notes+=@('','Runtime MSVC x64: DLLs redistribuiveis do Visual Studio instalado, com assinaturas Microsoft verificadas, distribuicao app-local. Nao inclui DLLs de Debug ou SDKs de desenvolvimento. Bootstrap/desinstalador usam CRT estatico. Os direitos de redistribuicao dependem dos termos Visual Studio aplicaveis ao distribuidor; esta coleta nao certifica contrato comercial.','Windows/UCRT/DirectWrite/Direct2D/WASAPI/Windows Installer: componentes do Windows10/11; nao copiados do System32. Sem WebView2/browser/CURL/ASIO/VST2/AAX/AU/LV2 no alvo.','Bibliotecas embutidas relevantes: zlib, HarfBuzz, SheenBidi, LunaSVG/PlutoVG, libpng/JPEG/WebP, FLAC/Ogg/Vorbis/Opus e SDK VST3 MIT; ver headers/avisos e inventario. Ativacao efetiva de codecs segue macros JUCE do build.','Nenhum plugin de terceiros e incluido. NOVA Test Synth e codigo original do repositorio; a licenca acompanha a decisao do autor.','Fontes e licencas precisam acompanhar uma eventual distribuicao AGPL. O arquivo de fontes de revisao inclui o JUCE e patches usados; nao e autorizacao de publicacao.','','Referencias primarias:','https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files','https://learn.microsoft.com/en-us/cpp/windows/determining-which-dlls-to-redistribute','https://juce.com/legal/juce-9-licence/','https://www.gnu.org/licenses/agpl-3.0.html')
[IO.File]::WriteAllLines((Join-Path $stage 'THIRD-PARTY-NOTICES.md'),$notes,$utf8)
$inventory=@{release='NOVA DAW Alpha 0.7 - Public Test';licenseDecision='Pending local review';juceCommit=(Get-Content dependencies.json -Raw|ConvertFrom-Json).JUCE.commit;runtime=$runtime;upstreamSBOM='Licenses/JUCE.spdx.json'}
[IO.File]::WriteAllText((Join-Path $stage 'dependency-inventory.json'),($inventory|ConvertTo-Json -Depth 8),$utf8)
$manifest=@(Get-ChildItem -LiteralPath $stage -File -Recurse|ForEach-Object {@{path=$_.FullName.Substring($stage.Length+1).Replace('\','/');size=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}})
[IO.File]::WriteAllText((Join-Path $stage 'installed-files.json'),($manifest|ConvertTo-Json -Depth 4),$utf8)
$msi=Join-Path $work 'NOVA-DAW-Alpha-0.7-Public-Test-x64.msi'
& "$PSScriptRoot/new-alpha-msi.ps1" -Stage $stage -Output $msi -SafetyDll (Join-Path $work 'native/NovaInstallSafety.dll')
Run-Dev @('cmake','-S','packaging/windows/native','-B',$nativeRelative,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',('-DPAYLOAD_MSI:FILEPATH='+$msi.Replace('\','/')))
Run-Dev @('cmake','--build',$nativeRelative,'--target','NovaSetup')
$artifacts=Join-Path $work 'artifacts';New-Item -ItemType Directory -Path $artifacts|Out-Null
Copy-Item -LiteralPath $msi,(Join-Path $work 'native/NOVA-DAW-Alpha-0.7-Public-Test-x64-Setup.exe'),(Join-Path $work 'native/Desinstalar NOVA DAW Alpha 0.7.exe') -Destination $artifacts
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath (Join-Path $artifacts 'NOVA-DAW-Alpha-0.7-Public-Test-x64-portable.zip') -CompressionLevel Optimal
# Explicit relative allowlist: source files only, no .git, sessions, captured audio or user settings.
$sourceList=@(& git ls-files --cached --others --exclude-standard);if($LASTEXITCODE -ne 0){throw 'Falha ao enumerar fontes NOVA.'}
# A tracked file can still be a local runtime cache. Restrict archive roots too.
$sourceList=@($sourceList|Where-Object {$_ -cmatch '^(\.gitignore|AGENTS\.md|CMakeLists\.txt|CMakePresets\.json|dependencies\.json|README\.md)$|^(Source|tests|cmake|scripts|patches|packaging|docs)/'})
$juceFiles=@(& git -C external/JUCE ls-files);if($LASTEXITCODE -ne 0){throw 'Falha ao enumerar fontes JUCE.'}
$sourceList+=@($juceFiles|ForEach-Object {'external/JUCE/'+$_})
$list=Join-Path $work 'source-files.txt';[IO.File]::WriteAllLines($list,$sourceList,$utf8)
& tar.exe -a -cf (Join-Path $artifacts 'NOVA-DAW-Alpha-0.7-source-review.zip') -T $list
if($LASTEXITCODE -ne 0){throw 'Falha ao arquivar fontes de revisao.'}
Copy-Item -LiteralPath (Join-Path $stage 'README-TESTADORES.md'),(Join-Path $stage 'THIRD-PARTY-NOTICES.md'),(Join-Path $stage 'dependency-inventory.json'),packaging/RELEASE-NOTES.md -Destination $artifacts
$state=@{release='NOVA DAW Alpha 0.7 - Public Test';readyForPublication=$false;reviewOnly=$true;sourceCommit=(& git rev-parse HEAD);workingTreeDirty=([bool](& git status --porcelain));blockingReasons=@('Licenca publica pendente por decisao do autor','Revisao explicita do instalador pendente','Estabilizacao de prazos de audio ainda parcial','Testes em outro computador pendentes');tests='Consultar docs/alpha07-release-report.md; artefatos nao implicam aprovacao de testes';noAutomaticPublication=$true;authenticodeSigned=$false}
[IO.File]::WriteAllText((Join-Path $artifacts 'RELEASE-STATUS.json'),($state|ConvertTo-Json -Depth 5),$utf8)
$hashes=Get-ChildItem -LiteralPath $artifacts -File|Sort-Object Name|ForEach-Object {(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+$_.Name}
[IO.File]::WriteAllLines((Join-Path $artifacts 'SHA256SUMS.txt'),$hashes,$utf8)
New-Item -ItemType Directory -Path $output -Force|Out-Null
Get-ChildItem -LiteralPath $artifacts -File|Copy-Item -Destination $output
Write-Output "Candidato LOCAL para revisao, nao publicar: $output"
