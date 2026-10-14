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
estabilização; o escopo das Fases 4/5/6 foi solicitado separadamente.

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

RESULTADOS_FINAIS

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

MEDICAO_FINAL

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

UI_FINAL

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
