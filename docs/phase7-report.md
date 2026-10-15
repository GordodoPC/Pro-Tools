# NOVA DAW — Fase 7: mixagem e masterização avançadas

Início: 2026-10-15. Base preservada: `73f07da`, após `a8df20d`.
Escopo autorizado em [roadmap.md](roadmap.md). Versão 0.7.0; entrega funcional
com limitações e validações externas pendentes discriminadas abaixo.

Entrega funcional validada em `805c5e7`: CTest15/15 Debug/Release,593 checks e
dois smoke por build; dez execuções UI da entrega aprovadas. Último ensaio
integrado300s: **10 violações de prazo**, max26,244ms, CPU20,36% de um núcleo,
zero new C++ observado/erros de controle. Estabilidade permanece **parcial**;
não se declara a Fase7 integralmente concluída quanto a prazos ou validação externa.

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
Percentis agregam os cinco tamanhos de bloco, incluindo callbacks silenciosos
por gate/manutenção; o orçamento é individual por frames/rate. Gate/silêncio são
publicados à parte para não confundir pouco trabalho com áudio integral entregue.

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
Grupos/sidechain e masterização foram integrados nas etapas seguintes.

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

### Etapa 4 — medição e masterização

MasteringMeter mede o sinal de saída efetivo após Master/preview. Implementação
própria de K-weighting stereo, janelas de 400 ms/75% de sobreposição, gate
absoluto -70 LUFS e relativo -10 LU para integrada; momentânea sem gate.
Referência algorítmica: [ITU-R BS.1770-5](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf).
Coeficientes de 48 kHz são mapeados por transformação bilinear para o rate ativo.
True Peak é estimado por FIR sinc/Blackman 4x, 32 taps por fase; o exportador
esvazia os 32 samples restantes do detector. Crest e PLR mostram dinâmica;
FFT 2048/Hann da soma mono alimenta espectro de 1024 bins na thread de controle.

Render só filtra/acumula e publica em filas SPSC prealocadas. FFT, histórico e
gating integrado ficam no controle/worker. Limites: 256 energias pendentes,
quatro quadros de espectro e 864000 janelas de integrada (24 h). Overflow é
contado, sem espera nem crescimento de fila; exportação recusa medição incompleta.
Histórico cresce no controle até o limite e mantém sua capacidade após Reset.
Medição é opt-in ao abrir o painel, desligada no engine por padrão. Callbacks
inibidos por MutationGuard não entram no medidor. Não é ensaio de conformidade
com toda a coleção de vetores ITU/EBU, nem certificado de precisão near-Nyquist.

Exportação cria snapshot da sessão, prepara o sample rate no controle e renderiza
em worker para arquivo float temporário. Passagens posteriores medem e aplicam
ganho/TPDF/escrita, fora do callback. WAV PCM16, PCM24 e float32; opções aceitam
8–192 kHz, painel oferece 44,1/48/88,2/96/192 kHz. Novo destino é publicado sem
sobrescrever arquivo existente, somente após flush; cancelamento remove temporários.
São usados JUCE já fixado no projeto e código próprio, sem biblioteca/dependência
nova. A licença de distribuição permanece a já aplicável ao projeto/JUCE
(AGPLv3 ou licença comercial correspondente).

Normalização usa o menor ganho entre alvo LUFS e teto de True Peak estimado.
Quando o teto impede o alvo, o resultado informa loudness alcançável; não se
insere limiter. Silêncio/intervalo sem 400 ms úteis não ganha LUFS inventado.
TPDF usa duas distribuições uniformes independentes por diferença, um LSB;
float32 não recebe dither. O hard clip existente do motor permanece anterior
à medição: exportar float não recupera sinal já clipado. O teto é estimado antes
da quantização/dither; diferenças de poucos LSBs não equivalem a limiter certificado.

Stems isolam cada fonte não auxiliar, mantêm seus sends/buses/Master e deixam
detector de sidechain disponível. Cada arquivo tem nome único e opção de
normalização independente. Processamento compartilhado não linear pode impedir
que a soma dos stems reproduza exatamente o mix. Exporta-se o intervalo escolhido,
sem extensão automática de caudas. Em erro/cancelamento, stems já concluídos são
preservados; nenhum WAV original é modificado.

