# NOVA DAW — progresso de execução

## 2026-10-14 — Fase 3.5

- Base 05c9ae9; PDC/guardas no commit validado b3e0a07.
- Auditoria do callback/wrapper, PDC e testes de lifecycle/concorrência adicionados.
- Diretórios VST3 personalizados persistentes; três padrões Windows ausentes.
- Último Debug/Release: builds aprovados, CTest 7/7, 333 verificações por build
  mais dois smoke tests. Medição final prolongada e documentação em fechamento.
- Fases 4/5/6 autorizadas e registradas em roadmap-phases-4-6.md e AGENTS.md.
- Próximo ciclo: concluir evidência da estabilização e iniciar motor MIDI/VST3i.

## 2026-10-14 — conclusão da Fase 3.5 e início da Fase 4

- Commits locais bb31717 (testes/correções/pastas) e f1b8934 (escopo/diretrizes).
- UI Debug/Release aprovada; Realtek 48000 Hz / 480 samples, callback não nulo.
- Reprodução final de 300 s concluída; métricas/violações de prazo registradas
  em phase3-stability-report.md, sem certificação de hardware/terceiros.
- Iniciado primeiro ciclo MIDI: dados musicais, PPQ/ticks/samples, agendamento
  limitado por bloco e importação/exportação SMF. Build/testes desse ciclo em curso.
- Piano Roll, gravação MIDI e instrumentos ainda não concluídos neste ciclo.

## 2026-10-14 — Fase 4, ciclo de dados validado

- PPQ 960, mensagens musicais e offsets por sample; filas de saída limitadas.
- SMF e estado do modelo com roundtrip, overflow/panic e validação de metadados.
- Builds Debug/Release aprovados, CTest 8/8 em ambos, 14 novas verificações.
- Próximo: instrumento VST3 original gerando áudio e integração MIDI no grafo.

## 2026-10-14 — Fase 4, integração e editor

- Instrumento VST3 real, motor/loop/seek/metrônomo, live MIDI, roteamento de
  dispositivos, captura com recuperação SMF e sessões v4 implementados.
- Piano Roll implementado; UI Debug/Release inicial passou com Realtek e notas reais.
- Última bateria antes dos ajustes finais: CTest 10/10 Debug e Release, 37 testes
  de instrumento/motor/captura mais 14 de dados e 13 de concorrência MIDI.
- Validando agora presets/CC/timestamps e últimos controles do editor.

## 2026-10-14 — conclusão funcional da Fase 4

- Versão 0.4.0: MIDI/Piano Roll/VST3i/SMF/metrônomo/entrada/captura e sessão v4.
- Builds Debug/Release aprovados; CTest 10/10 em ambos, 410 verificações mais smoke.
- UI com notas reais em Realtek passou; MIDI físico e terceiros permanecem pendentes.
- Próxima etapa autorizada: gravação de áudio multipista, punch e takes (Fase 5).

## 2026-10-14 — conclusão funcional da Fase 5

- 0.5.0: WAV multipista mono/stereo, inputs/arm/monitor opt-in, punch/roll,
  loop/takes, worker de disco/filas limitadas e recovery.json, sessão v5.
- Debug/Release builds e CTest 11/11; 448 verificações + dois smoke tests.
- UI nas duas configurações passou com captura simulada e sessão de duas faixas.
- Hardware multicanal/latência de entrada/ASIO/disco cheio real continuam pendentes.
- Próximo ciclo autorizado: Fase 6, edição de áudio, fades e render offline.

## 2026-10-14 — conclusão funcional da Fase 6 (0.6.0)

- Seleção múltipla, Slip/Ripple/duplicação/Undo, fades/crossfades que alteram sinal,
  editor visual, phase vocoder próprio stretch/pitch/cents, cache/preview/revert,
  comping e consolidação/bounce offline; sessão v6 compatível com 1–5.
- Builds Debug/Release aprovados, CTest 12/12; 503 verificações + dois smoke tests.
  Debug 29,97 s, Release 16,82 s; 55 novas verificações de edição/offline.
- UI fase6 Debug/Release e regressão Edit/Mix das duas builds passaram. Primeiro
  teste legado sem --session foi inválido por não carregar fixtures; repetição
  documentada com demo.novadaw passou. Não foi registrado como aprovação.
- Bounce próprio VST3 com track/bus/Master PDC 55 samples e pre-send/Read passou;
  VST3i gera WAV não nulo. Capture/fades/preview sem C++ new observado no render.
