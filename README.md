# NOVA DAW — Fase 6 (0.6.0)

Aplicativo desktop próprio para Windows em C++20/JUCE. Esta versão implementa
Edit/Mix, importação WAV mono/estéreo, waveform real, reprodução simultânea,
Play/Pause/Stop, seek, loop, volume, pan, solo, mute e sessões `.novadaw`.
A versão 0.2.0 adiciona edição não destrutiva de regiões, Undo/Redo,
grade musical, painéis redimensionáveis, preferências e saída Master stereo/L/R.
A versão 0.3.0 adiciona buses stereo/mono, sends pré/pós-fader, quatro inserts
por canal/Master, VST3 stereo, cinco efeitos nativos e automação Read.
A Fase 4 adiciona faixas/clips MIDI, Piano Roll, instrumentos VST3, entrada e
gravação MIDI, metrônomo e arquivos Standard MIDI File. A Fase 5 adiciona gravação
WAV multipista, entradas mono/estéreo, punch/roll, loop/takes e recuperação.
A Fase 6 adiciona fades/crossfades, seleção múltipla, slip/ripple, stretch/pitch
offline, preview, comping, consolidação e bounce.
ASIO e Write/Touch/Latch continuam pendentes.
A Fase 3.5 adiciona PDC e proteção do ciclo de vida/processamento VST3.
Edição avançada: [docs/phase6-report.md](docs/phase6-report.md).
Gravação: [docs/phase5-report.md](docs/phase5-report.md).
Resultados MIDI: [docs/phase4-report.md](docs/phase4-report.md).
Estabilização: [docs/phase3-stability-report.md](docs/phase3-stability-report.md);
histórico: [docs/phase3-report.md](docs/phase3-report.md).
Retomada em 2026-10-15, comparação com GitHub e situação das fases:
[docs/development-status.md](docs/development-status.md).

## Compilar no Windows

Requisitos: Visual Studio Build Tools 2026 com Desktop Development with C++,
MSVC x64/x86, Windows SDK 10.0.26100, CMake 3.22+ e Git for Windows.
As versões verificadas estão em `docs/environment-report.md`.

O JUCE é fixado em 9.0.4 pelo arquivo `dependencies.json`. Para repor uma cópia
ausente, execute `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/bootstrap.ps1`
(faz download do repositório oficial; a política é limitada ao processo).
O script não substitui uma dependência existente com commit diferente.
CMake aplica as adaptações de tempo real em `patches/juce-9.0.4/realtime.json`,
verifica hashes de todos os arquivos e recusa fontes desconhecidas.

No PowerShell, dentro desta pasta:

```powershell
.\scripts\dev-shell.cmd cmake --preset windows-ninja-debug
.\scripts\dev-shell.cmd cmake --build --preset windows-ninja-debug
.\scripts\dev-shell.cmd ctest --preset windows-ninja-debug

.\scripts\dev-shell.cmd cmake --preset windows-ninja-release
.\scripts\dev-shell.cmd cmake --build --preset windows-ninja-release
.\scripts\dev-shell.cmd ctest --preset windows-ninja-release

.\scripts\dev-shell.cmd cmake --preset windows-msvc
.\scripts\dev-shell.cmd cmake --build --preset windows-msvc-debug
.\scripts\dev-shell.cmd ctest --preset windows-msvc-debug
```

