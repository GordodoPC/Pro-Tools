# NOVA DAW — roadmap

Autorização da Fase 7: solicitação do usuário em 2026-10-15. Continua o projeto
C++20/JUCE/CMake existente e preserva as sessões e funcionalidades anteriores.
Escopo das Fases 4–6: [roadmap-phases-4-6.md](roadmap-phases-4-6.md).

## Fase 7 — execução sequencial

1. Investigar prazos de callbacks, alocações, contenção e crescimento de memória;
   distinguir buffers/cache/heap de vazamentos; corrigir problemas comprovados e
   comparar ensaios antes/depois sem relaxar o critério `frames / sampleRate`.
2. Automação Read/Write/Touch/Latch de volume/pan/parâmetros, curvas editáveis,
   Undo/Redo e persistência.
3. VCA, grupos de canais/ganho, sidechain, sends avançados, Solo Safe, PDC e
   preparação de roteamento multicanal/surround.
4. LUFS momentâneo/integrado, True Peak, dinâmica/espectro, normalização,
   dither, WAV 16/24/float32, sample rates, render offline e stems.
5. Painéis funcionais integrados ao MIX para automação, sidechain, VCA,
   loudness/True Peak e exportação.
6. Testes numéricos/comportamentais, regressões, builds Debug/Release e CTest.
7. Relatório [phase7-report.md](phase7-report.md), estado do desenvolvimento,
   reprodução, artefatos, commits locais e pendências explícitas.

Regras: sem reinstalação/recriação, dependências novas ou alteração global;
sem push/comandos Git destrutivos. Render limitado, sem IO/alocações/lifecycle
ou espera bloqueante; alterações estruturais sob MutationGuard de controle.
Não apresentar simulação como hardware, scanner como certificação de terceiros
ou funcionalidades/testes pendentes como entregues.

## Estado em 0.7.0

| Etapa | Resultado | Limite/continuidade |
|---|---|---|
| 1 | Instrumentação, análise de memória, cache EQ/PDC e comparação 300 s executados | 4→2 atrasos na mesma carga; causa dos nove históricos não demonstrável; horas/ETW pendentes |
| 2 | Read/Write/Touch/Latch, volume/pan/inserts, curvas/Undo/Redo e sessão v7 | Captura a 30 Hz no host; gestos de editor VST3 externo pendentes |
| 3 | VCA/grupos, ganho/Solo Safe, sends avançados, sidechain nativo/PDC | Auxiliar VST3 pendente; matriz multicanal preparada, render/dispositivo stereo |
| 4 | LUFS M/I, estimador True Peak, crest/PLR, FFT, normalização/TPDF, WAV/rates/stems offline | Sem limiter/caudas automáticas ou certificação completa; stems de buses não lineares podem não somar ao mix |
| 5 | Painéis funcionais no MIX e Master | Validação manual prolongada e hardware adicional pendentes |
| 6 | Debug/Release, CTest15/15 e 593 checks+2smoke por build; dez execuções UI aprovadas | Não certifica plugins arbitrários ou captura física |
| 7 | Relatório, status, README e continuidade das fases anteriores atualizados | Métricas/artefatos e pendências em phase7-report.md |

As limitações permanecem parte do backlog; não são declaradas concluídas pelo
aceite funcional automatizado. Nenhuma fase anterior foi removida ou reiniciada.
