# NOVA DAW — Fase 5: gravação profissional

Implementação 0.5.0 no projeto existente. Critério funcional: duas faixas armadas
com entradas simuladas distintas, captura WAV, reprodução, salvar e reabrir.
Builds Debug e Release aprovados. CTest 11/11 em ambos, 448 verificações
numéricas/comportamentais mais dois smoke tests. Hardware multicanal não foi
declarado aprovado.

## Implementação

- Faixas de áudio vazias, Record Arm independente e seleção de canais ativos do
  dispositivo em Audio; captura mono/estéreo, ganho digital, meters e monitoramento.
- Menu Gravacao abre entradas, Record/Stop, punch, pre/post-roll, loop e takes.
  Input Monitoring exige ação explícita; abertura de sessão e reinício do
  dispositivo revogam monitoramento. Preferência persistida não o habilita.
- Punch captura o intervalo [In, Out) por sample. Pre-roll inicia o transporte
  antes do In; post-roll encerra após Out. Loop cria arquivos por passagem,
  preserva anteriores e ativa o último; seleção pode ativar qualquer take.
- Sessão versão 5: inputs, ganho, arm, preferência de monitor, punch/roll/loop,
  takes, regiões e referências relativas. Leitura das versões 1–4 preservada.
- Cada captura usa pasta UUID nova. WAV PCM 24 bits e recovery.json são escritos
  por worker. Callback somente copia samples finitos para filas preparadas.
- 32 faixas armadas por captura; 256 pacotes de 256 frames por faixa, até 128
  takes/passagens. Fila cheia/interrupção/erro de disco param captura e mantêm
  o prefixo recebido. Overflow é contado; não há espera do callback por disco.
- Stop quiesce o callback antes de drenar/join e adotar assets. Mudanças de
  estrutura/seek/dispositivo finalizam captura; mudanças de rate interrompem
  para evitar arquivo com sample rate errado. Destruição ocorre fora do render.
- WAV headers e manifesto têm checkpoints a cada ~4096 frames. Recuperar captura
  lê manifestos limitados e prefixos WAV válidos; referências fora da pasta são
  recusadas. Nunca modifica fontes recuperadas nem sobrescreve capturas.

## Limites conhecidos

- Sem teste de interface física multicanal, microfone, roundtrip, cabo desconectado
  ou latência de driver/ASIO. Não foi instalado driver nem plugin externo.
- Ganho é software; controle analógico de preamp exige suporte do hardware.
- Alinhamento é por sample do stream recebido/timeline; latência de entrada do
  dispositivo não foi calibrada. PDC de reprodução continua ativa.
- Áudio gravado é escrito incrementalmente; reprodução ainda carrega assets na
  memória, limitada a 512 MiB/sessão. Capturas maiores continuam no disco, mas
  podem exigir divisão antes da reprodução. RIFF clássico limitado a <4 GiB/WAV;
  não há RF64. Sessão/timeline limitadas a 24 h.
- Recuperação tem granularidade de checkpoint; queda abrupta pode perder até
  ~4096 frames desde o último header válido. Não se garante recuperação após
  corrupção de disco, falta de energia ou falha do processo nativo de um plugin.
- Stop/join e adoção WAV podem ocupar a thread de controle; nunca o callback.
  Monitoramento é revogado também quando o dispositivo é reanexado para edição.

## Testes automatizados e manuais

RecordingTests executa entradas simuladas, escrita/decodificação WAV real,
contagem/amplitude exatas, replay, v5, monitoring opt-in, punch/roll, loop/takes,
estéreo/ganho, perda de entrada, rate, falha de writer injetada, recuperação,
stop concorrente/buffers variáveis e probe C++ new do callback.
O probe não observa todo malloc interno de terceiros e não é certificado RT.

Roteiro manual pendente: ativar duas entradas físicas em Audio; criar/armar duas
faixas; configurar entradas distintas; gravar e comparar sinais; testar mono e
estéreo, monitoring com fones, punch/loop, desconexão de hardware, buffer/device,
disco cheio real, captura prolongada e recuperação após encerramento abrupto.
Fazer backup dos WAVs antes de simular falhas reais.

Executável Release: build/ninja-release/NovaDAW_artefacts/Release/NOVA DAW.exe.

## Resultados executados (2026-10-14)

- Debug CTest 11/11, 22,73 s; Release 11/11, 15,95 s.
- RecordingTests: 38 verificações por configuração. Zero C++ new observado
  no callback; parada concorrente/buffers variáveis sem overflow no teste paced.
- Demais 410 verificações de áudio/mixer/PDC/VST3/MIDI/concorrência continuam passando.
- UI Debug e Release: passed=true, recordingPanelBindings=true,
  sessionTwoTakesRoundtrip=true; simulatedInputs=true, hardwareRecordingTested=false.
- Evidência: build/ui-validation-phase5-{debug,release}/ui-result.json,
  recording-panel.png e sessões/WAVs; LastTest.log nos dois diretórios CTest.
- Device disponível para output: Realtek 48000 Hz / 480 samples. Inputs físicos
  não ativados nesta validação; controles exibem “Ative entradas em Audio”.
- Verificação visual do snapshot Debug realizada; controles/takes legíveis.
- Sem instalação, dependências novas, configuração global ou push remoto.

Regressão em 0.6.0: RecordingTests mantém 38 verificações em Debug/Release,
com sessão atual v6 e leitura legada preservada. Recuperação fecha o worker
interrompido antes de ler manifesto/WAV; mensagens antigas de erro são limpas
pelas APIs. Hardware de captura continua não validado.

## Retomada — 2026-10-15: recuperação no limite de takes

Corrigida recuperação repetida de captura já importada em faixa com 128 takes:
a detecção de duplicatas precede a recusa por capacidade. Manifestos com entradas
repetidas de um mesmo arquivo/faixa passam a consumir um único slot; validação
continua fora do callback e caminhos externos continuam recusados.

RecordingTests agora executa **43 verificações**, incluindo o limite real de
novos takes, a preservação do arranjo/fontes e idempotência com manifesto
duplicado. O caso da faixa cheia foi reproduzido como falha antes da correção.
CTest final Debug **12/12 (36,78 s)** e Release **12/12 (20,43 s)**.
Não houve mudança de formato de sessão, dos limites ou do monitoramento opt-in.
Captura permanece simulada nesses testes, com escrita/leitura de WAVs reais;
gravação física multicanal e calibração de latência continuam pendentes.
