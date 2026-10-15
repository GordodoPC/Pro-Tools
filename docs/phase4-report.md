# NOVA DAW — Fase 4: MIDI e instrumentos

Data: 2026-10-14. Escopo oficial: roadmap-phases-4-6.md.
Critério funcional de aceitação executado com instrumento VST3 original: criar
track, editar no Piano Roll, tocar áudio real, mudar tempo e salvar/reabrir.
Validações de dispositivo MIDI físico e instrumentos de terceiros permanecem pendentes.

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
sem regressão da Fase 3.5. Release: build aprovado e CTest 8/8; 14 verificações MIDI aprovadas em ambos neste ciclo histórico.
Teste numérico confirma offsets 25, 275, 300 e 325 samples a 120 BPM/48 kHz,
inclusive Note Off cruzando o limite de buffer. Mil agendamentos não alocaram
armazenamento C++ observado; overflow e tamanho de evento inválido são testados.

## Ciclos de integração

Os ciclos posteriores integraram áudio/PDC, instrumentos, Piano Roll,
gravação MIDI, metrônomo e sessões musicais, conforme evidências abaixo.

## Validação externa pendente

Dispositivo MIDI físico, instrumentos de terceiros e interfaces nativas ainda
não foram testados. Nenhum plugin/dependência externa foi instalado. Fixtures
originais serão usadas para produzir e verificar áudio real de instrumento.

## Integração implementada

- Track MIDI com instrumento separado dos quatro inserts FX; áudio passa por
  pan/ganho/mute/solo/sends/buses/Master/PDC existentes.
- Instrumentos VST3i stereo/zero-input identificados no scanner filho e
  carregados pelo host; fixture original NOVA Test Synth produz oscilador real.
- Loop, reidratação de notas em seek, panic em stop/overflow, contexto VST3
  de BPM/PPQ/compasso/transporte e metrônomo real no Master alinhado pela PDC.
- MIDI ao vivo mesmo parado, múltiplas fontes em mailbox limitada e roteamento
  por dispositivo selecionado. Timestamps Windows monotônicos são convertidos
  para offsets com uma janela de um buffer; eventos futuros são preservados
  em fila fixa quando muda o tamanho do buffer. Timestamps tardios chegam no
  início do próximo bloco; precisão/roundtrip físico ainda não foram medidos.
- Captura MIDI drena eventos fora do render, emparelha notas, preserva CC/PG/PB,
  cria takes/clips e arquivos SMF de recuperação sem sobrescrever fontes.
  Mudança de tempo/seek/interrupção fecha a captura; não grava mapas de tempo.
- Sessões v4 guardam MIDI, input/arm, metrônomo e estado de instrumentos, com
  validação antes da substituição; leitura v1/v2/v3 mantida.
- Piano Roll com teclado, grade/Bars-Beats, notas/velocity/canais, seleção múltipla,
  desenho/move/resize/delete, copiar/colar, quantizar, Undo/Redo, zoom H/V e snap.
  Clips MIDI têm início/duração/offset, duplicação, SMF e loop próprios.
- Interface/presets internos e arquivos .novapreset do instrumento; ausentes
  preservam estado e ficam silenciosos. MIDI aparece na timeline e lista Clips.

A validação inicial da janela passou em Debug e Release: gestos reais
simulados no canvas, botões/atalhos, edição, histórico, notas produzindo callback
Realtek não nulo, alteração para 90 BPM e roundtrip de sessão/instrumento.
Snapshots piano-roll.png inspecionados. Dispositivo: Realtek 48000 Hz/480 samples.
MIDI físico e instrumentos de terceiros continuam explicitamente não testados.
Presets/CC/timestamps, loop e controles finais revalidados em Debug e Release.

## Validação final — 0.4.0

- Builds x64 Debug e Release aprovados; CTest **10/10** em ambos.
- 97 áudio/sessões + 36 mixer + 44 DSP/VST/Read + 116 PDC + 40 estabilidade
  + **22 MIDI/dados/editor** + **42 instrumento/motor/captura** + **13 filas**.
  Total **410 verificações**, além de dois smoke tests, por configuração.
- Debug: 20,70 s; Release: 15,76 s. Nenhum teste automatizado reprovado.
- Concorrência: quatro produtores, 20000 pacotes únicos, nenhuma corrupção
  nem alocação C++ observada nas chamadas de fila exercitadas.
- Preset Triangle altera waveform real e recupera estado; CC7 modifica áudio.
- Timestamps simulados mantêm Note On/Off nos offsets 17/150 mesmo ao passar
  de 256 para blocos de 64, com evento futuro preservado. Isso não é teste físico.
- Recuperação MIDI em arquivo separado, gravação simulada com velocity/canal,
  sessão v4 e dados inválidos sem substituir o projeto aprovado.
- `--validate-midi-ui`: snapshots e resultados em
  `build/ui-validation-phase4-{debug,release}/ui-result.json`,
  `piano-roll.png` e `phase4-ui.novadaw`.

Módulos de teste e scanner continuam locais, sem instalação ou push.
Executável Release: `build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe`.
Debug: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe`.

## Limites e validação manual pendente

- Timestamps usam relógio JUCE Windows e uma janela adicional de buffer; atraso
  físico depende de dispositivo/driver/buffer. PPQ 960 arredonda a captura ao
  tick mais próximo. Testes com entrada real, jitter e roundtrip ainda pendentes.
- Gravação MIDI é linear; loop MIDI é de reprodução. Loop recording de áudio
  pertence à Fase 5. Não há mapa de mudanças de tempo, MPE ou SysEx.
- Instrum. suportados: um output stereo e zero input de áudio ativo, com MIDI.
  Sidechains/múltiplos outputs e binários de terceiros ainda não são certificados.
- Programas dependem do que o plugin oferece; .novapreset guarda estado opaco.
- Histórico MIDI: até 64 estados, com limite estimado de 64 MiB de dados; memória
  de containers/strings e plugins é adicional. Render tem limites de eventos e
  emite panic em excesso. Nenhuma garantia hard real-time de plugin externo.
- Exercitar manualmente dispositivos MIDI reais, hotplug, plugins autorizados,
  seus editores/licenças, diálogos de import/export/preset e gravação longa.

A evidência de hardware executada é saída Realtek com instrumento próprio;
entrada MIDI física não foi testada, e nenhum módulo de terceiros foi instalado.

Continuidade: gravação de áudio/sessão v5 implementada e validada na Fase 5;
ver [phase5-report.md](phase5-report.md) para limites e evidências.

Regressão completa em 0.6.0: CTest Debug/Release 12/12; sessão v6 preserva MIDI,
instrumento e presets. MIDI físico/terceiros continuam não validados.

## Retomada — 2026-10-15

Base local `ccf7ec3` conferida igual à main remota antes das alterações.
MIDI permanece funcionalmente implementado; nenhum novo recurso MIDI é
declarado neste ciclo. CTest final Debug **12/12 (36,78 s)** e Release
**12/12 (20,43 s)**, mantendo os testes de dados/SMF, concorrência, instrumento
VST3i real, captura e persistência. Total do projeto: 513 verificações e dois
smoke tests por configuração, após regressões de recuperação/bounce.
Entrada MIDI física, jitter/hotplug e instrumentos externos seguem pendentes.
Auditoria e preparação das próximas etapas: [development-status.md](development-status.md).
