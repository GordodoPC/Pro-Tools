$ErrorActionPreference = 'Stop'
$novaProjectRoot = Split-Path -Parent $PSScriptRoot
$gitExecutable = 'C:\Program Files\Git\cmd\git.exe'
if (-not (Test-Path -LiteralPath $gitExecutable)) {
    $gitExecutable = (Get-Command git -ErrorAction Stop).Source
}
$dependencyPath = Join-Path $novaProjectRoot 'external\JUCE'
$expectedCommit = '3fb5cbc0340758596dac39f7c8de47d562b4896e'
if (-not (Test-Path -LiteralPath $dependencyPath)) {
    New-Item -ItemType Directory -Path (Join-Path $novaProjectRoot 'external') -Force | Out-Null
    & $gitExecutable clone --branch 9.0.4 --depth 1 https://github.com/juce-framework/JUCE.git $dependencyPath
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao obter JUCE do repositorio oficial.' }
}
$actualCommit = & $gitExecutable -C $dependencyPath rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actualCommit.Trim() -ne $expectedCommit) {
    throw 'A copia local do JUCE difere da versao fixada. Nenhum arquivo foi substituido.'
}
Write-Output "JUCE 9.0.4 validado: $actualCommit"
