# NOVA DAW — Fase 3

Data: 2026-10-14. Referência: `e8312597a50496898a5075ab5952187ba3815e91`.

## Auditoria antes das alterações

Workspace limpo; build Ninja x64 Debug aprovado; CTest 3/3 aprovado
(97 verificações em AudioAndSessions). Inspecionados AudioEngine, Session,
EditModel, Preferences, MainComponent, EditorComponents, App, CMake e testes.

O motor existente soma regiões de 64 faixas, reamostra linearmente WAVs em
memória e oferece volume/pan/mute/solo/master e saídas stereo/L/R. Timeline,
gestos, histórico e loop já funcionam. MIX reutiliza os controles de Edit,
mas inserts/sends são apenas rótulos pendentes. Não existem buses, VST3, DSP
de efeitos ou automação. Formatos de sessão 1/2 são validados antes de substituir
o estado e escritos por arquivo temporário. WAVs permanecem externos.

O callback não lê disco nem usa mutexes; estruturas são alteradas com callback
desanexado. Essa convenção será preservada para impedir ponteiros invalidados.
Controles são atômicos, mas ganho/pan não têm smoothing. A pausa estrutural
é segura, embora possa causar uma interrupção audível. Importação é síncrona.

## Implementação e validação

Implementação inicial validada no commit local `517175f`. Sem push remoto.
Segunda rodada: rampas de mute/solo/sends, RMS do buffer inteiro, publicação de
medidores no fim do callback, invalidação rápida do delay ao fazer seek,
gerenciador VST3, proteção de tamanho de metadados e validação da interface.

## Funcionalidades implementadas

- Mixagem real simultânea de WAVs/regiões existentes, sem substituir EditModel/timeline.
- Buffers stereo pré-alocados, processamento em segmentos de até 256 amostras,
  ganho de entrada/fader/Master, pan, mute, solo, peak/RMS e clipping retido.
- MIX com nomes/cores editáveis, entrada/saída, ganho em dB, quatro inserts,
  dois sends, pan, fader vertical, medidores e Master com mute e inserts.
- Aux stereo/mono, returns, Track → Bus → Bus → Master e sends pré/pós-fader.
  Grafo topológico validado fora do callback; destinos inválidos/ciclos rejeitados.
  Solo de fonte mantém returns downstream; solo de bus admite suas fontes.
- VST3 real pelo JUCE: descoberta, lista persistente, scanner filho por módulo,
  timeout de 20 s, blacklist, gerenciador de descrições, inserts, bypass,
  editor nativo/genérico e estado opaco em base64. Plugins ausentes conservam
  descrição/estado e funcionam como passagem direta.
- NOVA EQ: cortes, shelves e três bandas frequency/gain/Q, bypass e resposta visual.
  Coeficientes JUCE DSP calculados sem alocação no callback; filtros biquad stereo.
- Compressor stereo ligado: threshold/ratio/attack/release/knee/makeup/mix,
  medição de redução e bypass. Gate: threshold/attack/hold/release/bypass.
  Delay: 1..2000 ms/feedback/mix/bypass. Reverb JUCE: room/damping/mix/bypass.
- Smoothing de controles e parâmetros nativos; crossfade de bypass.
- Automação Off/Read, pontos editáveis por texto, interpolação, seek/loop e
  persistência. Volume/pan por amostra; parâmetros de inserts a cada 16 amostras.
- Sessões versão 3, migração 1/2, validação antes da troca e escrita temporária.
  Estado de plugins capturado na thread da interface com callback desanexado.

## Funcionalidades parciais e limites na entrega original

**Esta entrega não certifica a Fase 3 completa.** A garantia estrita de tempo real
da cadeia VST3 e a compatibilidade com plugins externos ainda não foram verificadas.

- O callback próprio não faz I/O, operações de interface, preparação/destruição
  de plugins ou aquisição de mutex. As estruturas mudam com callback desanexado;
  isso pode interromper brevemente a reprodução e reiniciar caudas.
- JUCE 9.0.4 usa `SpinLock` interno em `VST3PluginInstanceHeadless::processBlock`,
  em `external/JUCE/modules/juce_audio_processors_headless/format_types/juce_VST3PluginFormatImpl.h`.
  Não é correto declarar a cadeia VST3 livre de locks. Plugins externos podem
  também alocar, bloquear ou falhar. O wrapper oficial foi preservado.
- Somente efeitos com um bus stereo de entrada/saída; instrumentos, MIDI,
  sidechains e múltiplos buses de plugin não são aceitos nesta implementação.