- Stress curto Release final: 10,020 s, 1180 callbacks, max6,944ms, 1 deadline overrun,
  CPU12,63% de um core, pico88.354.816 bytes. Ensaio longo 3.5 preservado: 300 s
  com sete violações; não se declara certificação hard real-time.
- Fontes WAV byte-for-byte preservadas em transforms/comp/consolidate/bounce.
- Hardware de entrada/MIDI físico/terceiros/qualidade perceptual e testes com IO
  real adverso continuam pendentes; detalhes e roteiros nos relatórios 3.5/4/5/6.
- Sem instalação, alteração global ou push. Fase5 no commit811ffeb; Fase4 86b44d5.

- Revisão final: falha de alocação injetada no snapshot de bounce tratada em ambos
  os builds, preservando a sessão; 55 checks de edição/offline, 503 no total.

- Entrega Fase 6 testada no commit local `0f7f605`; Fase 5 `811ffeb`, Fase 4
  `86b44d5`. Relatórios/evidências atualizados; nenhum push remoto executado.

## 2026-10-15 — retomada no computador original

- Antes de editar: lidos AGENTS/README, roadmap e todos os relatórios anteriores;
  Git limpo, HEAD `ccf7ec3` igual à main consultada por `git ls-remote`.
- Base recompilada Debug/Release: CTest inicial 12/12, 41,47 s / 19,96 s.
  Auditoria e estado de cada fase em [development-status.md](development-status.md).
- Corrigida recuperação repetida com faixa cheia e contagem de manifesto
  duplicado; 43 verificações de gravação. Material novo no limite continua recusado.
- Corrigido encerramento de transporte no bounce cancelado/com erro, mudança
  de latência no último bloco e fault mascarado por bypass automático;
  60 verificações offline. Bypass explícito e fontes originais preservados.
- Falhas de faixa cheia, cancelamento, latência e exceção reproduzidas antes de
  corrigir. Latência/exceção síncronas usam fixture interna; integração mantém
  VST3/VST3i próprios reais. Nenhuma certificação externa adicionada.
- Builds finais Debug/Release aprovados; CTest **12/12**, **36,78 s / 20,43 s**,
  **513 verificações + dois smoke tests** por configuração, zero new observado
  nos probes existentes de render/captura/fades/preview.
- Oito execuções de UI aprovadas, em sequência: Edit/Mix, MIDI, gravação e
  edição avançada × Debug/Release. Evidências em
  `build/resume-20261015-ui-{editmix,midi,recording,advanced}-{debug,release}`.
  Saída Realtek 48000 Hz/480 samples; captura simulada, MIDI físico não testado.
- Ensaio Release final, separado de builds/CTest/UI: **300,081 s**, 35380 callbacks,
  13819428 samples, CPU 21,04% de um núcleo, max render 19,082 ms, **9 violações
  de prazo**, zero new observado/erros de controle/silêncios por contenção VST,
  14 recalculos de latência, samples finitos. `passed=true`; memória privada
  inicial/final/pico 89309184 / 109375488 / 109404160 bytes. Artefato:
  `build/stability-resume-20261015-release-300s/stability-result.json`.
- Prazos/memória, hardware, terceiros, qualidade perceptual e ensaios de horas
  continuam pendentes. Fase 7 sem escopo funcional definido no roadmap atual;
  preparação e contratos registrados na auditoria. Diretórios padrão VST3
  continuam ausentes. Não foi declarada certificação hard real-time.
- Nenhum push/reset/rebase, instalação de dependências/plugins ou alteração global.
- Correções e regressões no commit local **a8df20d**. Executável Release:
  `build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe`;
  SHA256 `0976427221EC4B7572578C00D96789C77B4F64EE91DBD5A36BF35827F76DC21F`.
  Snapshot Release do editor avançado inspecionado visualmente.

## 2026-10-15 — Fase 7, ciclo de estabilidade

- Escopo explicitamente autorizado pelo usuário, registrado em roadmap.md;
  base 73f07da/a8df20d preservada. Relatório novo phase7-report.md.
- Instrumentação limitada de callbacks: percentis, cada overrun, ciclos,
  atraso de despertar, contexto de controle e silêncios por gate/manutenção.
  Memória: buffers conhecidos, C++ vivos e liberação após clear; sem forçar heap.
