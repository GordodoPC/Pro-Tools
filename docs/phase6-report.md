# NOVA DAW — Fase 6: edição avançada de áudio

Versão 0.6.0, sessão v6. Implementação no projeto existente, após Fases 3.5/4/5.
Builds Debug e Release aprovados; CTest 12/12 em ambos, 503 verificações
numéricas/comportamentais mais dois smoke tests. Critério funcional executado,
com qualidade perceptual de material complexo e hardware externo ainda pendentes.

## Edição e fades

- Split/trim/move/copy/paste/delete preservados e ampliados; Slip, Ripple,
  duplicação, seleção múltipla entre faixas, seleção por intervalo, Snap/Grid,
  Undo/Redo. IDs de regiões novos nas cópias; seleção/referência primária separadas.
- Shift/Ctrl-click seleciona múltiplos clips; Selector marca intervalos da faixa;
  Ctrl+A seleciona áudio. Tool Slip desloca a janela da fonte, mantendo a posição.
- Ripple move/trim afeta clips posteriores da faixa; Delete fecha a união dos
  intervalos removidos, sem apagar outras fontes. Operações são validadas antes
  de alterar a estrutura; limites de tracks/regiões/timeline preservados.
- Fade In/Out lineares, exponenciais (x²) e equal-power (sin(x π/2)). Crossfade
  de dois clips adjacentes usa handles disponíveis; clips sobrepostos usam a
  largura da sobreposição. Timeline desenha a curva aplicada ao sinal.
- Editor de áudio mostra curvas/handles arrastáveis, duração e tipos dos fades,
  Crossfade/Slip/Ripple/duplicação/Undo/Redo e os controles offline.
- Envelope/sample compartilhados entre playback e render offline. Testes usam
  constantes/senoides para confirmar alteração de áudio, não só desenho.

## Stretch, pitch e preview

- Implementação própria de phase vocoder STFT, FFT JUCE já existente no projeto,
  Hann 2048 samples, hop de análise 64, hop de síntese conforme duração/pitch,
  fase desembrulhada e overlap-add normalizado. Pitch combina stretch e resampling
  linear; duração final é arredondada ao sample exato solicitado.
- Fator de duração 0.25–4, ±12 semitons e ±100 cents. Workspace/output limitados
  antes da alocação; jobs rodam no worker, com cancelamento/progresso.
- Fonte original/janela antes da primeira transformação ficam referenciadas.
  Reprocessar usa essa fonte; Reverter restaura essa janela e permite Undo.
  Trims posteriores no resultado são revertidos junto com a transformação.
- Cache em `.nova-cache`, WAV float32 + chave com caminho, hash FNV dos samples
  imutáveis decodificados, janela, rate, parâmetros e versão do algoritmo.
  Reuso exige chave exata e tamanho decodificado esperado. Colisão de nome/chave
  escolhe UUID novo; fontes nunca são destinos de processamento.
- Pre-visualizar gera cache sem alterar a sessão. Preview explícito usa voz/buffer
  já preparado no engine, sem player separado, disco/alocação ou lifecycle no
  callback; transport não avança. Gain de preview 0.25; Stop/fechar/cancelar param.
- Resultado assíncrono só é adotado se o clip/janela ainda corresponderem ao
  snapshot; edição durante o job não é sobrescrita. Assets têm ownership até
  worker/callback terminarem; troca/liberação na thread de controle.

### Limitações do algoritmo

Preservação de pitch/duração é aproximada. Phase vocoder pode suavizar transientes,
introduzir som metálico/phasiness e alterar a imagem estéreo em material complexo.
Não há preservação de formantes, phase locking de picos ou qualidade equivalente
à de algoritmos comerciais. Resampling linear pode gerar alias em pitch alto;
voz/percussão/polifonia/extremos exigem audição manual. Senoides de 440 Hz, oitavas,
cents e canais idênticos foram verificados numericamente; isso não certifica
qualidade perceptual para todo material.

## Comping e render

- Takes continuam preservados e organizados em grupos de captura. Comp trecho
  seleciona take/intervalo e combina a fonte escolhida com as regiões ao redor,
  aplica equal-power nas fronteiras e permite Undo. `activeTake=0` identifica
  arranjo composto; regiões mantêm takeId e fontes, todos persistidos.
- Consolidar gera WAV float32 novo da seleção em uma faixa, incluindo slip,
  offsets e fades. Mantém mono quando todas as fontes são mono; caso misto usa
  stereo com pan law central para fontes mono. Adota região sem duplicar fades;
  fontes no disco e Undo preservados. Renderiza antes de inserts/fader.
- Bounce seleção clona a sessão/mixer na thread de controle e processa um engine
  independente no worker: instrumentos, inserts, buses, sends, fader/pan, Master,
  Read e PDC. Loop, input monitoring e metrônomo desligados para render da seleção.
