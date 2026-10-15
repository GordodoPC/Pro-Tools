# NOVA DAW — Fase 7: mixagem e masterização avançadas

Início: 2026-10-15. Base preservada: `73f07da`, após `a8df20d`.
Escopo autorizado em [roadmap.md](roadmap.md). Implementação em andamento;
este registro será atualizado com resultados efetivamente executados.

## Etapa 1 — investigação

Referência anterior: 300,081 s, 35380 callbacks, 9 violações de prazo,
max render 19,082 ms, CPU 21,04% de um núcleo, memória privada inicial/final
89309184 / 109375488 bytes. Zero C++ new observado no callback. Os dados antigos
não contêm duração individual ou contexto das nove violações; não permitem
atribuição retroativa a uma função ou à preempção do Windows.

Instrumentação adicionada ao teste: tempos de parede/percentis, detalhes dos
atrasos, ciclos da thread, atraso ao acordar, ação de controle concorrente,
contadores de silêncio por mutação/manutenção, bytes dos buffers conhecidos,
bytes/objetos C++ ainda vivos, valores após limpar a sessão.

O critério de prazo permanece duração de render > frames/sampleRate.
Instrumentação e armazenamento são limitados/prealocados; JSON, ordenação e
resumos são produzidos após parar/join da thread. Contagens de memória não
instrumentam todas as DLLs, mallocs privados ou alocações alinhadas.

Ciclos são mantidos como contadores, sem convertê-los em tempo: a frequência
pode variar, conforme a [documentação Microsoft](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime).
Tempo total de CPU da thread é medido separadamente com GetThreadTimes.

Resultados instrumentados antes/depois: concluídos nesta etapa, com limites abaixo.

### Antes instrumentado (300 s)

`build/phase7-stability-before-300s/stability-result.json`: 300,068 s,
35379 callbacks, **4 violações**, máximo 13,190 ms, P50/P95/P99
911/4254/5831 µs, CPU de processo 18,66% de um núcleo, CPU da thread de render
49,656 s, zero new observado/erros de controle/contenções VST. As quatro
violações ocorreram com ação de controle 0 na entrada/saída; os atrasos para
acordar estavam entre 948 e 1783 µs. Esse dado é compatível com interferência
de escalonamento, mas não constitui trace ETW capaz de provar sua causa.
Os nove casos históricos não possuem dados individuais para essa análise.

Foram contados, separadamente, 523 callbacks inibidos por mutações e 4069
silêncios de manutenção. O workload histórico só atende manutenção uma vez por
segundo; a UI real atende a 30 Hz. Não se mudou essa carga na comparação.
O host próprio usa CAS/try-lock e não espera pelo controle; o cliente JUCE dos
plugins próprios conserva `getCallbackLock()` dentro da DLL. Zero contenção do
wrapper do host não demonstra que toda DLL ou Windows seja livre de esperas.

Memória privada inicial/final/pico: 92655616 / 110940160 / 112689152 bytes.
Nas amostras 0..300, bytes C++ vivos: 8422063 → 8466933 (+44870), objetos
83535 → 83872 (+337); cada medição acrescenta um registro ao histórico do teste.
Buffers conhecidos permanecem em aproximadamente 1,6 MiB. Limpar a sessão
reduziu memória privada para 93257728 bytes (liberou 17682432) e os objetos C++
vivos para 798. Isso separa o grande crescimento de private bytes dos buffers
vivos e aponta retenção de heap/cache; não prova ausência de leaks de DLLs ou
alocadores fora do probe. Não se esvaziou o heap artificialmente.

### Correções verificadas

EQ nativa recalculava sete biquads a cada 16 samples com parâmetros estáveis,
e dividia pelos denominadores dentro de cada amostra. Agora normaliza os
coeficientes ao atualizá-los e reutiliza-os até mudar parâmetro ou sample rate.
Três novas regressões verificam uma única construção em sinal estacionário,
mudanças com smoothing e invalidação por rate. O avanço do ring PDC usa comparação
no wrap em vez de divisão inteira por célula; os 116 checks de PDC preservam o sinal.

Builds Debug/Release aprovados; CTest **12/12** em ambos, **516 verificações**
mais dois smoke tests. Debug: 30,68 s; Release: 19,18 s. O ensaio de 300 s após
essas correções terminou antes da integração dos recursos funcionais.

### Comparação após correções e callback real

| Métrica (mesmo workload de 300 s) | Antes | Depois |
|---|---:|---:|
| Violações de prazo | 4 | 2 |
| Render máximo ms | 13,190 | 9,458 |
| P50 / P95 / P99 µs | 911 / 4254 / 5831 | 908 / 4162 / 5554 |
| CPU processo, % de um núcleo | 18,66 | 18,44 |
| CPU thread de render, s | 49,656 | 49,609 |
| Callbacks | 35379 | 35375 |
| Gate / manutenção | 523 / 4069 | 582 / 4097 |
| Memória privada inicial / final bytes | 92655616 / 110940160 | 94027776 / 111779840 |
| Memória privada após clear bytes | 93257728 | 95019008 |
| Bytes C++ vivos, amostra 0 / 300 | 8422063 / 8466933 | 8423743 / 8468489 |