- Scan isolado; carregamento e processamento dos inserts permanecem no processo
  principal. Exceções C++ desativam o insert e indicam falha; access violations
  de plugins não são isoladas durante reprodução.
- Sem compensação de latência de plugins ou playhead VST3 para tempo-sync.
- Automação textual, até 64 lanes/4096 pontos por lane; sem desenho de envelopes,
  edição via timeline ou automação de Master/sends/bypass. Read usa smoothing;
  o sinal pode acompanhar mudanças de parâmetros com atraso da rampa.
- Sem streaming, limiter, exportação, gravação/Record Arm, ASIO, fades de clips
  ou Write/Touch/Latch operacionais. O enum reserva os modos avançados, sem UI ativa.
- Reprodução encerra ao fim das regiões; caudas após esse ponto estão pendentes.
  Seek limpa caudas nativas; reset VST3 não é chamado dentro do callback.
- 64 canais incluindo Aux; quatro inserts/dois sends por canal; 512 MiB de mídia,
  128 regiões por faixa, timeline 24 h, JSON de sessão limitado a 64 MiB.
  A memória dos plugins/delays é adicional à mídia e não tem orçamento global.
- Sem benchmark/certificação de carga máxima, latência roundtrip ou teste de horas.
  Importação WAV e carregamento de plugins continuam síncronos na interface.

## Testes automatizados da Fase 3

Resultados finais: build Ninja x64 **Debug e Release aprovados**, CTest **5/5**
em cada configuração. AudioAndSessions: **97** verificações; MixerRouting:
**36**; DSPVST3AutomationSessions: **44**. Total: **177** verificações por build,
além dos smoke tests C++20/Windows SDK e JUCE. Nenhum teste automatizado permanece
reprovado nesta revisão.

As duas janelas passaram em `build/ui-validation-phase3-debug/ui-result.json`
e `build/ui-validation-phase3-release/ui-result.json`: todos os campos verdadeiros.
Dispositivo: `Alto-falantes (Realtek(R) Audio) | 48000 Hz | 480 samples`.
Snapshots Edit/Mix foram inspecionados; faders/valores de volume ficam visíveis
em 1280×800, com rolagem para janelas menores e mais canais.

| Critério solicitado | Evidência |
|---|---|
| Mixagem múltipla, volume/pan/mute/solo | AudioAndSessions + MixerRouting |
| Clipping e Master | AudioAndSessions + MixerRouting |
| Roteamento, ciclos, solo de bus | MixerRouting |
| Sends pré/pós-fader e smoothing | MixerRouting |
| Carregar/processar VST3 | DSPVST3AutomationSessions: VST3 próprio compilado |
| Bypass, estado e editor VST3 | DSPVST3AutomationSessions |
| DSP nativo | Seno de 1 kHz, impulso, sinal constante e equação de compressor |
| Salvar/abrir e plugins ausentes | Sessão v3 completa e roundtrip de placeholder |
| Compatibilidade 1/2 | AudioAndSessions + fixture v2 |
| Buffers diferentes | 1, 17, 64, 257, 480, 1024, 4096; segmentos DSP |
| Automação de volume/pan/parâmetros | Interpolação numérica, seek e DSP/VST3 Read |
| Regressão Fases 1/2 | 97 verificações AudioAndSessions |
| Alocações do caminho nativo | Interceptação C++ new; Debug também usa hook CRT |
| Scanner isolado e recuperação | Fixture induz saída 86 no construtor do processo filho; host permanece vivo, blacklist/retry aprovados |
| Janela/hardware | `--validate-ui`: edição, layout, bindings, DSP/send/automação/Master e Realtek |

O VST3 NOVA Test Gain é uma fixture original compilada na pasta de build, com
ganho conhecido. O teste inicia scanner filho, carrega o módulo real pelo JUCE,
verifica áudio matematicamente, abre/fecha editor, persiste/restaura estado e
valida plugin ausente. Nenhum plugin é instalado globalmente. Os testes de
alocação são evidência do caminho exercitado, não prova para plugins de terceiros.

### Testes reprovados e correções

- Primeiro carregamento da fixture VST3: estado inicial vazio tratado como base64
  inválido. Corrigido; carregamento/processamento/roundtrip aprovados.
- Primeiro teste de bindings MIX: controles conservavam valores do teste Edit,
  enquanto o motor já estava restaurado. Corrigido sincronizando antes do teste.
