# NOVA DAW Alpha 0.7 — Public Test: candidato local

2026-10-15. Base preservada:c6874da. O autor escolheu preparar somente para
revisão local enquanto decide a licença. **Publicação não autorizada; não pronta
para GitHub Releases.** Nenhum push/upload/release foi executado.

## Aplicativo

Identificação Alpha0.7 no título, cabeçalho e Ajuda. Preferências/lista de plugins
passaram a `%LOCALAPPDATA%\NOVA DAW\Alpha 0.7`; gravações/render sem sessão salva
usam Workspace nessa pasta. Sessões e fontes existentes não foram migradas,
reescritas ou apagadas. Caminhos absolutos do workspace saíram da configuração
de execução. Instrumento próprio resolve primeiro Plugins ao lado do executável;
o fallback de desenvolvimento é relativo à árvore de build e não vale instalado.

Encontrada/corrigida notificação assíncrona de valores iniciais: BPM/compasso
marcavam sessão vazia como modificada. Valores iniciais de DSP/sends/grupos também
passaram a notificações silenciosas para não aplicar edições ao simplesmente
abrir painéis. Os bindings de alterações reais permanecem ativos.

Recurso Windows estava preso em0.3.0 apesar de Info.txt0.7.0. Corrigida dependência
CMake do RC na Info.txt; arquivo Release agora declara0.7.0. Não se modificou
JUCE fora do mecanismo de patches já verificado; os hashes continuam verificados.

## Diagnósticos

Ajuda abre Diagnostics ou gera relatório JSON. Logger copia UTF-8 para fila MPSC
prealocada de256 entradas/768 bytes, no máximo oito tentativas por produtor.
Overflow é contado; não há espera por escritor nem crescimento da fila.
Timer de controle a250ms escreve JSONL e rotaciona ao atingir2MiB antes do lote,
mantendo cinco arquivos (o último lote pode ultrapassar o limiar). Relatórios de
erro têm retenção16; texto fatal Windows também tem retenção16.

Callback de áudio não grava logs em disco. Exceção Windows já fatal usa handle
preaberto para texto mínimo de código/endereço/thread; isso ocorre na thread
da falha e não é contrato de recuperação em tempo real. Não captura áudio,
sessão inteira, stack dump ou minidump. Erro de disco/stack corrompida podem
impedir relato fatal. Marcador distingue encerramento limpo/incompleto.
Nenhum envio automático. Caminhos/dispositivo/OS podem aparecer; revisar dados
pessoais antes de compartilhar manualmente os arquivos.

29 checks novos de DiagnosticsAndPaths passaram em Debug/Release: quatro
produtores concorrentes, zero C++ new observado no produtor, overflow,
UTF-8/JSONL, persistência/rotação/retenção, fontes preservadas e shutdown.
Exceção Windows real controlada E04E4F56 produziu arquivo e cleanExit=false.

## Empacotamento

EXE x64 próprio com MSI/CAB incorporado, construído com Windows SDK/MSVC já
presentes. Não se instalou Inno/WiX/NSIS ou dependência nova. Bootstrap e
desinstalador usam CRT estático. Windows Installer faz registro, rollback,
atalhos, reparo e remoção de arquivos conhecidos. Instalação destinada ao usuário
em `%LOCALAPPDATA%\Programs\NOVA DAW Alpha 0.7`, sem drivers/serviços/plugins globais.
Contexto verificado por API Windows Installer USERUNMANAGED(2), não MACHINE(4);
não se infere contexto apenas da hive onde o serviço guarda metadados de ARP.

Componentes/IDs estáveis derivados do caminho; novos PackageCodes por pacote.
Iniciar/desktop apontam para executável instalado; scanner fica ao seu lado.
Reparo usa novamente o Setup; não depende do temporário já removido. Remoção
preserva dados/sessões/fontes e arquivos desconhecidos na pasta de instalação.
Proteção no EXE e ação nativa do MSI recusam instalação/reparo/remoção com DAW
instalada ou scanner em execução; não fecham processo nem descartam sessão.
O desinstalador inicia msiexec e sai antes de o Windows remover seu executável.

Primeiros ensaios encontraram falhas reais de autoria do MSI: condição VersionNT
moderno inadequada e tabela Signature ausente. Corrigidos antes do pacote final.
A checagem de Windows10/11 usa o EXE e AppSearch/RegLocator64, porque o host de
ações MSI não fornece a mesma versão de OS que o EXE normal. O teste inicial
que exigia registro em HKCU foi corrigido para verificar a API de contexto.
Falhas anteriores permanecem nas pastas alpha07-install-validation*; não são
apresentadas como instalação aprovada.

## Dependências/licenças

Inventário e avisos originais acompanham pacote. JUCE9.0.4 commit3fb5cbc, SBOM
SPDX original, licenças/COPYING/NOTICE/README e proveniência. VST3 SDK3.8MIT;
ASIO/VST2/AAX/AU/LV2/WebView/CURL não são ativados no alvo. Codecs/gráficos
embutidos seguem módulos/macros JUCE. Não se anuncia suporte de UI a codecs
apenas por estarem compilados. Fontes Windows/SDKs não foram copiados.

