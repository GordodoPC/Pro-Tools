# Estado do desenvolvimento — 2026-10-15, versão 0.7.0

Alpha0.7 Public Test: candidato **local**, licença em decisão e publicação bloqueada.
Instalador EXE/MSI, desinstalador, atalhos/runtime e diagnósticos implementados;
CTest16/16 Debug/Release,622checks+2smoke, dez UIs e26checks de instalação aprovados.
Estado atual de entrega: [alpha07-release-report.md](alpha07-release-report.md).
Fase7 permanece parcial quanto a prazos; empacotamento não encerra essa pendência.

Entrega atual: automação Write/Touch/Latch/Read e curvas/Undo, VCA/grupos,
sidechain nativo com PDC, Solo Safe/sends avançados e masterização/exportação
integradas ao MIX. Builds Debug/Release e CTest **15/15** nas duas configurações,
**593 verificações + dois smoke tests** por build. Tempos: 46,40 s / 20,59 s.
Dez validações de UI passaram: Edit/Mix, MIDI, gravação, edição e masterização
em cada configuração, com WAV real exportado e configurações reabertas.
Artefatos: `build/phase7-delivery-ui-{editmix,midi,recording,advanced,mastering}-{debug,release}`.
Relatório atual e limites: [phase7-report.md](phase7-report.md).

Estabilização investigada: comparação de 300 s com a mesma carga passou de
4 para 2 violações, após cache EQ/PDC; os nove atrasos históricos não tinham
instrumentação individual. Crescimento privado foi separado de objetos vivos e
retenção de heap/cache; não há comprovação de leak geral nem de ausência dele.
Persistem prazos ocasionais, investigação de escalonamento e ensaios de horas.
Na carga integrada com recursos7, a otimização FIR passou32→10 violações em300s,
max55,757→26,244ms, CPU23,02→20,36%. **Estabilidade de prazo permanece parcial**.
Último artefato: `build/phase7-delivery-stability-300s/stability-result.json`;
zero new C++ observado/controle/contenção VST; private após clear88608768 bytes.

Sessões atuais v7 leem v1–6. Master/plugin IO/dispositivo continuam stereo;
matriz 5.1/7.1 está preparada. Sidechain externo opera nos DSPs nativos; buses
auxiliares VST3 não estão habilitados. Automação captura gestos a 30 Hz pelo
host; gestos de editores VST3 externos não são capturados diretamente.
Medição foi validada numericamente, sem certificação ITU/EBU completa.
Hardware de captura/MIDI físico, terceiros e audição prolongada seguem pendentes.

O registro abaixo preserva a auditoria e resultados históricos da retomada.
Executável atual: `build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe`;
SHA256 Alpha `488815BB683BBA913768FDD6786D66CF86E9F48FB9CFF4740C1A23970FF03A32`.
Commits locais da Fase7:9ff7c27,9d759da,a3bf187,805c5e7; base anterior preservada.
Main remota consultada novamente ao fechar:ccf7ec34bd1577bd19a9bbd57b12e53b7861c755,
sem alteração em relação à auditoria. Nenhum conteúdo remoto substituiu arquivos locais.

## Base conferida antes das alterações

Lidos AGENTS.md, README, relatórios das Fases 1/2/3/3.5/4/5/6, roadmap 4–6,
notas de implementação MIDI, progresso e diagnóstico REA. Workspace inicialmente
limpo. `git status`, `git log -12`, `git remote -v`, `git ls-remote origin
refs/heads/main`, `git rev-parse HEAD` e `git diff --stat origin/main` conferidos.

HEAD local e main consultada no GitHub: **ccf7ec34bd1577bd19a9bbd57b12e53b7861c755**.
Nenhum fetch/merge/reset/rebase/push foi necessário; nenhuma alteração local
precisou ser substituída. Últimas entregas: MIDI `86b44d5`, gravação `811ffeb`,
edição/offline `0f7f605`, registro de entrega `ccf7ec3`.

Builds existentes Ninja x64 Debug/Release aprovados antes da edição do código.
CTest inicial: **12/12 Debug (41,47 s)** e **12/12 Release (19,96 s)**.
Esses tempos foram medidos durante a auditoria, com atividade concorrente de
build/testes; não são benchmark isolado de desempenho.

## Situação por fase

| Fase | Funcionalidade | Validação ainda parcial ou pendente |
|---|---|---|
| 1/2 | Implementadas; áudio/sessões/Edit/Mix e edição básica preservados | Diálogos nativos e audição física completa |
| 3/3.5 | Mixer/DSP/Read/VST3/PDC e estabilização crítica implementados | Terceiros, horas com dispositivo, roundtrip, limites de prazo e crescimento de memória |
| 4 | MIDI/Piano Roll/VST3i/SMF/captura implementados | Entrada MIDI física, jitter/hotplug e instrumentos externos |
| 5 | Captura multipista/punch/loop/takes/recuperação implementados | Entradas físicas multicanal, calibração de latência, desconexão e disco cheio real |
| 6 | Fades/slip/ripple/comp/stretch/pitch/consolidate/bounce implementados | Audição de material complexo, sessões longas e falhas reais de IO/plugins |
| 7 | Recursos implementados e integrados; 15 testes e cinco modos UI por configuração aprovados | Prazos/ensaios de horas, validação completa de medição, gestos externos VST3 e sidechain auxiliar VST3; surround preparado na API |