Executáveis: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe`,
`build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe` e
`build/msvc/NovaDAW_artefacts/Debug/NOVA DAW.exe` (build histórico Fase 2).
São builds locais; ainda não existe instalador de distribuição.

## Uso

1. Abra o executável e use **Importar WAV** ou arraste WAV para a janela.
2. Cada arquivo cria uma faixa e um clip no início da timeline.
3. Use Play/Pause/Stop. Clique na régua para posicionar o transporte; `|<` volta ao início.
4. Alterne Edit/Mix. Volume, pan, M e S modificam o áudio em tempo real.
5. Use o menu Sessão para salvar/abrir `.novadaw`.
6. O botão Audio permite selecionar saída, sample rate e buffer.

Atalhos fixos iniciais: Espaço Play/Pause, Home Stop, Ctrl+I Importar,
Ctrl+O Abrir, Ctrl+S Salvar e Ctrl+Shift+S Salvar como.

Em Edit, escolha Smart, Grabber, Trim, Selector ou Split. Arraste clips para
mover, inclusive entre faixas; apare pelas bordas com Smart/Trim; Split divide
no ponto clicado. Trim também recupera áudio disponível na fonte. Selector marca
um intervalo. Snap/Grid ajustam a edição à grade; BPM/compasso alteram a grade,
sem mudar a velocidade do áudio. Loop repete o intervalo, o clip selecionado ou
toda a sessão quando não há seleção.

Editar e o menu de contexto oferecem Copiar/Recortar/Colar (Ctrl+C/X/V), Excluir
(Del), Dividir (Ctrl+E), Desfazer (Ctrl+Z) e Refazer (Ctrl+Shift+Z ou Ctrl+Y).
A colagem usa a faixa selecionada e a posição do transporte. Undo/Redo mantém
até 64 operações de regiões, nome, cor e altura; importar/abrir/nova sessão
reinicia o histórico. Os WAV originais permanecem intactos.

Dê duplo clique no nome para renomear uma faixa; Color altera sua cor. O menu
de contexto da faixa e a borda inferior da linha alteram sua altura. Divisores
ajustam os painéis Tracks/Clips; sliders `px/s` e `x` ajustam o zoom horizontal
e vertical. A lista Clips seleciona e localiza regiões. Volume, pan, M, S e
saída Master stereo/L/R funcionam em Edit/Mix; os masters ficam sincronizados.
Em MIX, **+ Aux stereo** e **+ Bus mono** criam returns/buses. A saída escolhe
Master stereo/L/R ou um Aux. Ciclos são rejeitados. Cada slot de send abre
seleção de destino, pré/pós-fader e nível em dB. A entrada das faixas vem da
timeline e a dos buses vem das conexões. Record Arm das faixas de áudio está
disponível no painel de gravação; faixas MIDI têm arm no Piano Roll.

Os quatro slots de insert oferecem NOVA EQ, Compressor, Gate, Delay e Reverb,
editor, bypass e remoção. **Scan VST3** pesquisa os locais padrão do Windows
e os diretórios adicionais salvos. **Pasta VST3** adiciona um diretório
persistente; **Plugins** mostra os caminhos configurados. Para experimentar o VST3 próprio
de teste, selecione `build/ninja-debug/NovaTestGain_artefacts/Debug/VST3`.
A lista aparece no submenu VST3 dos inserts. O scanner usa processos filhos,
com timeout de 20 segundos por módulo. Conserve `NovaPluginScanner.exe` ao lado
de `NOVA DAW.exe`. Lista e blacklist são guardadas em `.nova-plugin-list.xml`;
uma nova varredura tenta novamente os módulos. Efeitos exigem um bus
stereo de entrada/saída; instrumentos exigem MIDI, saída stereo e zero entradas
de áudio ativas. Plugins não encontrados preservam
slot/estado, deixam o sinal seco passar com o último atraso conhecido e mostram `[ausente]`.
**Plugins** exibe fabricante, versão, caminho e blacklist; permite remover
descrições da lista ou limpar falhas. Isso preserva os módulos instalados e
os inserts já carregados na sessão.

**Auto Off/Read** abre a edição de volume, pan ou parâmetros de inserts.
Escolha destino/parâmetro e edite um ponto por linha (`segundos : valor`).
**Aplicar lane** substitui os pontos; uma lista vazia remove a lane.
Tempos devem ser crescentes. Volume usa ganho linear 0..2, pan -1..1,
VST3 0..1 e DSP nativo as unidades do editor. A interpolação segue seek/loop.
Volume/pan são calculados por amostra; parâmetros usam segmentos de até
16 amostras. A edição de pontos é textual; envelopes gráficos estão pendentes.

**CLIP** retém overload e permite limpar a indicação. Os medidores P/RMS usam
escala de -60 a 0 dBFS. Faders, entrada, pan, mute/solo, sends e bypass têm
transições suaves. Alterações estruturais podem interromper brevemente o áudio.
Em janelas pequenas, use a rolagem da mesa para acessar todos os controles.

Larguras, zoom, vista Edit/Mix e Snap são restaurados por
`.nova-preferences.json`, local à pasta deste projeto.

Sessões versão 6 guardam também MIDI/instrumentos, gravação/entradas/takes, fades
e fontes/resultados de transformações/comping. Versões 1–5 continuam legíveis.
A versão 3 guardava regiões, mixagem, nomes/cores/alturas, entrada, buses,
roteamento, inserts, estado opaco VST3, sends, automação, tempo/loop e Master.
Versões 1/2 continuam legíveis, com valores padrão para os novos recursos.
A captura do estado de plugins ocorre com o processamento de áudio suspenso
durante a operação.
As referências aos WAV são relativas. Os arquivos
de áudio não são copiados para a pasta de sessão; preserve os arquivos e sua
posição relativa ao transportar um projeto. Dados ausentes/inválidos cancelam
a abertura sem substituir a sessão atual. A gravação da sessão utiliza arquivo
temporário e substituição do destino. Nova sessão, Abrir e Fechar pedem confirmação
antes de descartar alterações não salvas.

## Arquitetura e limites

- `Source/Audio`: buffers, waveform e mixer com callback sem operações de disco,
  mutexes próprios ou alocações deliberadas; controles atômicos lock-free no alvo x64.
  O wrapper VST3 usa try-lock sem esperar, com silêncio/contador em contenção.
  Plugins externos podem alocar/bloquear/falhar dentro de seu próprio código;
  os testes com a fixture própria não certificam segurança de terceiros.
- `Source/Session`: validação/persistência JSON, preferências e histórico de edição.
- `Source/UI`: timeline, mesa e controles vinculados ao motor.
- `Source/App`: janela desktop e ciclo de vida JUCE.
- `tests`: C++20/Win32, JUCE e testes numéricos de áudio/sessão.

Limites desta fase: 64 faixas, 128 regiões por faixa, timeline de 24 horas e
512 MiB de WAV decodificado por sessão; somente
WAV mono/estéreo. Os arquivos são carregados integralmente na memória e a importação
ocorre na thread da interface, podendo pausá-la com arquivos grandes. Alterações
estruturais desanexam o callback antes de modificar buffers. Não existe streaming de leitura
para reprodução; captura e render escrevem em workers de disco. Clips importados começam no tempo zero e podem ser movidos.
Regiões sobrepostas somam seus sinais.

Mono usa pan de potência constante; estéreo usa balanceamento L/R. Reamostragem
linear permite arquivos com sample rates diferentes. Overload é mostrado nos
canais/Master e a saída recebe hard clipping em ±1. Não há limiter de masterização
ou streaming de reprodução. PDC alinha faixas/buses/sends/Master e preserva atrasos em bypass.
Limites: 65536 amostras/insert, 262144/caminho e 128 MiB de buffers de arestas.
A reprodução drena a PDC ao fim das regiões; caudas musicais adicionais de
delay/reverb após o fim da sessão ainda não são reproduzidas. Performance máxima
e compatibilidade com plugins externos dependem de validação por máquina.

A saída Realtek foi validada por callback WASAPI com amostras não nulas. A
audibilidade física não foi medida externamente. ASIO não está habilitado nesta
fase. Gravação/entradas foram implementadas na Fase 5 e validadas com sinais
simulados; hardware multicanal continua pendente.

O modo `--session "arquivo.novadaw"` abre uma sessão ao iniciar. O modo de teste
`--validate-ui "pasta"` precisa ser combinado com `--session` e uma sessão
com dois WAVs (por exemplo demo.novadaw de CTest). Importa os WAV referenciados, exercita gestos de edição,
atalhos, controles, layout, sessões e transporte/loop, salva uma sessão editada,
snapshots Edit/Mix e um resultado JSON, e encerra o aplicativo. As fixtures
`build/ninja-debug/test-data/demo.novadaw` são criadas por CTest.
Resultados históricos: `docs/phase2-report.md`, `docs/phase3-report.md`;
atuais: `docs/phase3-stability-report.md`.

O JUCE conserva sua licença AGPLv3/comercial em `external/JUCE/LICENSE.md`.
A modalidade apropriada para distribuição do produto ainda deve ser definida.

REA: diagnóstico e limitações em `docs/research/rea-status.md`. Nenhuma análise
de binários proprietários foi feita; Ghidra e IDA não foram instalados.

## Teste prolongado automatizado

CTest inclui uma reprodução de 10 segundos, concorrência e ciclos VST3.
Para executar 5 minutos com 32 faixas, 3 buses/returns, 10 instâncias do
VST3 próprio, cinco DSPs, Read, mudanças de parâmetros/rate/buffer e sessões:

```powershell
$taskRoot = (Get-Location).Path
$taskPlugin = Join-Path $taskRoot 'build/ninja-release/NovaTestGain_artefacts/Release/VST3/NOVA Test Gain.vst3/Contents/x86_64-win/NOVA Test Gain.vst3'
& './build/ninja-release/NovaStabilityTests_artefacts/Release/NovaStabilityTests.exe' (Join-Path $taskRoot 'build/stability-manual-run') $taskPlugin 300
```

O argumento aceita 10..3600 segundos. `stability-result.json` registra CPU,
memória privada, duração do render, violações de prazo, alocações observadas
e erros de controle. Esse teste usa um render agendado, sem dispositivo de
áudio; audição física, ASIO, roundtrip e VST3 de terceiros exigem validação própria.

## MIDI e Piano Roll

Use **MIDI → Nova faixa MIDI**. No Piano Roll, selecione instrumento e entrada,
clique no teclado para preview ou desenhe notas na grade. Arraste notas para
mover e a borda direita para alterar duração. Shift seleciona várias notas;
desative Desenhar para seleção por retângulo. Velocity/canal, Copy/Paste/Delete,
Quantizar, Snap, zoom, Undo/Redo, início/duração/offset e duplicação são funcionais.
Clips aparecem na timeline; duplo clique abre seu Piano Roll.

O instrumento **NOVA Test Synth** é original e compilado no build com testes;
nenhum VST3 é instalado globalmente. O scan local da fixture é iniciado na
interface. Para outros instrumentos, adicione sua pasta e faça Scan VST3.
Instruments aceitos: MIDI, saída stereo e zero entradas de áudio ativas.
Interface, programas e presets .novapreset preservam o estado opaco.

Arme a faixa e selecione sua entrada para **Record MIDI**. Também é possível
gravar o teclado visual. Eventos são drenados fora do callback e cada take
gera um .mid de recuperação em Recordings/MIDI. Parar, seek/mudança de tempo
ou interrupção fecha o take; MIDI recebido é preservado. Não há mapa de tempo
variável. Entrada física ainda precisa de validação no dispositivo do usuário.
**Loop clip**, metrônomo, importação/exportação .mid e sessão v4 estão disponíveis.

A validação **--validate-midi-ui "pasta"** exercita gestos/controles, áudio real
da fixture no callback de saída e save/reopen MIDI. Ela não testa dispositivo
MIDI físico nem instrumento de terceiros.

## Gravação de áudio (Fase 5)

Em **Audio**, ative as entradas e escolha device/rate/buffer. Use **Gravacao →
Nova faixa de audio**, configure canais em **Entradas, Record, Punch e Takes**,
arme as faixas e pressione Record. Stop fecha os WAVs e cria regiões/takes.
Punch/roll e loop usam segundos da timeline. Ativar take preserva arquivos anteriores.
Input Monitoring vem desligado; habilite explicitamente com fones e evite feedback.
**Recuperar captura** aceita pastas `capture-*` com `recovery.json` e WAVs válidos.
Capturas ficam em `Recordings/Audio` ao lado da sessão (ou na raiz antes de salvar).
As pastas de captura são fontes da sessão; mantenha-as junto ao projeto.
Validação automatizada usa entradas simuladas; hardware de gravação não certificado.

## Edição avançada e render (Fase 6)

Selecione clips com Shift/Ctrl-click ou Selector na timeline; Ctrl+A seleciona áudio.
Use **Editar → Editor de audio / Fades / Comp / Render**. Arraste os pontos da curva
para mudar fades; escolha Linear, Exponential ou Equal-power. Crossfade requer dois
clips adjacentes/com handles ou sobrepostos na mesma faixa. Tool Slip desloca a
janela da fonte; Ripple afeta os clips seguintes. Duplicação e edição têm Undo/Redo.

No editor, configure fator de duração, semitons e cents. **Pre-visualizar audio**
processa em background e toca o resultado sem alterar o clip; **Aplicar preview**
ou **Aplicar duracao/pitch** adota o resultado. **Reverter ao original** restaura a
janela anterior à primeira transformação. O algoritmo é aproximado; artefatos e
limites estão no relatório. Nenhuma fonte é reescrita.

Selecione um take e intervalo em segundos para **Comp trecho**. **Consolidar clips
selecionados** renderiza antes do mixer na faixa atual; **Bounce selecao** renderiza
o mix completo do intervalo selecionado (ou dos clips selecionados), incluindo
VST3, buses/sends/Read/PDC. Resultados ficam em `Rendered`, sem sobrescrever WAVs.
Mantenha `.nova-cache`, `Rendered`, `Recordings` e fontes junto ao projeto ao copiar
sessões. Cancelamento limpa apenas temporários do job; cache usado não é apagado.

Validações opcionais da interface (um processo por vez):

```powershell
& '.\build\ninja-release\NovaDAW_artefacts\Release\NOVA DAW.exe' --validate-advanced-ui "$PWD\build\ui-validation-phase6-release"
& '.\build\ninja-release\NovaDAW_artefacts\Release\NOVA DAW.exe' --validate-recording-ui "$PWD\build\ui-validation-phase5-release"
& '.\build\ninja-release\NovaDAW_artefacts\Release\NOVA DAW.exe' --validate-midi-ui "$PWD\build\ui-validation-phase4-release"
```

Esses modos criam fixtures/sessões e `ui-result.json` nos diretórios indicados e
fecham o aplicativo. Não validam gravação física ou plugins de terceiros.

## Regressões de recuperação e bounce

Após compilar, execute nas duas configurações:

```powershell
.\scripts\dev-shell.cmd ctest --preset windows-ninja-debug -R AudioRecordingAndTakes
.\scripts\dev-shell.cmd ctest --preset windows-ninja-debug -R AdvancedAudioEditing
.\scripts\dev-shell.cmd ctest --preset windows-ninja-release -R AudioRecordingAndTakes
.\scripts\dev-shell.cmd ctest --preset windows-ninja-release -R AdvancedAudioEditing
```

Incluem recuperação repetida/manifesto duplicado no limite de 128 takes,
preservação do arranjo/fontes, cancelamento/erro do bounce, transição de latência
no último bloco e exceção com bypass automático. Transições são injetadas por
fixture interna controlada; os testes também processam os VST3/VST3i reais próprios.
O bounce abortado não publica WAV parcial e encerra seu transporte offline.
Notificações externas que chegam depois do render continuam fora dessa garantia.

## Desenvolvimento da Fase 7

Escopo: [docs/roadmap.md](docs/roadmap.md). Resultados e limites:
[docs/phase7-report.md](docs/phase7-report.md).
O teste prolongado conserva o critério de prazo `frames/sampleRate` e agora
registra P50/P95/P99, detalhes de cada overrun, ciclos da thread, atrasos para
acordar e contadores de silêncio por mutação/manutenção. Também mede buffers
conhecidos, alocações C++ vivas e memória após limpar a sessão. Ciclos não são
convertidos em tempo; probes não cobrem todos os alocadores de DLLs.

Em MIX, **Auto** abre modos Off/Read/Write/Touch/Latch. Crie uma lane com
**Aplicar lane**; arraste pontos na curva e use Shift-clique para excluir.
Linear/Hold/Smooth escolhem interpolação. O fader/pan MIX, editores de DSP nativo
e o slider de destino do painel gravam os gestos. Write grava as lanes;
Touch volta ao Read ao soltar e Latch mantém até Stop. Undo/Redo de automação
estão no painel, após parar o transporte. Salvar fecha a captura pendente.
Captura tem resolução de UI (30 Hz); Read continua calculado no render.
O slider do painel controla parâmetros VST3 reais; editores de terceiros
arbitrários ainda não enviam gestos ao gravador de automação.

**VCA / SC** em cada canal MIX configura membership em VCA/grupo, ganho/mute,
solo de grupo, Solo Safe e fonte de sidechain do NOVA Compressor/Gate.
Key externo tem PDC inclusive com inserts de latência antes do detector.
Sends oferecem pan, largura estéreo e mute. O playback permanece stereo;
ChannelMatrix prepara a evolução para mais canais, sem habilitar surround no device.
