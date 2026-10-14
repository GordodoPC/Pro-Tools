# NOVA DAW — Fase 1

Aplicativo desktop próprio para Windows em C++20/JUCE. Esta versão implementa
Edit/Mix, importação WAV mono/estéreo, waveform real, reprodução simultânea,
Play/Pause/Stop, seek, volume, pan, solo, mute e sessões básicas `.novadaw`.
Não inclui gravação, edição de regiões, MIDI, VST3, buses, sends ou automação.

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
3. Use Play/Pause/Stop. Clique na timeline para posicionar o transporte.
4. Alterne Edit/Mix. Volume, pan, M e S modificam o áudio em tempo real.
5. Use o menu Sessão para salvar/abrir `.novadaw`.
6. O botão Audio permite selecionar saída, sample rate e buffer.

Atalhos fixos iniciais: Espaço Play/Pause, Home Stop, Ctrl+I Importar,
Ctrl+O Abrir, Ctrl+S Salvar e Ctrl+Shift+S Salvar como.

Sessões guardam referências relativas aos WAV e estados de mixagem. Os arquivos
de áudio não são copiados para a pasta de sessão; preserve os arquivos e sua
posição relativa ao transportar um projeto. Dados ausentes/inválidos cancelam
a abertura sem substituir a sessão atual. A gravação da sessão utiliza arquivo
temporário e substituição do destino. Nova sessão, Abrir e Fechar pedem confirmação
antes de descartar alterações não salvas.

## Arquitetura e limites

- `Source/Audio`: buffers, waveform e mixer com callback sem operações de disco,
  mutexes ou alocações deliberadas; controles atômicos lock-free no alvo x64.
- `Source/Session`: validação e persistência JSON.
- `Source/UI`: timeline, mesa e controles vinculados ao motor.
- `Source/App`: janela desktop e ciclo de vida JUCE.
- `tests`: C++20/Win32, JUCE e testes numéricos de áudio/sessão.

Limites desta fase: 64 faixas; 512 MiB de WAV decodificado por sessão; somente
WAV mono/estéreo. Os arquivos são carregados integralmente na memória e a importação
ocorre na thread da interface, podendo pausá-la com arquivos grandes. Alterações
estruturais desanexam o callback antes de modificar buffers. Não existe streaming
de disco nesta versão. Todas as faixas começam no tempo zero.

Mono usa pan de potência constante; estéreo usa balanceamento L/R. Reamostragem
linear permite arquivos com sample rates diferentes. Overload é mostrado no
medidor master, e a saída recebe hard clipping em ±1. Não há limiter de masterização
nem smoothing de ganho/pan; alterações abruptas podem produzir clicks. Ainda não
há validação de desempenho de longa duração, latência roundtrip ou carga máxima.

A saída Realtek foi validada por callback WASAPI com amostras não nulas. A
audibilidade física não foi medida externamente. ASIO não está habilitado nesta
fase; gravação e entrada de áudio também não estão implementadas.

O modo `--session "arquivo.novadaw"` abre uma sessão ao iniciar. O modo de teste
`--validate-ui "pasta"` importa os WAV referenciados, exercita controles e transporte,
salva snapshots Edit/Mix e um resultado JSON, e encerra o aplicativo.

O JUCE conserva sua licença AGPLv3/comercial em `external/JUCE/LICENSE.md`.
A modalidade apropriada para distribuição do produto ainda deve ser definida.

REA: diagnóstico e limitações em `docs/research/rea-status.md`. Nenhuma análise
de binários proprietários foi feita; Ghidra e IDA não foram instalados.