Dez DLLs MSVC x64 app-local do Redist14.51.36231 instalado: assinatura Microsoft,
versão e SHA256 inventariados. Não há DLL Debug, VC installer global nem download
no destino. Importações PE do bootstrap são somente Windows; runtime da aplicação
deve ser observado saindo da própria instalação. UCRT/DirectWrite/Direct2D/WASAPI
e Windows Installer pertencem ao Windows10/11.

Código NOVA DAW/Test Synth: licença do autor pendente. JUCE requer escolha de
AGPLv3 com fontes correspondentes ou licença comercial aplicável. Não se presume
contrato JUCE/Visual Studio nem se atribui licença ao código sem decisão do autor.
Fontes de revisão incluem arquivos do repositório e JUCE efetivamente usado,
incluindo adaptações; excluem .git, build, sessões, gravações e preferências.
Inventariar/copiar avisos não equivale a parecer jurídico ou autorização pública.

Referências primárias: [licença JUCE](https://juce.com/legal/juce-9-licence/),
[redistribuição MSVC](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files),
[DLLs redistribuíveis](https://learn.microsoft.com/en-us/cpp/windows/determining-which-dlls-to-redistribute),
[MSI por usuário](https://learn.microsoft.com/en-us/windows/win32/msi/authoring-packages-without-the-uac-dialog-box),
[contexto Product](https://learn.microsoft.com/en-us/windows/win32/msi/product-object).

## Evidência automática já executada

Primeiro ciclo: builds Debug/Release aprovados, CTest16/16 Debug35,57s e Release17,29s,
622 verificações+dois smoke por configuração. Dez UIs passaram em
`build/alpha07-ui-{editmix,midi,recording,advanced,mastering}-{debug,release}`.
Após inicialização silenciosa, Edit/Mix reexecutado/aprovado nas duas builds em
`build/alpha07-corrected-ui-editmix-{debug,release}`.
Instalação completa aprovada em `build/alpha07-install-validation6b`:
**26 verificações**, inclusive hashes de todos os arquivos do manifesto, atalhos,
contexto USERUNMANAGED, runtime MSVC carregado da instalação, título/versão,
inicialização/fechamento limpo, scanner VST3 próprio, quatro fluxos UI instalados,
exceção Windows/relançamento, reparo de scanner removido e desinstalação completa.
Recusa com DAW em uso passou no EXE, desinstalador e rota MSI/Aplicativos.
O teste espera log final de sucesso **e** remoção dos arquivos antes do aceite;
registro de produto pode desaparecer antes de terminar o engine de desinstalação.
Fontes desconhecidas/diagnósticos permaneceram. Arquivo sentinela é preservado
na antiga pasta da instalação como evidência; não se limpa recursivamente usuário.

Executável Release Alpha validado:
`build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe`, SHA256
`488815BB683BBA913768FDD6786D66CF86E9F48FB9CFF4740C1A23970FF03A32`.
O pacote final de revisão é gerado após registrar este código e será revalidado
pelo mesmo roteiro; hashes e resultado final serão registrados a seguir.

### Regressão Unicode detectada na revisão dos ZIPs

O pacote inicialmente aprovado em instalação ASCII falhou ao varrer VST3 em
pasta portátil comç: main/argv narrow era decodificado como UTF-8, embora o CRT
Windows fornecesse outra codificação. Corrigido para wmain/argv UTF-16 no scanner.
ScannerUnicodePaths copia bundle real para caminho acentuado e verifica processo,
metadata e caminho Unicode persistido em XML. **CTest final17/17 Debug40,52s e
Release19,01s**,625checks+dois smoke por configuração, todas as suítes preservadas.
Scanner Release SHA256`F5EDB3B6B9391B72E97037FEDBC09ABCA18FDA892DE1DB20C816951C93FA48F7`.
O pacote anterior não é o candidato final; revisão nova será gerada em
`build/releases/alpha-0.7-review-final` com esta correção e novas verificações.

## Critérios para liberar

RELEASE-STATUS.json mantém readyForPublication=false e reviewOnly=true.
Ainda exigidos: licença escolhida/documentada, revisão explícita do instalador,
aceite de estabilidade de áudio e testes em outro computador. Fase7 continua
funcionalmente implementada com os limites de [phase7-report.md](phase7-report.md):
dez overruns no último ensaio integrado300s; não se declara áudio crítico aprovado.
Esse número é histórico; este ciclo não o converte em zero por empacotar a aplicação.

Em outra máquina: Windows10/11 x64 limpo sem Visual Studio/VC runtime, usuário
sem admin, caminhos com acentos/OneDrive, offline, instalar/reparar/remover/atalhos,
rollback/cancelamento/disco cheio/permissão negada, sessões antigas com fontes,
horas com interface física, captura/MIDI/hotplug, plugins externos, medidor
independente e SmartScreen/antivírus. Sem assinatura do autor neste candidato.
