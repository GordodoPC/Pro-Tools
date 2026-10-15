# Retomada do desenvolvimento — 2026-10-15

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
| 7 | Implementação ainda não iniciada; não há escopo funcional aprovado neste roadmap | Definir requisitos e aceitação após avaliar as evidências e limitações anteriores |

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
