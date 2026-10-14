# Notas de implementação da Fase 4

Preservar grafo/mixer, com tipo MIDI no Track, clips em ticks, eventos compilados
em vetor sob MutationGuard e buffers MIDI preparados. Instrumento é separado
dos quatro inserts FX; incluir sua latência antes dos inserts no PDC. Descoberta
aceita VST3i stereo com MIDI e zero entradas; efeitos stereo permanecem aceitos.
Plugin ausente de instrumento preserva estado e fica silencioso.

Scheduler musical já implementado em MidiData. Integrar eventos por spans do
loop e segmentação sem crescimento de MidiBuffer; panic em seek/stop/overflow.
Reidratar notas ativas ao fazer seek para dentro da nota; Note Off no fim do clip.
MIDI live usa uma mailbox atômica limitada, de múltiplos produtores e um
consumidor, para permitir mais de um dispositivo e teclado/interface; captura de gravação drenada fora do render.
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

A mailbox usa sequência por célula, publicação acquire/release e no máximo
oito tentativas de reserva. Consumidor retorna vazio se o produtor ainda não
publicou; não espera. Não chamar isso de fila irrestritamente lock-free: um
produtor suspenso pode atrasar entrega. É uma implementação original de fila
limitada, com provas práticas de integridade/contenção a acrescentar nos testes.

Entrada ao vivo/gravação: novo MidiIO alocado no controle, duas mailboxes
(ingresso MPSC, capturas áudio→UI), tabela de device tokens atômicos por track
e gen de sessão. Capturar gen antes de ler rotas; ao clear desabilitar todas
as rotas antes de avançar gen. Trocando input, descartar eventos de token
antigo e enviar panic. Sem reset de fila contra produtores ativos.

Live buffers preparados para 64 tracks, máximo 256 eventos/callback por track;
overflow → panic/contador. Render drena entrada, converte offsets em ticks e
publica capturas; UI emparelha notas e acrescenta clips fora do callback.
Processar instrumentos MIDI mesmo parado para teclado/entrada selecionada,
sem avançar timeline/WAV/metrônomo; pause/seek/end fazem panic e reidratação
quando reproduzir novamente. Record impede stop automático no fim das regiões.
Hardware timestamps devem ter política explícita e testes simulados distintos.

Piano Roll usa MidiEditModel próprio com checkpoints/rollback sob guard, histórico
limitado por memória, clips selecionáveis e gestos/notas/velocity/zoom/snap.
Adicionar menu MIDI (+ track, editor, importar/exportar SMF, metrônomo/record).
Não permitir EditModel WAV colar/mover para track MIDI. Exibir clips/notas na
timeline e abrir Piano Roll no duplo clique. Fechar janela antes de trocar sessão.
Fixture VST3i de build pode ser varrida localmente em builds com testes, sem instalar.