35 checks de MasteringAndExport passaram em Debug e Release, incluindo áudio
decodificado: referência stereo 997 Hz/0,1 a 44,1/48/96 kHz mediu
-19,9993/-19,9999/-20,0023 LUFS (tolerância 0,12 LU), True Peak de seno 12 kHz
com pico entre samples (tolerância 0,15 dB), gating, silêncio, crest, espectro,
estatística TPDF, formatos/rates/frames, normalização -14 LUFS e teto, stems,
persistência, cancelamento e fontes byte-for-byte. Probe não observou C++ new
no render com medição habilitada. Não substitui inspeção de alocadores estrangeiros.

### Etapa 5 — interface MIX

Auto abre modos, curva gráfica/textual, parâmetro ao vivo e Undo/Redo;
VCA / SC abre grupos/Solo Safe/key e os sends têm controles avançados.
LUFS / Export no Master abre LUFS M/I, True Peak, crest/PLR, espectro, reset,
formato/rate/intervalo, normalização/dither e jobs reais de mix/stems/cancelamento.
UI acompanha progresso e mostra arquivo/loudness/ganho. O snapshot é preparado
antes do worker e plugins não são destruídos enquanto em processamento.

### Etapas 6/7 — regressão e documentação

Builds Ninja x64 da versão 0.7.0 passaram em Debug e Release. CTest antes da
otimização adicional da FIR:
**15/15 Debug (47,55 s)** e **15/15 Release (19,08 s)**, **593 checks e dois
smoke tests por configuração**. Todas as suítes anteriores continuam habilitadas.
Logs em `build/{ninja-debug,ninja-release}/Testing/Temporary/LastTest.log`.
Resultados de interface e ensaio integrado prolongado são registrados abaixo
após sua execução; nenhuma aprovação é inferida a partir do build.

Dez execuções UI passaram, uma por vez, em
`build/phase7-final-ui-{editmix,midi,recording,advanced,mastering}-{debug,release}`.
MIX/Master inspecionados visualmente; exportação do painel gerou PCM16/44100 Hz,
88200 frames e configuração reaberta. Realtek48k/480: no Master Debug136 callbacks,
média1591,42us/max3810,5us, Release84 média95,05us/max248us, zero overrun.
LUFS ao vivo inclui silêncio de início e é diferente da medição offline isolada;
nenhum teste de captura física ou terceiros foi inferido desses resultados.

Correção da revisão final: slider ao vivo de automação agora notifica sessão
modificada, e Aplicar/Undo atualizam curva/modo. A UI Edit/Mix foi reexecutada
em Debug/Release com binding e notificação exigidos, ambos aprovados em
`build/phase7-final-corrected-ui-editmix-{debug,release}`.

### Ensaio integrado adicional — primeira execução

`build/phase7-final-stability-300s/stability-result.json`: 300,090s, 35379 callbacks,
13818404 samples, **32 violações**, máximo55,757ms, P50/P95/P99
1068/5205/6933us, CPU23,02% de um núcleo, render thread61,359s.
Zero C++ new observado/controle/contenção VST, zero overflow de medição/automação,
integrada-24,075LUFS/TruePeak-21,333dBTP, 11 recalculos de latência.
Gate804/manutenção3778; todas as violações com ação0 na entrada/saída.

LUFS/TP e drops do medidor nesse JSON são o estado final desde seu último Reset
(o teste troca sample rate e recarrega sessão periodicamente); não representam
LUFS acumulada nem contador de drops preservado através de todos os resets dos
300s. Automação mantém seu contador de overflow; probes de render/tempo cobrem
o ensaio inteiro. Essa distinção também se aplica à execução integrada seguinte.

Essa carga opt-in acrescentou grupos/VCA, dois sidechains nativos, captura
Write/Touch/Latch e medidor ao workload de32 faixas/3buses/10VST3; não substitui
nem é a mesma carga da comparação4→2. `passed=true` valida finitude/estado/
alocações/overflow; **não aprova cumprimento de prazo**. O máximo e o aumento
dos atrasos exigem continuidade de estabilização, sem relaxar frames/sampleRate.

Private inicial/final/pico:92631040/110497792/112222208 bytes; após clear91865088.
C++ vivos amostra0/300:8472166/8522711, objetos83575/83928, buffers conhecidos
1895264/1833368 bytes. Após clear:3575074 bytes/868 objetos e335160 bytes de
buffers conhecidos. Histórico das medições/automação e capacidade de heap/cache
explicam parte da retenção, sem evidência de crescimento de18MB em objetos
medidos; DLLs e alocadores fora do probe continuam não certificados.