- Uma execução da janela sob compilação concorrente capturou pico zero intermediário.
  Corrigida publicação dos medidores somente ao final do callback; revalidação
  final Debug/Release aprovada.
- Teste de falha do scanner revelou metadados de bundles sem contagem de canais.
  Scanner passou a construir/preparar cada candidato no processo filho e consultar
  os canais reais. Descoberta de bundle, blacklist e recuperação aprovadas.
- Copiar scanner somente em POST_BUILD deixava cópias antigas após recompilar
  apenas o helper. Targets de runtime garantem cópias atualizadas em cada build.

### Testes manuais pendentes

1. Instalar/usar seus próprios VST3, varrer suas pastas e testar interfaces nativas,
   autorização normal dos plugins e reabertura de projetos em outra máquina.
2. Audição física de alterações, bypass, loop, transições e roteamento com seu hardware.
3. Carga máxima/execução longa, xruns, troca de dispositivo e latência roundtrip.
4. Exercitar diálogos nativos de arquivos/dispositivo e menus de inserts por mouse.
5. Injetar access violation/timeout de módulos externos. Falha de processo e retry
   já têm teste automatizado com a fixture própria; timeout não foi exercitado.

## Não implementado na entrega original

Write/Touch/Latch, automação gráfica/Master/sends, compensação de latência,
playhead VST3 para tempo-sync, instrumentos/MIDI/sidechains, gravação/Record Arm,
ASIO, streaming, exportação, caudas após o fim das regiões e isolamento dos
plugins durante reprodução. Esses recursos não são apresentados como concluídos.

## Executáveis e evidências locais

- Debug: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe`.
- Release: `build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe` (aprovado).
- Scanner acompanha cada executável como `NovaPluginScanner.exe`.
- CTest: `build/{ninja-debug,ninja-release}/Testing/Temporary/LastTest.log`.
- Validação final: `build/ui-validation-phase3-{debug,release}/ui-result.json`,
  `edit.png`, `mix.png`, `bindings.txt` e `phase3-ui.novadaw`.
- Builds locais sem instalador; o MSVC/Debug anterior pertence à Fase 2 e não foi
  recompilado nesta fase. Use os executáveis Ninja listados acima.

## Dependências e REA

JUCE fixado no projeto, licença AGPLv3/comercial em `external/JUCE/LICENSE.md`.
SDK VST3 distribuído com JUCE sob MIT, em
`external/JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt`.
A modalidade JUCE de distribuição ainda deve ser definida, como na Fase 2.
API oficial: [AudioPluginFormatManager](https://docs.juce.com/master/classjuce_1_1AudioPluginFormatManager.html)
e [KnownPluginList](https://docs.juce.com/master/classjuce_1_1KnownPluginList.html).

REA não foi utilizado: a investigação necessária era do código-fonte público
do projeto/JUCE, sem necessidade de engenharia reversa de artefatos proprietários.

## Atualização: estabilização Fase 3.5

A Fase 3.5 substitui as limitações históricas acima quanto a PDC e às esperas
identificadas no wrapper. Implementados alinhamento de faixas/buses/Master e
sends pré/pós, sinal seco compensado em bypass/suspensão/plugin ausente,
automação considerando a origem do áudio atrasado e drenagem da latência
no fim das regiões. Sessões continuam na versão 3, com latência opcional.

O host usa tentativa única de aquisição com silêncio/contador em contenção,
filas atômicas para notificações e preparação/reconstrução fora do render.
Mutações estruturais esperam o render terminar na thread de controle antes
de trocar/destruir objetos. Diretórios VST3 adicionais são persistentes e
somados aos diretórios padrão; nenhum plugin externo foi instalado.

Os resultados atuais, testes prolongados, limites de tempo real e roteiro
manual estão em [phase3-stability-report.md](phase3-stability-report.md).
A compatibilidade com terceiros permanece pendente e, por instrução do
usuário, não bloqueia as Fases 4, 5 e 6. Os demais recursos futuros listados
na entrega original não foram implementados nesta estabilização.

## Continuidade das fases seguintes

O histórico da Fase 3 permanece acima. As Fases 4, 5 e 6 foram implementadas no
mesmo workspace, com relatórios phase4/5/6-report.md, sessão v6 compatível com 1–5,
builds Debug/Release e regressão CTest 12/12 (503 verificações + dois smoke tests).
MIDI/VST3i, captura WAV e edição/offline preservam mixer/PDC/Read. Pendências de
hardware/terceiros e limitações de tempo real permanecem explícitas nos relatórios.
