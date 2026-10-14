# NOVA DAW — Fase 4: MIDI e instrumentos

Data: 2026-10-14. Escopo oficial: roadmap-phases-4-6.md.
Fase em implementação; ainda não satisfaz o critério completo de aceitação.

## Ciclo 1: dados, relógio e SMF

- Modelo de clips/notas musicais: PPQ 960, Note On/Off, velocity e canais 1..16.
- Mensagens CC, Program Change, Pitch Bend e pressão; ticks ↔ segundos/samples.
- Compilação fora do callback, ordenação de Note Off antes de retrigger no mesmo tick.
- Agendamento numérico em offsets do bloco, com teto de 256 eventos/bloco;
  buffers antecipados de 8192 bytes. Overflow emite all-notes-off nos 16 canais.
- Trim MIDI preserva notas fonte e limita eventos à região visível.
- Persistência de dados do modelo e importação/exportação SMF via JUCE existente;
  escrita temporária, PPQ/velocity/canais preservados. SMPTE e mapas de tempo
  variáveis não são interpretados; tempo global da sessão governa a reprodução.
- Limites: 128 clips/track, 4096 notas e 4096 controles/clip, 65536 eventos/track,
  16 MiB de arquivo SMF. Metadados são validados antes de substituir um clip.

Debug: build aprovado, CTest 8/8 incluindo MidiTimingAndFiles (14 verificações),
sem regressão da Fase 3.5. Release: build aprovado e CTest 8/8; 14 verificações MIDI aprovadas em ambos.
Teste numérico confirma offsets 25, 275, 300 e 325 samples a 120 BPM/48 kHz,
inclusive Note Off cruzando o limite de buffer. Mil agendamentos não alocaram
armazenamento C++ observado; overflow e tamanho de evento inválido são testados.

## Próximos ciclos em execução

Integração de faixas MIDI no áudio/PDC, instrumentos VST3 com notas gerando
áudio real, Piano Roll interativo, gravação de entrada MIDI, metrônomo e
sessões com dados musicais. A interface do ciclo 1 ainda é a Fase 3.5; o
módulo de dados não é apresentado como motor MIDI completo.

## Validação externa pendente

Dispositivo MIDI físico, instrumentos de terceiros e interfaces nativas ainda
não foram testados. Nenhum plugin/dependência externa foi instalado. Fixtures
originais serão usadas para produzir e verificar áudio real de instrumento.