O custo do medidor foi isolado em benchmark Release de1000 blocos de480 frames,
três passagens, excluindo FFT/service do cronômetro:154,210/143,568/126,521us
por bloco antes da otimização adicional. Foi preparada convolução SSE2 em
double com histórico espelhado/prealocado, mantendo quatro fases/32 taps e
coeficientes, para reduzir esse trabalho comprovado. Medição posterior e novo
ensaio integrado serão registrados após validação.

Benchmark depois:37,6148/43,0453/47,1140us por bloco; mediana143,568→43,0453us
(aproximadamente70% menos wall time nesse microbenchmark). As mesmas35
verificações numéricas/exportações passaram. SSE2 é garantido pelo alvo Windows
x64 já usado; não se alteraram filtro, número de fases/taps, precisão double ou
critérios/tolerâncias. Logs em `build/phase7-meter-{before,after}.log`; modo
opcional `NovaMasteringTests.exe "pasta" benchmark`. Não atribui todos os32
atrasos à FIR: a primeira carga também teve grandes atrasos de despertar.

Após SSE2 e correção da UI, rebuilds completos aprovados. CTest final
**15/15 Debug (46,40s)** e **15/15 Release (20,59s)**, mantendo593 checks+2smoke
por configuração e todas as tolerâncias. Logs atuais em Testing/Temporary;
logs anteriores preservados em `build/phase7-regression-before-simd`.

Dez execuções UI da entrega após essas correções passaram em
`build/phase7-delivery-ui-{editmix,midi,recording,advanced,mastering}-{debug,release}`.
Realtek48k/480, zero overrun nas cenas curtas medidas:

| Configuração/cena | Callbacks | Média us | Máximo us | P99 superior us |
|---|---:|---:|---:|---:|
| Debug Edit/Mix | 474 | 983,10 | 4227,6 | 3500 |
| Debug MIDI | 183 | 543,83 | 1655,5 | 1500 |
| Debug Master | 114 | 632,20 | 1691,0 | 1700 |
| Release Edit/Mix | 373 | 58,02 | 322,6 | 300 |
| Release MIDI | 138 | 42,21 | 156,8 | 200 |
| Release Master | 84 | 43,72 | 136,7 | 200 |

Gravação UI usa entradas simuladas e WAVs reais, MIDI usa instrumento próprio
real com saída de dispositivo; nenhuma validação física de entrada é inferida.

## Reprodução e entrega

Compilar/CTest Debug e Release: presets e comandos no [README](../README.md).
Ensaio integrado: executar NovaStabilityTests com argumentos pasta, caminho do
NOVA Test Gain.vst3, `300 phase7`; sem `phase7` conserva a carga histórica.
UI de masterização: `NOVA DAW.exe --validate-mastering-ui "pasta nova"`.
Rode um processo de validação por vez. Artefatos em build não são versionados.