O aceite funcional das Fases 4–6 já tinha evidência automatizada, incluindo
áudio de instrumentos reais próprios e WAVs reais de entradas simuladas. Isso
não equivale à conclusão da validação física. Compatibilidade externa indisponível
continua sem bloquear correções independentes nessas fases.

## Ciclo de correções desta retomada

- Recuperação repetida em faixa com 128 takes passa a reconhecer material já
  importado antes de recusar falta de espaço. Entradas repetidas do mesmo
  arquivo/faixa no manifesto consomem apenas um slot. Um take realmente novo
  continua sendo recusado no limite, preservando arranjo e fontes.
- Bounce passa a parar/resetar o transporte offline também em cancelamento e
  erro, fora do callback. Arquivos temporários continuam pertencendo ao job.
- Mudança de latência reportada durante o último bloco aborta a publicação.
  Exceção de processamento não é mais aceita como sucesso pelo bypass automático.
  Bypass explícito de insert ausente continua admitindo sinal seco compensado.
- Regressões reproduziram falhas antes das correções. Transições síncronas de
  latência/exceção usam fixture JUCE interna controlada; testes existentes mantêm
  processamento de módulos VST3/VST3i próprios reais. Não certificam terceiros
  nem notificações que só chegam depois do término do render.

## Preparação para a Fase 7

Nota de atualização: a situação sem escopo descrita nesta seção era a da primeira
retomada. Uma nova solicitação explícita autorizou a Fase 7, agora registrada em
[roadmap.md](roadmap.md), com execução em [phase7-report.md](phase7-report.md).

A base continua C++20/JUCE 9.0.4/CMake, sessão v6 compatível com v1–5, um
proprietário de MutationGuard, filas/buffers limitados e trabalho de disco fora
do callback. A próxima fase deve preservar esses contratos e os testes existentes.

Não existe especificação da Fase 7 nos documentos atuais. ASIO, streaming,
Write/Touch/Latch, automação gráfica/Master/sends, caudas musicais depois do fim
e isolamento de plugins são pendências históricas; não constituem automaticamente
o escopo da Fase 7. A preparação registra essa lacuna sem anunciar tais recursos
como implementados nem substituir o roadmap autorizado.

Antes de ampliar a arquitetura: manter regressões 4–6 aprovadas, revisar os
ensaios de estabilidade, e executar/registrar os roteiros físicos disponíveis.
Medições pendentes e limites conhecidos permanecem nos relatórios de cada fase.

## Validação do ciclo

Correções e regressões no commit local **a8df20d**. Builds Ninja x64 Debug e
Release aprovados. CTest final: **12/12 Debug (36,78 s)** e **12/12 Release
(20,43 s)**, **513 verificações + dois smoke tests** em cada configuração.
Gravação: 43 verificações; edição avançada: 60; demais fases: 410.

Oito execuções da interface passaram, uma por vez: Edit/Mix, MIDI, gravação e
edição avançada nas duas configurações. JSONs e snapshots em
`build/resume-20261015-ui-{editmix,midi,recording,advanced}-{debug,release}/`.
Edit/Mix e instrumento próprio produziram callback de saída Realtek não nulo,
48000 Hz/480 samples. Captura usa entradas simuladas; MIDI físico/terceiros
não foram exercitados. Salvar/reabrir e bindings passaram em todos os modos.

Executável Release validado:
`build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe`.
SHA256: `0976427221EC4B7572578C00D96789C77B4F64EE91DBD5A36BF35827F76DC21F`.
Debug: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe`.
CTest: `build/{ninja-debug,ninja-release}/Testing/Temporary/LastTest.log`.

Ensaio prolongado Release aprovado funcionalmente: 300,081 s, 35380 callbacks,
13819428 samples, CPU 21,04% de um núcleo, max render 19,082 ms, **9 violações
de prazo**, zero new observado/erros de controle, 14 recalculos de latência.
Memória privada inicial/final/pico amostrado: 89309184 / 109375488 / 109404160
bytes. Crescimento e prazos continuam pendências de estabilização; nenhuma
certificação hard real-time ou ausência de leaks é declarada.
Artefato: `build/stability-resume-20261015-release-300s/stability-result.json`.
Detalhes e interpretação em [phase3-stability-report.md](phase3-stability-report.md).
O snapshot Release do editor avançado também foi inspecionado visualmente.