- Bounce descarta a latência inicial total e grava exatamente o intervalo. Falha
  em plugin ausente/ativo, fault/suspensão/dropout ou alteração de latência aborta
  a publicação, com mensagem. Bypass explícito mantém comportamento de dry/PDC.
- Arquivos em `Rendered/`, nomes UUID. Temporários são fechados antes de publicar;
  MoveFileW publica no Windows sem substituir um destino existente. Cancelamento
  ou erro remove apenas o temporário pertencente ao job, preservando fontes.
- Cache/Rendered são referências da sessão; não são apagados automaticamente.
  Copie-os junto ao projeto. Backups devem incluir capturas/fontes/cache usados.

## Persistência, tempo real e limites

- Sessões versões 1–5 são migradas; v6 adiciona fades/curvas, fonte/janela original,
  parâmetros de transformação e referências dos resultados. Comp usa regiões/takes.
  Metadados inválidos e fontes ausentes são recusados antes de substituir sessão.
- Render usa apenas buffers/estado preparados e matemática limitada. Edições e
  liberação de assets exigem MutationGuard; callback nunca espera o controlador.
- 64 tracks/128 regiões por track, 512 MiB de assets vivos e workspace offline
  até 512 MiB. Undo mantém até 64 estados, reduzidos por budget de até 1 GiB de
  assets compartilhados (inclui clipboard/estado vivo); snapshots de bounce usam
  memória adicional. Bad allocation/IO failure reportados, sem editar originais.
- Consolidar clips misturando mono e estéreo congela o pan law central de fontes
  mono; com pan não central o balanço pode mudar. Use Bounce para resultado exato
  do mixer. Consolidação não inclui inserts/fader; Bounce inclui.
- Bounce aplica o limite/clipping de saída do engine; não inclui caudas além do
  intervalo escolhido. Seleções iniciadas no meio de áudio resetam estado/tails;
  use intervalo anterior se precisar aquecer efeitos. Capture latencies não
  calibradas na Fase 5 continuam pendentes.
- Inicialização/state dos plugins do snapshot e adoção/decode ocorrem na thread
  de controle; podem ocupar a UI. Fechar/cancelar aguarda o worker liberar recursos.
  IO ou plugin nativo travado pode bloquear essa espera. Não há bridge/crash
  isolation de playback/bounce; terceiros continuam não certificados.
- Scanner isolado/custom dirs, efeitos/PDC/mixer/MIDI/gravação anteriores preservados.
  Nenhuma dependência nova, instalação de plugin ou alteração global.

## Validação automática e manual

AdvancedAudioEditing verifica multiseleção/move/copy/paste/delete/undo, trim/slip,
ripple, crossfade unit-power, fades em samples reais, v6/invalid metadata, pitch/
stretch/cache/revert, consolidação, full bounce, comping/roundtrip, cancelamento,
resultado obsoleto, sources byte-for-byte, preview e edição concorrente com render.
Inclui VST3 próprio com latências track 31 + bus 17 + Master 7, pre-send/Read:
impulso somado fica no sample 0 após remover 55 samples de PDC. Inclui VST3i real
com notas que geram WAV não nulo. Probe observa C++ new; sem prova sobre malloc
interno arbitrário. ThreadSanitizer/hardware multicanal não foram executados.

Roteiro manual pendente: editar/comp com takes físicos longos; audição de voice,
percussão, stereo/polifonia e extremos do stretch/pitch; zoom/drag em muitos clips;
horas de trabalho, IO lento/disco cheio real, plugins de terceiros com editor/
state/bounce, mudança de hardware. Compatibilidade externa não foi aprovada.

Executável: build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe.

## Resultados executados — 2026-10-14

- CTest Debug: 12/12, 29,97 s; Release: 12/12, 16,82 s.
- 55 verificações AdvancedAudioEditing por build; mais 448 das fases anteriores.
- WAVs de sinal conhecido: fades, stretch 1.5/0.5, ±12 semitons, +100 cents,
  canais stereo idênticos, consolidação/bounce e comp. Originals byte-for-byte.
- Preview/fades: zero C++ new observado no render; edição/Undo concorrente com
  callback finito. Probe e testes não substituem ThreadSanitizer ou hardware.
- Full bounce real: VST3 próprio, track/bus/Master 55 samples, pre-send e Read;
  impulso alinhado no sample zero. VST3i próprio produz áudio não nulo em WAV.
- UI Debug/Release final: passed=true, nativeEditorBindings=true,
  asyncTransformPreviewConsolidateBounce=true, sessionRoundtrip=true.
- Evidência UI: build/ui-validation-phase6-complete-{debug,release}/ui-result.json,
  audio-editor.png, WAVs/cache/Rendered e sessão v6.
