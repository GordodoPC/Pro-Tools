# NOVA DAW — Fase 3.5: estabilização profissional

Data: 2026-10-14. Base: `05c9ae9e57e9d1719ea7d42b9f4dc93e1b1fd5ef`.
Primeiro commit validado de PDC/proteção: `b3e0a07`.

## Resultado e alcance

Implementados PDC, proteção contra mutações/destruição concorrente, remoção
das esperas identificadas no processamento do wrapper JUCE, preparação fora
do callback e diretórios VST3 personalizados persistentes. Os testes usam
módulos VST3 reais da fixture original NOVA Test Gain, compilados localmente.
Nenhum plugin de terceiros foi instalado ou certificado.

A pendência de compatibilidade com terceiros não bloqueia Fases 4, 5 e 6,
conforme instrução posterior do usuário. MIDI não foi implementado nesta
estabilização. O escopo autorizado das Fases 4/5/6 está em
[roadmap-phases-4-6.md](roadmap-phases-4-6.md); elas serão executadas nessa ordem.

## Auditoria do caminho de áudio

Inspecionados `audioDeviceIOCallbackWithContext → render → renderBlock`,
roteamento, smoothing, Read, MixProcessor, runtime/UI/persistência e wrapper
JUCE 9.0.4. Não foram encontrados acessos a disco no render próprio. WAV,
JSON, estado opaco e descoberta/carregamento ficam na thread de controle
ou no scanner filho. Buffer, grafo e lifecycle não são construídos no render.

| Achado | Correção e limite |
|---|---|
| SpinLock de `processBlock` e `processBlockBypassed` esperava por lifecycle | ScopedTryLock, uma tentativa, silêncio e contador em contenção; lifecycle conserva sua exclusão |
| Listas de eventos com CriticalSection e crescimento | Armazenamento antecipado de 2048 eventos, rejeição de excesso, propriedade da chamada process; sem lock na lista do host |
| Restart via AsyncUpdater podia postar/alocar no áudio | Flags atômicas drenadas por Timer a 60 Hz na interface |
| Parâmetros de saída chamavam listeners da interface | Valores atômicos e notificações drenadas pela interface |
| Suspensão/latência JUCE lidas enquanto outra thread escreve | Campos atômicos; sinal seco compensado quando suspenso |
| Estruturas públicas podiam ser trocadas durante render | MutationGuard nos pontos estruturais e contrato explícito para edições diretas |
| Preparação VST3 em início do dispositivo | Callback publica rate; manutenção da interface prepara/recalcula |
| Reset VST3 em seek | Executado na thread de controle, após quiescer render |
| Troca/destruição/editor de insert em uso | Render termina antes de destruir; abertura e fechamento pela interface sob proteção |
| Bypass/sends/buses sem alinhamento | PDC por caminho e sinal seco atrasado por insert |

`MutationGuard` tem **um proprietário de controle** (interface). A thread de
controle inibe novas chamadas e espera a chamada existente terminar. Render
usa uma única CAS e retorna silêncio se inibido/ocupado; não espera pelo
controle. Ganhos/pan/mute/solo/parâmetros nativos são atômicos. Edições diretas
em inserts, regiões, rotas e lanes exigem guard; não há suporte a múltiplas
threads de mutação simultâneas. Os testes exercitam render em outra thread.

Buffers do mixer são fixos (segmentos de até 256 amostras); views stereo
usam ponteiros internos JUCE. PDC e delay nativo invalidam suas filas por
geração em seek. Preparação redundantemente idêntica é evitada. Os atomics
usados em áudio têm verificação lock-free no alvo x64. O medidor publica
somente após o callback completo. Falha C++ no process marca o insert como
falho e usa dry compensado, sem construir texto/objetos no callback.