- Antes instrumentado 300,068 s: 4 overruns, max13,190ms, P50/P95/P99
  911/4254/5831us, CPU18,66%, zero new observado/controle/contenção VST.
  Private 92655616→110940160 bytes; após clear93257728; C++ vivos crescem44870
  bytes com histórico das medições. Limitações de atribuição registradas.
- EQ elimina coeficientes redundantes/divisões por amostra; PDC avança sem módulo.
  Builds Debug/Release e CTest12/12 aprovados, 516 checks + dois smoke por build.
  Comparação após correções em execução por300s com critérios originais.
- Fechamento da etapa1: depois300,063s, 2 overruns, max9,458ms,
  P50/P95/P99 908/4162/5554us, CPU18,44%, zero new/erros/contenção VST;
  private94027776→111779840, após clear95019008 bytes. Dados completos e
  limitações em phase7-report.md; CPU mudou pouco e dois atrasos permanecem.
- Callback Realtek medido nas UIs Edit/Mix e MIDI das duas builds: zero overrun;
  Release max274,6/127,7us, Debug3332,5/999,9us. Cenas menores, sem certificação
  de carga máxima. CTest final Debug12/12 31,16s; Release12/12 17,12s.

## 2026-10-15 — Fase 7, automação avançada

- Etapa1 no commit9ff7c27, preservando os dois commits anteriores e os prazos.
- Read/Write/Touch/Latch, volume/pan/parâmetros, curvas gráficas/textuais,
  histórico próprio limitado e sessão v7 compatível implementados.
- Captura UI30Hz fora do callback; máscaras atômicas no render; 64 lanes/faixa,
  4096 pontos/lane e65536 pendentes. Limites/overflow em phase7-report.md.
- AdvancedAutomation23 checks aprovados em Debug/Release, incluindo VST3 real.
  Regressão Debug13/13 passou; Release em execução neste registro intermediário.
- Fechamento: Debug13/13 40,61s e Release13/13 22,95s, 539 checks + dois smoke;
  UI Edit/Mix/automação nas duas builds passou em build/phase7-automation-ui-*.

## 2026-10-15 — Fase 7, VCA/grupos/sidechain/sends

- Automação no commit9d759da. Oito VCAs/oito grupos, Solo Safe, sends pan/width/
  mute e key externo no Compressor/Gate nativo, com grafo/PDC e persistência.
- Matriz mono/stereo/5.1/7.1 preparada; IO/playback atual segue stereo.
-19 checks novos; CTest Debug14/14 33,01s e Release14/14 18,46s, 558 + dois smoke.
  UI com curva/Undo e grupos passou em build/phase7-mix-final-ui-*.
- Regressão de pilha Debug corrigida movendo estado adicional para heap de
  controle; nenhuma pilha/critério de teste aumentado. Limites em phase7-report.md.

## 2026-10-15 — Fase 7, masterização e integração

- Medidor K-weighted LUFS M/I com gating, FIR4x True Peak, crest/PLR e FFT2048;
  captura limitada/prealocada, FFT/histórico/gating integrado fora do render.
- Snapshot/worker exporta WAV PCM16/24/float32, rates8–192k, normalização com
  teto estimado, TPDF e stems através de buses/Master. Fontes preservadas;
  cancelamento/no-overwrite e persistência de opções em sessão v7.
- Painel real Master LUFS / Export; checks numéricos/decodificação35 aprovados.
  CTest Debug15/15 (47,55s) e Release15/15 (19,08s),593checks+2smoke por build.
- Dez execuções UI passaram nas duas configurações; snapshots MIX/Master
  inspecionados. Ensaio integrado300s com recursos7 habilitados em execução,
  separado da carga comparativa anterior e sem compilação concorrente.
- Revisão final encontrou slider de automação sem notificação dirty: corrigidos
  callback de alteração e refresh de curva/modo após Aplicar/Undo; validação UI
  agora exige o binding do slider e notificação. Rebuild/checagem após o ensaio.
- Ensaio integrado inicial terminou:300,090s,35379callbacks,32overruns,
  max55,757ms,CPU23,02%, zero new/controle/contenção VST/overflow medição/automação.
  Carga diferente da comparação4→2: novos recursos habilitados explicitamente.
  Private92631040→110497792, após clear91865088. Não aprovado quanto a prazos.
- Medido custo do medidor: mediana143,568us por480frames; FIR SSE2/double com
  ring espelhado manteve fases/taps e passou35checks, mediana43,0453us (~70%
  menor wall time isolado). Sem relaxar critérios. UI corrigida Debug/Release
  passou; rebuilds completos passaram e regressão/novo300s serão registrados.

