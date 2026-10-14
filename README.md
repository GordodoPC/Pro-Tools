# NOVA DAW — Fase 2

Aplicativo desktop próprio para Windows em C++20/JUCE. Esta versão implementa
Edit/Mix, importação WAV mono/estéreo, waveform real, reprodução simultânea,
Play/Pause/Stop, seek, loop, volume, pan, solo, mute e sessões `.novadaw`.
A versão 0.2.0 adiciona edição não destrutiva de regiões, Undo/Redo,
grade musical, painéis redimensionáveis, preferências e saída Master stereo/L/R.
Gravação, MIDI, VST3, ASIO, buses, sends, automação e exportação estão pendentes.

## Compilar no Windows

Requisitos: Visual Studio Build Tools 2026 com Desktop Development with C++,
MSVC x64/x86, Windows SDK 10.0.26100, CMake 3.22+ e Git for Windows.
As versões verificadas estão em `docs/environment-report.md`.

O JUCE é fixado em 9.0.4 pelo arquivo `dependencies.json`. Para repor uma cópia
ausente, execute `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/bootstrap.ps1`
(faz download do repositório oficial; a política é limitada ao processo).
O script não substitui uma dependência existente com commit diferente.

No PowerShell, dentro desta pasta:

```powershell
.\scripts\dev-shell.cmd cmake --preset windows-ninja-debug
.\scripts\dev-shell.cmd cmake --build --preset windows-ninja-debug
.\scripts\dev-shell.cmd ctest --preset windows-ninja-debug

.\scripts\dev-shell.cmd cmake --preset windows-msvc
.\scripts\dev-shell.cmd cmake --build --preset windows-msvc-debug
.\scripts\dev-shell.cmd ctest --preset windows-msvc-debug
```

Executáveis: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe` e
`build/msvc/NovaDAW_artefacts/Debug/NOVA DAW.exe`.
São builds locais Debug, não um pacote de distribuição.

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
REC está desabilitado, e Inserts/Sends indicam recursos pendentes.

Larguras, zoom, vista Edit/Mix e Snap são restaurados por
`.nova-preferences.json`, local à pasta deste projeto.

Sessões versão 2 guardam regiões, estados de mixagem, nomes/cores/alturas/saídas,
tempo, compasso e loop; sessões da Fase 1 continuam legíveis.
As referências aos WAV são relativas. Os arquivos
de áudio não são copiados para a pasta de sessão; preserve os arquivos e sua
posição relativa ao transportar um projeto. Dados ausentes/inválidos cancelam
a abertura sem substituir a sessão atual. A gravação da sessão utiliza arquivo
temporário e substituição do destino. Nova sessão, Abrir e Fechar pedem confirmação
antes de descartar alterações não salvas.

## Arquitetura e limites

- `Source/Audio`: buffers, waveform e mixer com callback sem operações de disco,
  mutexes ou alocações deliberadas; controles atômicos lock-free no alvo x64.
- `Source/Session`: validação/persistência JSON, preferências e histórico de edição.
- `Source/UI`: timeline, mesa e controles vinculados ao motor.
- `Source/App`: janela desktop e ciclo de vida JUCE.
- `tests`: C++20/Win32, JUCE e testes numéricos de áudio/sessão.

Limites desta fase: 64 faixas, 128 regiões por faixa, timeline de 24 horas e
512 MiB de WAV decodificado por sessão; somente
WAV mono/estéreo. Os arquivos são carregados integralmente na memória e a importação
ocorre na thread da interface, podendo pausá-la com arquivos grandes. Alterações
estruturais desanexam o callback antes de modificar buffers. Não existe streaming
de disco nesta versão. Clips importados começam no tempo zero e podem ser movidos.
Regiões sobrepostas somam seus sinais.

Mono usa pan de potência constante; estéreo usa balanceamento L/R. Reamostragem
linear permite arquivos com sample rates diferentes. Overload é mostrado no
medidor master, e a saída recebe hard clipping em ±1. Não há limiter de masterização
nem fades automáticos ou smoothing de ganho/pan; alterações abruptas podem produzir clicks. Ainda não
há validação de desempenho de longa duração, latência roundtrip ou carga máxima.

A saída Realtek foi validada por callback WASAPI com amostras não nulas. A
audibilidade física não foi medida externamente. ASIO não está habilitado nesta
fase; gravação e entrada de áudio também não estão implementadas.

O modo `--session "arquivo.novadaw"` abre uma sessão ao iniciar. O modo de teste
`--validate-ui "pasta"` importa os WAV referenciados, exercita gestos de edição,
atalhos, controles, layout, sessões e transporte/loop, salva uma sessão editada,
snapshots Edit/Mix e um resultado JSON, e encerra o aplicativo. As fixtures
`build/ninja-debug/test-data/demo.novadaw` são criadas por CTest.
Resultados da continuação: `docs/phase2-report.md`.

O JUCE conserva sua licença AGPLv3/comercial em `external/JUCE/LICENSE.md`.
A modalidade apropriada para distribuição do produto ainda deve ser definida.

REA: diagnóstico e limitações em `docs/research/rea-status.md`. Nenhuma análise
de binários proprietários foi feita; Ghidra e IDA não foram instalados.