Depois: 300,063 s, zero new observado/erros/contensões VST, 14 recalculos de
latência. Artefato `build/phase7-stability-after-300s/stability-result.json`.
Os dois atrasos foram em blocos de 64, sem ação de controle na entrada/saída.
A CPU mudou pouco; a redução observada não permite atribuir todos os atrasos
às divisões/EQ ou declarar solução do escalonamento. Permanecem dois overruns.
Nenhum critério/timeout/duração/carga foi relaxado, e nenhum heap foi compactado.

Instrumentação opcional do callback **real** adicionada, desligada por padrão.
Histogramas têm resolução de 100 µs; percentis abaixo são limites superiores.
Dispositivo Realtek 48000 Hz/480 frames; os 10 ms de orçamento permanecem reais.

| UI/callback | Callbacks | Média µs | Máximo µs | P99 superior µs | Overruns |
|---|---:|---:|---:|---:|---:|
| Debug Edit/Mix | 459 | 828,31 | 3332,5 | 3000 | 0 |
| Debug MIDI | 177 | 440,94 | 999,9 | 1000 | 0 |
| Release Edit/Mix | 362 | 47,25 | 274,6 | 200 | 0 |
| Release MIDI | 138 | 39,24 | 127,7 | 200 | 0 |

Essas cenas de UI são menores que o render agendado de 32 faixas; não são sua
equivalência em hardware. JSONs em `build/phase7-device-timing-{editmix,midi}-{debug,release}`,
todos `passed=true`. Áudio de saída observado; entrada física/MIDI físico e
terceiros continuam pendentes. CTest final desta etapa: Debug12/12 (31,16s),
Release12/12 (17,12s), 516 checks mais dois smoke por configuração.

## Etapas funcionais

### Etapa 2 — automação avançada

Implementados Read/Write/Touch/Latch por faixa, volume/pan/parâmetros de inserts,
curvas lineares/hold/smoothstep, editor gráfico/textual, histórico próprio de
32 operações/64 MiB e sessão v7 compatível com v1–6. Read conserva avaliação por
sample para volume/pan e segmentos de 16 samples para parâmetros, com PDC.

Captura na thread de controle a 30 Hz, sem alocar/esperar/modificar vetores no
render. Máscara atômica de 64 lanes identifica overrides; Touch devolve ao Read
ao soltar, Latch permanece até parar, Write grava lanes configuradas mesmo sem
gesto contínuo. Salvar/edições estruturais fecham capturas antes da troca.
O histórico de regiões continua independente, preservando Undo/Redo anteriores.

Limites: 64 lanes/faixa, 4096 pontos/lane, até 65536 pontos pendentes; overflow
contado, captura interrompida/Read restaurado. Se a fusão ultrapassa 4096 pontos,
a lane original é mantida e a nova captura daquela lane não é adotada.
Não é gravação sample-accurate de gestos; playback mantém os offsets existentes.
VST3 tem captura pelo slider do painel de automação; gestos de editores externos
arbitrários ainda não estão conectados a listeners de automação do host.

23 verificações AdvancedAutomation passaram em Debug/Release: áudio de Touch/
Latch, Write, retorno ao Read, Undo/Redo, curvas, persistência e VST3 real próprio.
Regressão completa: Debug **13/13 (40,61 s)**, Release **13/13 (22,95 s)**,
539 verificações e dois smoke tests por build. UI Edit/Mix com editor/parâmetros,
sends/automação/Master passou em `build/phase7-automation-ui-{debug,release}`.
Grupos/sidechain e masterização são as próximas etapas.

### Etapa 3 — mixagem profissional

Implementados oito VCAs e oito grupos de canais, nomes/ganho/mute, solo de grupo,
membership independente, Solo Safe e suavização de ganho. VCA atua depois do
fader; sends pré-fader conservam seu nível. Sends incluem pan, width 0..2 e
mute com smoothing. Configurações/grupos persistem na sessão v7.

Sidechain externo funciona no NOVA Compressor/Gate, a partir do tap pós-fader
da fonte, independente de sua exclusão por solo. Grafo inclui dependência de
key, rejeita ciclos e alinha fonte principal/detector e cada estágio anterior
de insert. Fontes/grupos mutados não são promovidos a áudio audível por Solo Safe.
VST3 com buses auxiliares de sidechain e instrumento MIDI como destino de key
não estão habilitados; o painel identifica os DSPs nativos suportados.

Preparação multicanal: ChannelLayout/ChannelMatrix validam mono/stereo/5.1/7.1 e
executam matriz de roteamento/downmix. Playback, buses/plugin IO e dispositivo
continuam stereo; não se anuncia saída surround operacional por existir a API.

19 verificações ProfessionalMixing passaram: sinais reais em VCA/grupos/sends,
Solo Safe, estado, ciclo e gate com key VST3 de31 samples + insert anterior de17,
abertura exata no sample148 para impulso na fonte100, inclusive Read segmentado.
Regressão final: Debug **14/14 (33,01s)** e Release **14/14 (18,46s)**, 558 checks
mais dois smoke por build. UI/curva/Undo/grupos passou em
`build/phase7-mix-final-ui-{debug,release}`.

Regressão Debug encontrada e corrigida: estruturas grandes inline aumentaram a
pilha de testes com vários engines locais; duas execuções falharam antes de
executar suas verificações. Estado adicional passou a ownership no heap,
prealocado no controle, e os mesmos testes passaram sem aumentar sua pilha.
Uma asserção inicial usou a sobrecarga errada de getMagnitude; corrigida para
canal/start/count, mantendo a exigência numérica do gate no sample esperado.