## 2026-10-15 — entrega 0.7.0 validada

- Implementação de masterização/UI no commit local805c5e7, após9ff7c27,
  9d759da,a3bf187; a8df20d/73f07da preservados. Sem push/reset/rebase/instalação.
- CTest final após FIR/UI:15/15 Debug46,40s e Release20,59s,593checks+2smoke
  por build. Dez UIs de entrega passaram em build/phase7-delivery-ui-*.
- Ensaio final sem builds/testes/UI concorrentes:300,083s,35375callbacks,
  13817475samples,10overruns,max26,244ms,P50/P95/P99 1002/4506/6584us,
  CPU20,36%, renderCPU54,813s, zero new C++ observado/controle/contenção VST;
  13recalculos de latência, gate594/manutenção4100. Funcionalmente aprovado,
  **estabilização de prazo parcial**, sem hard real-time certificado.
- Private92938240→112209920,pico112623616, após clear88608768; C++ amostra0/300
  8472182/8523723 bytes. Artefato build/phase7-delivery-stability-300s.
  Medidor final drops0 é desde último Reset; não acumula entre trocas de rate/sessão.
- Release:build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe;
  SHA2561FD56437C10993E5428216177B54CCB1D72030ACBFF2144858C19E77C2557A05.
  Resultados/cargas/limitações/roteiro manual no phase7-report.md.

## 2026-10-15 — NOVA DAW Alpha0.7 Public Test, revisão local

- Autor escolheu preparar somente para revisão local enquanto decide licença;
  não foi adicionada licença ao código e não haverá publicação automática.
- AppPaths elimina dependência do workspace: LocalAppData/Alpha0.7 para dados,
  Workspace para fontes sem sessão salva, instrumento próprio relativo ao exe.
- Identificação Alpha no app; RC0.3.0 obsoleto corrigido com dependência na Info.txt.
  BPM/compasso e painéis nativos usam inicialização silenciosa; sem edição ao abrir.
- Logger MPSC256x768/8tentativas, overflow/rotação limitada no controle, relatório
  de operações/engine e exceção Windows real mínima. Nenhum envio automático.
- Build Debug/Release aprovados; CTest16/16:35,57s/17,29s,622checks+2smoke por
  configuração. Dez UIs passaram, Edit/Mix corrigido reexecutado nas duas builds.
- EXE/MSI/CAB e desinstalador nativos, atalhos por usuário, dez DLLs MSVC locais
  com assinatura verificada. Sem instalar WiX/Inno/NSIS/dependências, drivers,
  serviços ou plugins globais. Avisos/SBOM/fontes de revisão incluídos.
- Instalador validado em26checks: contextoMSI2(não4), hashes/atalhos/runtime local,
  início/fechamento, scanner/instrumento, quatro fluxos UI, crash/relançamento,
  recusa de remoção em uso, reparo e desinstalação completa preservando fontes/logs.
  Artefato:build/alpha07-install-validation6b/installation-result.json.
- Falhas de autoria MSI corrigidas antes do aceite. Teste agora espera msiexec
  completar, além de o registro desaparecer; sentinela é mantida sem limpeza recursiva.
- Release Alpha SHA256488815BB683BBA913768FDD6786D66CF86E9F48FB9CFF4740C1A23970FF03A32.
  Publicação continua bloqueada: licença/revisão, dez overruns históricos e outro PC.

## 2026-10-15 — correção Unicode antes de entregar Alpha

- Instalação do candidato aa241ea passou26checks; teste adicional de ZIP em
  caminho comç revelou argv narrow interpretado como UTF-8 pelo scanner.
- Corrigido PluginScanner para wmain UTF-16, incluindo entrada VST3 e saída XML.
  ScannerUnicodePaths acrescenta3checks reais e falhou antes da correção no
  pacote portátil. Não se declarou esse pacote antigo pronto.
- Build Debug/Release e CTest17/17 aprovados:40,52s/19,01s,625checks+2smoke.
  Alpha exe permaneceSHA488815BB683BBA913768FDD6786D66CF86E9F48FB9CFF4740C1A23970FF03A32;
  scanner corrigidoF5EDB3B6B9391B72E97037FEDBC09ABCA18FDA892DE1DB20C816951C93FA48F7.
- Novo pacote em build/releases/alpha-0.7-review-final sera revalidado antes de
  informar caminhos definitivos; publicacao continua bloqueada para revisao local.