As adaptações são reproduzíveis por `cmake/JuceRealtimePatch.cmake` e
`patches/juce-9.0.4/realtime.json`: SHA256 normalizado de entrada/saída,
verificação de todos os arquivos antes de escrita, aplicação idempotente e
recusa de fontes desconhecidas. JUCE upstream continua fixado pelo projeto.
A reprodução dos quatro patches a partir dos blobs upstream foi conferida
por SHA256, inclusive comparação com as fontes efetivamente compiladas.
A macro `NOVA_VST3_RT_GUARD` está ativa no host e na fixture própria; ela não
modifica binários de terceiros. Licenças JUCE AGPLv3/comercial e SDK VST3 MIT
permanecem nos arquivos originais da dependência.

Referências oficiais: [AudioProcessor/latência](https://docs.juce.com/master/classjuce_1_1AudioProcessor.html),
[notificações](https://docs.juce.com/master/structjuce_1_1AudioProcessorListener_1_1ChangeDetails.html)
e [SpinLock/try-lock](https://docs.juce.com/master/classjuce_1_1SpinLock.html).

## Compensação de latência

A preparação consulta `getLatencySamples()` do plugin. Uma ordem topológica
validada acumula input/insert/output em cada canal; entradas de buses e Master
são alinhadas ao maior caminho afluente. Cada rota e send tem uma fila stereo
própria, alocada na preparação. Master soma sua própria cadeia de inserts.
Sends pré-fader saem após inserts, antes do fader; pós-fader saem depois dele.
Ambos chegam alinhados ao destino, inclusive quando existem rotas paralelas.
O grafo considera conexões configuradas mesmo com send em nível zero, evitando
mudanças de latência a cada movimento de nível/mute/solo/bypass.

Dry/wet e bypass conservam a latência de cada insert. Plugins ausentes usam
seu último atraso persistido; sessões antigas sem esse campo usam zero.
Suspensão usa imediatamente o sinal seco atrasado. Pan de bus, fader e
parâmetros Read usam o tempo de origem do áudio em cada etapa, incluindo
latência acumulada e loop. A reprodução drena a PDC no fim das regiões e
mantém a posição visual no fim lógico; caudas musicais adicionais continuam
pendentes. PDC não é compensação de latência roundtrip de hardware.

Rotas/sends são alterados de forma transacional: ciclos/destinos inválidos
preservam o plano anterior. Mudanças de inserts/rate/latência reconstruem o
plano fora do render. Uma divergência **já reportada ao host** produz silêncio
até a manutenção. Notificações de plugins são assíncronas; transições podem
causar uma pausa e não têm garantia de troca perfeitamente contínua nem de
previsão da latência antes de o plugin reportá-la.

Limites: 65536 amostras por insert, 262144 por caminho/Master, 128 MiB de filas
de arestas. Insert com latência inválida é desativado com diagnóstico; excesso
do caminho/orçamento invalida o processamento com erro. Buffers dry de inserts,
memória interna dos plugins e mídia são adicionais ao orçamento de arestas.
A versão de sessão permanece **3**; `latencySamples` é opcional e validado.

## VST3 e diretórios

Verificados módulo real, carga/descarga repetida, bypass, parâmetros, estado
opaco, plugin ausente, suspensão, rates 44100/48000/96000, buffers variados,
editor genérico, close, scan com falha no construtor e retry/blacklist.
O scanner continua filho por módulo, com limite de 20 segundos, e prepara o
candidato para validar seus buses stereo. Detecta bundles e DLLs `.vst3`.
São aceitos efeitos stereo; instrumentos/MIDI/sidechains ficam para depois.
Buffers externos são segmentados: o tamanho máximo preparado do plugin é
256; mudar o buffer do dispositivo não exige repassar esse máximo ao plugin.
As chamadas reais variam entre 1 e 256 amostras conforme bloco e Read.

Verificação local dos diretórios em 2026-10-14:

| Diretório | Resultado |
|---|---|
| `C:\Program Files\Common Files\VST3` | Não existe; zero módulos |
| `C:\Users\eduar\AppData\Local\Programs\Common\VST3` | Não existe; zero módulos |
| `C:\Program Files (x86)\Common Files\VST3` | Não existe; zero módulos |

**Pasta VST3** adiciona diretórios salvos em `.nova-plugin-list.xml`.
**Scan VST3** soma esses diretórios aos padrões JUCE/Windows; **Plugins** mostra
os caminhos. Cache usa arquivo temporário e preserva os caminhos entre sessões.
O teste automatizado salva/reabre um diretório personalizado. Não há nome ou
fabricante especial exigido pelo scanner; a compatibilidade real com terceiros
permanece pendente porque nenhum módulo externo estava disponível.

## Validação automatizada

Ninja x64 Debug e Release compilados; CTest **7/7 em ambos**.
AudioAndSessions: 97; MixerRouting: 36; DSPVST3AutomationSessions: 44;
PluginDelayCompensation: 116; VST3ConcurrencyAndStability: 40 verificações.
Total **333** por build, além dos smoke tests C++20/SDK e JUCE.
Execução final: Debug 18,20 s; Release 14,20 s; nenhuma falha funcional.

PDC cobre impulsos conhecidos com atrasos reais 0, 1, 17, 255, 256 e 511,
faixas paralelas, buses encadeados, Master, sends pré/pós com amplitudes
esperadas, bypass, suspensão, Read com tempo compensado, seek, limite de
memória/caminho, ciclos, placeholder, sessão v3 com estado opaco e impulso no
último sample. Particionamentos 1/17/64/257/480/1024 produzem o mesmo impulso.

O teste de concorrência induz lifecycle VST3 de 150 ms em paralelo ao process:
render retorna silencioso em menos de 100 ms, sem esperar os 150 ms, e o
contador aumenta. Também troca/destrói inserts, roteia, faz seek e captura
sessão enquanto outro thread renderiza; verifica finitude e encerra a sessão.
12 ciclos testam recuperar estado e reprepare em sample rates alternados.
A decisão de aprovação funcional não certifica ausência de acesso a memória
inválida em trajetos não exercitados; não há um sanitizer de races nesta execução.
São testes de concorrência possíveis no MSVC; ThreadSanitizer não foi executado.

A carga agendada usa 32 faixas, 3 Aux, 10 VST3 reais, cinco DSPs, volume/pan e
parâmetros Read. Altera parâmetros/bypass/pré-pós a cada poucos segundos,
rates a cada 5 s, salva/reabre a cada 7 s e abre/fecha editor a cada 11 s.
A versão final também altera a latência do VST3 durante reprodução e conta
recalculos. CPU (tempo de processo equivalente a um núcleo) e memória privada
são amostradas a cada segundo; duração/prazos do render também são registrados.

| Execução final curta | Callbacks | CPU equivalente a 1 núcleo | Maior render | Violações de prazo | Alocações observadas |
|---|---:|---:|---:|---:|---:|
| Debug, 10.05 s | 1178 | 84.44% | 158.215 ms | 152 | 0 |
| Release, 10.07 s | 1189 | 13.34% | 5.357 ms | 0 | 0 |

Todas as execuções curtas tiveram zero erros de controle, samples finitos e
recalculo dinâmico de latência observado. A medição preliminar de 300,05 s,
antes da última correção da fixture e sem variação dinâmica de latência,
processou 35376 callbacks/13817539 samples, CPU 17,99%, máximo 18,070 ms e
8 violações de prazo; zero alocações observadas/erros de controle.
Memória privada: início 82,30 MiB, final 98,78 MiB, pico amostrado 99,49 MiB.
Artefato: `build/stability-phase35-release-300s/stability-result.json`.

A versão final completou **300.05 s**, com **35373** callbacks e
**13815971** samples; CPU **15.23%** de um núcleo,
render máximo **29.327 ms**, **7 violações de prazo**.
Zero alocações C++ observadas, zero erros de controle e samples finitos;
**10 recalculos dinâmicos** de latência executados.
Memória privada: início 82.86 MiB,
final 99.69 MiB, pico amostrado 101.27 MiB.
Há crescimento de memória neste intervalo; o teste não comprova ausência de
vazamentos. Caches/edição de estado exigem medição de horas para qualificá-lo.
A estabilidade funcional passou; os prazos observados impedem declarar carga
máxima/hard real-time certificada. Esse ensaio não usou hardware de áudio.

Artefato: `build/stability-phase35-final-release-300s/stability-result.json`.
Executável/módulo foram copiados após a validação para preservar a revisão
medida durante o desenvolvimento das próximas fases. A estabilização funcional
da Fase 3.5 foi concluída com esses limites documentados.


Os testes detectam falhas funcionais, amostras inválidas e alocações observadas;
exceder o prazo é registrado separadamente, sem transformar estabilidade
funcional em aprovação de latência de hardware. Debug é muito mais lento e
apresentou violações de prazo. Carga/compilação/Windows podem influenciar a
medição. CPU inclui interface, operações de sessão e plugin; memória inclui
caches e alocadores. Uma janela curta não comprova ausência de vazamentos.

O probe Debug intercepta C++ new e CRT Debug na thread de render; Release
intercepta C++ new. malloc/allocadores privados de DLLs não instrumentados
podem escapar. A fixture utiliza também locks internos do cliente JUCE, que
não são os locks eliminados do host. Zero alocações observadas não certifica
hard real-time, ausência de locks em plugins ou plugins arbitrários.

### Reprovação encontrada e correção

O primeiro teste de latência dinâmica observou duas alocações, embora o host
já estivesse corrigido. A fixture compilava seu próprio ComponentRestarter
JUCE sem a macro e notificava latência via AsyncUpdater no render. Habilitada
a fila atômica/Timer também no módulo original próprio e repetida a validação.
Isso não seria uma correção aplicável a DLLs externas fechadas.
O teste de suspensão também expôs dry sem atraso e motivou o fallback corrigido.
A revisão de mudança de rate encontrou reinterpretação do contador de samples
do relógio Read; o relógio agora conserva seus segundos acumulados, com teste
numérico de volume na transição 48000 → 96000 Hz.
A primeira preparação do teste PDC precisou usar parâmetros reais e estado
opaco JUCE; bytes internos do plugin não substituem o formato de estado do host.

## Validação de janela e hardware executada

Debug e Release: `passed: true`, todos os critérios booleanos verdadeiros
em `build/ui-validation-phase35-{debug,release}/ui-result.json`.
Dispositivo: `Alto-falantes (Realtek(R) Audio) | 48000 Hz | 480 samples`.
Snapshot MIX final Release inspecionado: controles/faders visíveis em 1280×800,
status mostra PDC/contador de contenção e dispositivo.

Esse teste automático verifica callback de saída Realtek não nulo, Play/Pause,
Stop/seek/loop, gestos/layout, bindings Edit/Mix, DSP/send/Read/Master e sessão.
Não foi executada audição física, medição externa de roundtrip, ASIO ou carga
prolongada com dispositivo real e VST3 de terceiros. O teste prolongado acima
é um render agendado sem dispositivo, com processamento VST3 real.

## Roteiro manual pendente

1. Com plugins já autorizados pelo usuário, adicionar pasta, scan, carregar,
   parametrizar, bypass, abrir/fechar editor e salvar/reabrir. Registrar módulo,
   fabricante/versão, rate, buffer, latência reportada e resultado por plugin.
2. Comparar impulsos paralelos gravados/roteados com plugin de latência conhecida,
   inclusive sends pré/pós e buses; conferir alinhamento e transição de latência.
3. Executar 1..8 horas com hardware real, buffers 64/128/256/512/1024 e rates
   disponíveis. Registrar xruns, CPU, memória, áudio audível, dispositivo e driver.
4. Exercitar troca de dispositivo/rate, suspensão e fechamento de sessão/editor;
   repetir com UI nativa de cada plugin e autorizações/licenças normais.
5. Testar scan de módulo externo com timeout/crash e recuperação. Exit 86 e retry
   próprios passaram; timeout real de 20 s e access violation externo não foram
   executados. Não alterar módulos instalados para induzir falha.

Scan é isolado; load/process/editor continuam no processo principal. Não foi
adicionado bridge de áudio em outro processo: exigiria protocolo, transferência
e buffers de áudio, editor e recuperação de estado, com nova latência/custos.
Plugins que travam indefinidamente ou causam access violation podem travar ou
encerrar o aplicativo; a thread de controle também pode esperar um process
externo travado. Exceções C++ são tratadas, falhas nativas arbitrárias não.
Nenhuma segurança irrestrita de plugin externo é declarada.

## Executáveis, evidências e reprodução

- Release: `build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe`.
- Debug: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe`.
- Scanner deve acompanhar o aplicativo como `NovaPluginScanner.exe`.
- CTest: `build/{ninja-debug,ninja-release}/Testing/Temporary/LastTest.log`.
- Carga curta: `build/{ninja-debug,ninja-release}/stability/stability-result.json`.
- UI: `build/ui-validation-phase35-{debug,release}/ui-result.json` e snapshots.
- Comandos Debug/Release e carga de 300 s: README.

Builds locais; sem instalação de plugins ou push remoto. Relatórios descrevem
os testes realmente executados e os pendentes separadamente.

## Continuidade validada até 0.6.0

Fases 4, 5 e 6 implementadas depois da estabilização crítica: MIDI/VST3i,
captura WAV com worker e edição/render offline. Sessões agora v6, mantendo leitura
1–5. Debug/Release 12/12 CTest, 503 verificações + dois smoke tests. Fades/preview/
captura tiveram zero C++ new observado no callback em seus testes; lifecycle/
PDC/concorrência existentes continuam passando. Ver phase4/5/6-report.md.

Novo stress curto Release final (10,020 s): 1180 callbacks, 1 deadline overrun,
max 6,944 ms, CPU 12,63% de um core, pico privado 88.354.816 bytes, zero C++ new
observado. Evidência em build/ninja-release/stability/stability-result.json.
A execução anterior curta teve zero overruns. Não é substituição do ensaio
isolado de 300 s, que registrou 7 violações de prazo; hard real-time continua sem
certificação. Gravação física multicanal, MIDI físico e terceiros continuam pendentes.
Nenhum módulo externo foi instalado nem feito push remoto.

## Retomada e regressão — 2026-10-15

Auditoria inicial: workspace limpo, HEAD `ccf7ec3` igual ao hash da main consultado
por `git ls-remote`. Compilação/CTest anteriores às edições passaram nas duas
configurações. Detalhes: [development-status.md](development-status.md).

O ciclo corrigiu recuperação de takes e falhas/encerramento de bounce nas
Fases 5/6. O caminho do callback, o contrato de MutationGuard, os buffers/filas,
os patches JUCE e a PDC foram preservados. A limpeza do engine offline ocorre
no worker depois do retorno de render. Transições síncronas de latência e
exceção em fixture interna agora impedem a publicação de bounce incorreto.
Isso não adiciona isolamento de falhas nativas nem certifica plugins externos.

Builds Debug/Release aprovados; CTest final **12/12 em ambos**, Debug **36,78 s**,
Release **20,43 s**. Total: **513 verificações + dois smoke tests** por build.
Inclui regressões de PDC/lifecycle/concorrência e probes existentes do callback.
Persistem as pendências de horas com hardware, roundtrip, terceiros e ausência
de certificação hard real-time; uma nova execução curta não elimina os limites
históricos do teste de 300 s nem comprova ausência de vazamentos.

Regressão de interface Edit/Mix e MIDI Debug/Release: callback Realtek não nulo,
48000 Hz/480 samples; Play/Pause/Stop/loop, DSP/sends/Read/Master, Piano Roll e
sessões aprovados. Artefatos em `build/resume-20261015-ui-*`.
Em 2026-10-15, os três diretórios padrão VST3 da tabela histórica continuam
ausentes (`Test-Path=false`); nenhum plugin de terceiros foi instalado ou validado.

### Novo ensaio prolongado Release — 2026-10-15

Código validado no commit `a8df20d`; executado depois dos builds, CTest e oito
processos de UI, sem outras compilações/testes concorrentes deste ciclo.
Render agendado, sem dispositivo de áudio, 32 faixas/3 buses/10 VST3 próprios,
cinco DSPs, Read e mudanças de rate/buffer/latência/sessão/editor.

| Métrica | Resultado observado |
|---|---:|
| Duração | 300,081 s |
| Callbacks / samples | 35380 / 13819428 |
| CPU equivalente a um núcleo | 21,04% |
| Render máximo | 19,082 ms |
| Violações de prazo | **9** |
| Alocações C++ observadas no render | 0 |
| Erros de controle / silêncios por contenção VST | 0 / 0 |
| Recalculos dinâmicos de latência | 14 |
| Memória privada inicial / final | 89309184 / 109375488 bytes |
| Pico privado amostrado | 109404160 bytes |

`passed=true`, samples finitos e 40 verificações de estabilidade aprovadas.
As nove violações de prazo e o crescimento de memória permanecem evidência
de limitações: não se declara certificação hard real-time nem ausência de
vazamentos. O ensaio histórico teve sete violações; são execuções em momentos
distintos, sem demonstração causal de melhora de desempenho. Este ciclo alterou
recuperação/bounce fora do callback, preservando o processamento de reprodução.

Resultado completo, incluindo amostras de CPU/memória por segundo:
`build/stability-resume-20261015-release-300s/stability-result.json`.
Reprodução: comando de 300 s do README, usando essa pasta como primeiro argumento.
Probe Release observa C++ new, sem instrumentar todo malloc/alocador estrangeiro.
Hardware e terceiros não foram usados neste ensaio; investigação de prazos,
crescimento de memória e execução de horas permanecem pendentes.

## Investigação solicitada na Fase 7 — 2026-10-15

Histórico preservado; instrumentação e comparação detalhada em
[phase7-report.md](phase7-report.md). Não há dados individuais dos nove atrasos
históricos, portanto sua atribuição exata não é possível retrospectivamente.
Novo ensaio instrumentado de 300 s contou quatro atrasos e separou gate,
manutenção, wall time, ciclos e bytes C++ vivos da memória privada do processo.
A limpeza da sessão liberou 17682432 bytes privados; o crescimento de objetos
medidos acompanha o histórico das medições. Isso não certifica DLLs/alocadores
não instrumentados. Cache de coeficientes EQ e avanço PDC sem divisão passam
nos testes Debug/Release. Comparação após correções concluída: 300,063 s,
35375 callbacks, dois atrasos, máximo9,458ms, P50/P95/P99 908/4162/5554us;
CPU18,44% de um núcleo, zero new observado/controle/contenção VST.
Private94027776→111779840, após clear95019008 bytes; C++ vivos nas amostras
0/300:8423743/8468489. A redução observada é modesta e não demonstra eliminação
de preempção ou leak de todos os alocadores. Os limites de prazo não mudaram.

Callback real Realtek48k/480 instrumentado opcionalmente, desligado por padrão:
UI final0.7 Debug Edit/Mix/MIDI/Master registrou481/188/136 callbacks,
max4,5811/1,7502/3,8105ms; Release376/142/84, max0,2677/0,1469/0,2480ms,
zero overrun nas seis cenas curtas. Essas cenas não equivalem à carga sintética
de32 faixas. Testes completos finais15/15 em ambas configurações,593checks+2smoke.
O medidor novo não aloca no render nos probes; histórico/FFT ficam no controle.
Dados e ensaio integrado distinto da Fase7 em [phase7-report.md](phase7-report.md).