- Regressão da interface Edit/Mix/gestos/atalhos/loop/DSP/sends/Read/Master:
  Debug e Release passaram com demo.novadaw do CTest e saída Realtek 48k/480 frames.
  Invocação inicial sem --session não tinha fontes e falhou; não foi contada como
  evidência aprovada. O README agora explicita a fixture necessária.
- Stress curto Release final do CTest: 10,020 s, 1180 callbacks, buffers
  64/128/257/480/1024, 32 faixas/3 buses/10 VST3 próprios; CPU 12,63% de um core,
  pico privado 88.354.816 bytes, max render 6,944 ms, **1 deadline overrun**, zero
  C++ new observado. A execução anterior teve zero overruns; ambas são ensaios
  curtos e não substituem os 300 s da Fase 3.5, que tiveram 7 violações de prazo.
- Memória final do ensaio curto: 83.021.824 bytes; variação não prova ausência de
  leaks em terceiros. Native plugins/hardware externos não foram certificados.

Snapshot/restore também preserva smoothingTime do engine para comparar render
com playback. Recuperação da Fase 5 agora fecha worker interrompido antes de ler
WAV/manifesto, e APIs de captura limpam mensagens antigas de erro ao iniciar.

Evidência de regressão Edit/Mix: build/ui-regression-phase6-final-{debug,release}/
ui-result.json, todos os flags true. Verificações incluem abertura da sessão v6,
atalhos, controle de mesa, curva EQ, send/native editor/Read/Master e callback Realtek.

Revisão final: snapshot de bounce captura bad_alloc/exceções C++, devolve erro e
mantém a sessão aberta. Falha de alocação injetada foi executada em Debug/Release;
55 checks de edição/offline por configuração. Hardware e falhas nativas arbitrárias
continuam fora dessa garantia. UI MIDI e gravação Release também passaram em
build/ui-{midi,recording}-regression-phase6-release/ui-result.json; entradas
simuladas, MIDI físico e plugins de terceiros explicitamente não testados.

Build Release validado (SHA256):
`6C204A8F238ABD6BD72DCD6E08943AA6FA8442BD171A7683A173541A22C7119B`.
Path Windows: `C:\Users\eduar\OneDrive\Documentos\Projeto SOM\ProTools\build\ninja-release\NovaDAW_artefacts\Release\NOVA DAW.exe`.
Regressões MIDI/recording da UI passaram; hardwareInput=false/physicalMidi=false.

Commits locais de entrega: Fase 4 `86b44d5`, Fase 5 `811ffeb`, Fase 6 `0f7f605`.
Escopo oficial: roadmap-phases-4-6.md; diretrizes AGENTS.md. Sem push remoto.

## Retomada — 2026-10-15: falhas e encerramento de bounce

Corrigidos três casos reproduzidos por regressões:

- Cancelamento/erro deixava o transporte offline em execução. Uma guarda de
  escopo agora para/reseta o engine independente em toda saída após Play,
  fora do callback; temporários continuam sendo removidos pelo job.
- Latência reportada durante o último bloco não era conferida novamente.
  O worker verifica manutenção também depois de cada render, antes de escrever.
- Exceção de processamento ativava bypass automático e podia fazer o teste de
  saúde aceitar um WAV com sinal seco como render bem-sucedido. Fault agora tem
  precedência sobre bypass; o WAV parcial não é publicado. Bypass explícito de
  insert ausente mantém a passagem seca compensada e tem regressão própria.

Transições síncronas de latência/exceção são injetadas por fixture JUCE interna
no teste; não se trata de comprovação de falha nativa ou notificação assíncrona
arbitrária de VST3 externo. Os testes existentes continuam processando módulos
VST3/VST3i próprios reais e verificando o áudio e a PDC de 55 samples.
Notificações que só chegam após terminar o render continuam fora dessa garantia.

AdvancedAudioEditing: **60 verificações**. CTest final Debug **12/12 (36,78 s)**
e Release **12/12 (20,43 s)**; total **513 verificações + dois smoke tests** por
configuração. Originais continuam conferidos byte-for-byte. Nenhuma mudança de
algoritmo stretch/pitch, formato de sessão ou callback de áudio neste ciclo.
Qualidade perceptual, sessões longas, IO adverso real e terceiros seguem pendentes.

UI avançada Debug/Release passou: bindings, transform/preview/consolidate/bounce
assíncronos e sessão aprovados, em
`build/resume-20261015-ui-advanced-{debug,release}/ui-result.json` e
`audio-editor.png`. Regressão Edit/Mix também passou nas duas configurações,
em `build/resume-20261015-ui-editmix-{debug,release}/ui-result.json`.
Correções no commit local `a8df20d`. Novo SHA256 Release:
`0976427221EC4B7572578C00D96789C77B4F64EE91DBD5A36BF35827F76DC21F`.
