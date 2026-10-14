# NOVA DAW — Fase 3.5: estabilização

Referência: `05c9ae9e57e9d1719ea7d42b9f4dc93e1b1fd5ef`, workspace limpo.
Data: 2026-10-14. Referência CTest Debug: 5/5, 177 verificações.

## Auditoria inicial

Inspecionados callback, grafo, automação, ciclo de vida/UI/sessões e JUCE 9.0.4.
O código próprio não faz I/O de disco no render nem destrói inserts nele.
Buffers e DSP nativo são preparados fora do callback. Estruturas são alteradas
com callback de hardware desanexado; essa regra ainda dependia dos chamadores.

Achados concretos:

- `processBlock`/`processBlockBypassed` VST3 esperam em `processMutex` do JUCE.
- `MidiEventList` usa `CriticalSection` mesmo para listas vazias.
- Restarts emitidos pelo plugin podem postar mensagens com AsyncUpdater.
- Notificações de parâmetros de saída podem chamar listeners da interface no áudio.
- Getter de suspensão JUCE lê estado não atômico.
- Preparação repetida em editStructure e audioDeviceAboutToStart; callbacks de
  início de dispositivo não devem executar lifecycle VST3 arbitrariamente.
- Bypass mistura sinal seco sem compensar a latência do insert.
- Rotas/sends não alinham caminhos e automação de buses usa tempo adiantado.
- Mudanças de latência ainda não são observadas nem aplicadas ao grafo.

Implementação e validação em andamento. Nenhum plugin externo arbitrário está
certificado como seguro por estes testes. Fase 4/MIDI não foi iniciada.

## Primeiro ciclo validado

Build Ninja Debug e CTest 6/6 aprovados: regressões anteriores e 74 verificações
de PDC, com VST3 de teste compilado que reporta e realiza atrasos de
0, 1, 17, 255, 256 e 511 amostras. Impulsos alinhados em faixas paralelas,
buses encadeados, Master, sends pré/pós, bypass e placeholder de plugin ausente.
Automação de fader acompanha o instante de origem do sinal atrasado.

Adaptações de JUCE reproduzidas por CMake a partir de
`patches/juce-9.0.4/realtime.json`: hashes normalizados de entrada/saída,
verificação de todos os arquivos antes de escrever, recusa de versões/edições
desconhecidas. O commit upstream permanece fixado. As mudanças são de código
aberto/licenciado, sem alterar módulos comerciais.

Processamento usa try-lock com silêncio e contador se houver contenção,
eventos com armazenamento limitado e ownership de uma chamada process,
restarts/notificações drenados por Timer na thread da interface e campos de
latência/suspensão atômicos. A proteção contra lifecycle concorrente foi mantida.
O modo NOVA continua somente áudio; MIDI de saída de plugins é descartado.

`MutationGuard` inibe novas chamadas de render com CAS; a thread de controle
espera o render existente terminar antes de trocar/destruir recursos. Apenas
uma thread de controle pode realizar mutações; controles escalares são atômicos.
Preparação, resets VST3 em seek e buffers PDC ficam fora do callback.

PDC máximo: 65536 amostras por insert, 262144 por caminho, 128 MiB de buffers
de arestas. Excesso invalida o processamento com diagnóstico em vez de tocar
áudio desalinhado. Bypass conserva a latência. Versão de sessão permanece 3;
latência conhecida do placeholder é opcional, compatível com sessões antigas.
