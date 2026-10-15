# NOVA DAW Alpha 0.7 — Public Test

**Candidato para revisão local. Ainda não autorizado para publicação.**
A licença do código da NOVA DAW está em decisão pelo autor. O ensaio integrado
da Fase 7 ainda registra violações de prazo. Consulte RELEASE-STATUS.json e o
relatório de validação; Alpha não significa que falhas críticas sejam aceitáveis.

## Instalar e remover

Windows 10/11 x64, saída de áudio Windows e espaço livre para aplicativo e sessões.
Execute NOVA-DAW-Alpha-0.7-Public-Test-x64-Setup.exe. Instalação por usuário em
`%LOCALAPPDATA%\Programs\NOVA DAW Alpha 0.7`; não instala drivers, serviços ou
plugins globais. Runtime MSVC está ao lado do programa, sem download no destino.
Menu Iniciar e área de trabalho recebem atalhos. O pacote ainda não possui
assinatura Authenticode do autor; reputação/SmartScreen precisam de revisão.

Remova em Configurações → Aplicativos ou pelo atalho Desinstalar NOVA DAW Alpha 0.7.
Somente arquivos pertencentes ao pacote são removidos. Fontes, sessões, gravações,
preferências e diagnósticos permanecem. Feche o aplicativo e o scanner antes de
reparar/desinstalar. Executar o mesmo Setup novamente repara os arquivos instalados.
Cancelamento, permissões e rollback ainda devem ser conferidos em máquina limpa.

## Primeiro teste

1. Use cópias de WAVs e sessões; mantenha backup externo dos originais.
2. Abra Audio e confira saída/sample rate/buffer. Comece com buffer de480/512
   samples e volume baixo. Importar WAV, Play/Stop, Edit/Mix e salvar/reabrir.
3. Confira automação Read/Write/Touch/Latch, curvas/Undo, grupos VCA/Solo Safe e
   sidechain no NOVA Compressor/Gate. Playback e IO permanecem stereo.
4. Em Master → LUFS / Export, meça pelo menos400ms; exporte mix/stems para NOVOS
   arquivos, compare formatos/rates/áudio e confirme que os originais permanecem.
5. Para MIDI sem instrumento externo, adicione a pasta Plugins da instalação
   em Pasta VST3, faça Scan e escolha NOVA Test Synth no Piano Roll. É instrumento
   original de teste; nenhum plugin de terceiros acompanha o pacote.
6. Gravação física/MIDI e plugins externos exigem testes específicos. Monitoramento
   de entrada inicia desligado; habilite conscientemente com fones para evitar feedback.

Dados ficam em `%LOCALAPPDATA%\NOVA DAW\Alpha 0.7`. Preferências/lista de plugins
ficam ali; sessões sem nome usam Workspace para Recordings/Rendered/cache.
Salve sua sessão numa pasta escolhida e preserve todas as fontes/pastas referidas.
A aplicação não copia automaticamente os WAVs importados nem migra dados do workspace
de desenvolvimento. Sessões v7 leem v1–6; use Salvar como ao testar projetos antigos.

## Logs e falhas

Ajuda → Abrir logs e relatórios ou Gerar relatório de diagnóstico.
Logs JSONL limitados, relatórios JSON de operação e texto de exceção Windows ficam
na subpasta Diagnostics. Nenhum envio automático, captura de áudio ou conteúdo
integral da sessão. Caminhos de arquivos, dispositivo, OS e erros podem aparecer:
revise/remova dados pessoais antes de compartilhar manualmente um relatório.
O relatório fatal é mínimo; não é dump de memória nem garantia de recuperação.

Ao relatar falha, inclua versão/hash do pacote, Windows/interface/driver, rate,
buffer, passos mínimos, resultado esperado/observado e os logs relevantes. Em
travamento, não sobrescreva a sessão anterior; preserve Recordings e recovery.json.

## Limites conhecidos e aceite pendente

Prazos de áudio ainda não aprovados na carga máxima; não utilizar para trabalho
crítico sem backup. Captura de gestos tem resolução de30Hz. Sidechain auxiliar
VST3, surround físico e captura de gestos em editor VST3 externo não estão ativos.
True Peak é estimativa FIR4x; medição não tem certificação completa ITU/EBU.
Normalização não inclui limiter; o teto pode impedir atingir o alvo LUFS.
Stems através de buses não lineares podem não somar ao mix; não há caudas automáticas.

Em outro computador ainda testar: Windows10/11 x64 limpo sem Visual Studio/VC runtime,
usuário sem administrador, caminhos com acentos/OneDrive, instalação/reparo/remoção,
atalhos, nenhuma dependência do computador original, sessões antigas, permissão
negada/disco cheio, áudio de horas/interface real, entradas/MIDI/hotplug, plugins
externos, áudio exportado em medidor independente e SmartScreen/antivírus.
