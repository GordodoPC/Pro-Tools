# Notas de implementação da Fase 4

Preservar grafo/mixer, com tipo MIDI no Track, clips em ticks, eventos compilados
em vetor sob MutationGuard e buffers MIDI preparados. Instrumento é separado
dos quatro inserts FX; incluir sua latência antes dos inserts no PDC. Descoberta
aceita VST3i stereo com MIDI e zero entradas; efeitos stereo permanecem aceitos.
Plugin ausente de instrumento preserva estado e fica silencioso.

Scheduler musical já implementado em MidiData. Integrar eventos por spans do
loop e segmentação sem crescimento de MidiBuffer; panic em seek/stop/overflow.
Reidratar notas ativas ao fazer seek para dentro da nota; Note Off no fim do clip.
MIDI live usa filas SPSC separadas para dispositivo e teclado/interface (não
misturar produtores), limitadas; captura de gravação drenada fora do render.
Registro deve preservar offsets de entrada e converter para ticks; overflow
precisa diagnóstico e panic. Monitor MIDI vivo requer processar instrumentos
quando transporte parado, sem avançar timeline/WAV/metrônomo.

BPM passa a atômico quando lido pelo render; compasso alterado sob guard.
Metronome é gerador próprio baseado em beats, som real no Master; habilitação
explícita. AudioPlayHead transmite tempo/PPQ/rate/playing ao VST3 sem alocação,
com ponteiro reassociado após substituir sessões.

Sessão futura v4 guarda tipo, MIDI clips, instrumento/estado, entrada e arm;
ler v1/v2/v3 e validar antes da troca. Manter old regressions e testar migração.
Piano Roll próprio: grade/teclado, notas/velocity, seleção múltipla, gestos,
Undo/Redo, quantização, zoom e snap; UI de instrumentos/entrada/clips/SMF.
MIDI EditModel separado para histórico de clips/notas sob guard.

Testar VST3 sintetizador original de build gerando áudio de verdade; arquivo
JUCE AudioPluginClient com macro de restart limitada como fixture de ganho.
Não inferir segurança de instrumentos fechados ou instalação de terceiros.
