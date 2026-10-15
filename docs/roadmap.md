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