Executável Release validado:
`build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe`.
SHA256: `1FD56437C10993E5428216177B54CCB1D72030ACBFF2144858C19E77C2557A05`.
Debug: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe`.
SHA256: `4E06CA6CFCA555F00D686E7BEE83A025AAA4799992CC551FF99F6E701F5A2486`.
Conserve NovaPluginScanner.exe ao lado do aplicativo e fontes/cache de sessões.
Nenhum instalador ou instalação global foi criado.

Commits locais validados: `9ff7c27` instrumentação/EQ/PDC, `9d759da` automação,
`a3bf187` mixagem profissional, `805c5e7` medição/masterização/exportação/UI.
Os commits `a8df20d` e `73f07da` foram preservados. Sem push, reset, rebase,
instalação de dependências/plugins ou alteração global.

## Situação funcional e pendências

| Área | Situação | Limites/pendências |
|---|---|---|
| Estabilidade | Investigação/correções executadas; estabilização parcial | Ainda há overruns; sem trace ETW causal ou ensaio de horas |
| Automação | Implementada/testada em faixas e Aux, volume/pan/inserts | Captura30Hz, um gesto Touch/faixa; Master/sends não têm lanes; gestos de editor externo pendentes |
| Grupos/sends/Solo Safe | Implementados/testados/persistidos | Oito VCA/oito grupos; dois sends por canal |
| Sidechain/PDC | Implementado/testado no Compressor/Gate nativo | Auxiliar VST3/destino instrumento não habilitados |
| Multicanal | Preparação implementada/testada em matriz | IO/render/dispositivo stereo; surround físico pendente |
| Medição/exportação | Implementadas/testadas com áudio e WAV reais | FIR4x estimada, sem certificação completa; sem limiter/caudas automáticas |
| MIX/persistência/regressão | Painéis reais e sessão v7 validados | Roteiro manual amplo, terceiros e entradas físicas pendentes |

O código foi desenvolvido para o projeto existente, sem copiar implementação
proprietária. A ausência de plugins externos não bloqueou recursos independentes.
As pendências acima não são declaradas concluídas pelos testes automatizados.

Roteiro manual ainda pendente: abrir cópia de sessão antiga e conferir áudio/
roteamento ao salvar v7; gravar gestos Touch/Write/Latch em material musical;
ouvir suavidade de curvas/grupos e compressor com key; conferir ganho/Solo Safe
em sessões grandes; comparar WAVs normalizados em medidor independente e ouvir
dither em material de baixo nível. Testar captura/MIDI físico e desconexão com
hardware disponível, além de horas de playback com contadores de prazo ativos.
Investigar escalonamento com trace de CPU/DPC/ISR e medição independente de
alocadores de DLLs antes de afirmar cumprimento hard real-time ou ausência de leak.

## Comparação integrada final após FIR SIMD

Artefato: `build/phase7-delivery-stability-300s/stability-result.json`, executado
após o commit805c5e7, sem builds/CTest/UI concorrentes. Mesma opção `300 phase7`,
mesmo orçamento individual, sem alterar duração, filtros ou tolerâncias.
41 checks de estabilidade passaram; `passed=true` é aceite funcional.

| Métrica, carga integrada Fase7 | FIR anterior | FIR SIMD |
|---|---:|---:|
| Duração s | 300,090 | 300,083 |
| Callbacks / samples | 35379 / 13818404 | 35375 / 13817475 |
| Violações de prazo | **32** | **10** |
| Máximo ms | 55,757 | 26,244 |
| P50 / P95 / P99 us | 1068 / 5205 / 6933 | 1002 / 4506 / 6584 |
| CPU processo % de um núcleo | 23,02 | 20,36 |
| CPU thread render s | 61,359 | 54,813 |
| Gate / manutenção | 804 / 3778 | 594 / 4100 |
| New C++ observado / erros controle / contenção VST | 0 / 0 / 0 | 0 / 0 / 0 |
| Private inicial / final bytes | 92631040 / 110497792 | 92938240 / 112209920 |
| Private após clear bytes | 91865088 | 88608768 |
| C++ vivos, amostra0 / 300 bytes | 8472166 / 8522711 | 8472182 / 8523723 |

Pico privado final112623616 bytes; engine buffers conhecidos1896032→1834136,
após clear335928. C++ após clear3574386 bytes/846 objetos, incluindo estruturas
do próprio teste. Private caiu23601152 bytes ao limpar; crescimento de C++ nas
amostras foi51541 bytes/353 objetos, coerente com histórico de medições/controle.
Retenção privada não corresponde a19MB de objetos vivos instrumentados;
isso não exclui leaks estrangeiros ou de longa duração.

Treze recalculos de latência, samples finitos, overflow de timings0 e automação0.
Estado final do medidor após seu último Reset: LUFS I-24,071/TP-21,338dBTP,
drops0 (limite de interpretação já registrado acima). Todas as dez violações
tinham ação0 na entrada/saída. Maior caso:198,959s,480frames/44100Hz,
wall26244us/orçamento10884,354us, despertar8610,470us atrasado,12014421 ciclos.
Não se convertem ciclos em tempo; preempção é hipótese, não conclusão causal.
O marcador controlAction cobre sends/grafo, rate, save/load e editor de plugins;
service/gestos de automação não recebem um código próprio. Valor0 não prova
ausência de toda atividade concorrente de controle; gate é contado separadamente.

O microbenchmark e a redução de CPU sustentam a redução de trabalho da FIR.
Variação de escalonamento/gates e os dez atrasos impedem declarar estabilidade
de prazo resolvida. Os quatro ensaios instrumentados são mantidos em suas pastas;
a comparação histórica4→2 e a integrada32→10 têm cargas diferentes entre si.
Próximos passos de estabilização: trace CPU/DPC/ISR, horas com dispositivo real,
contagem de drops preservada através de resets e probes de alocadores externos.
