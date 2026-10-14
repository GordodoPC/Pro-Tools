# NOVA DAW — Escopo oficial das Fases 4, 5 e 6

Registrado em 2026-10-14 a partir da autorização explícita do usuário.
Repositório: https://github.com/GordodoPC/Pro-Tools.
C++20, JUCE e CMake, Windows; continuar no workspace existente.

## Ordem, regras e validação

Finalizar primeiro as pendências críticas da Fase 3/3.5. Executar Fase 4,
depois 5, depois 6, preservando funcionalidades anteriores. Desenvolver em
ciclos pequenos e compiláveis; analisar os módulos atuais, compilar Debug e
Release, executar CTest, corrigir regressões, atualizar documentação, fazer
commits locais das alterações validadas e registrar docs/nightly-progress.md.

Não recriar o projeto, modificar configurações globais ou instalar dependências
sem autorização, usar comandos destrutivos, desativar testes, executar push
remoto sem autorização, copiar código proprietário ou declarar funcionalidades
incompletas concluídas. Recursos opcionais/dependências externas indisponíveis
não interrompem funcionalidades independentes; documentar bloqueios.
Compatibilidade VST3 de terceiros pendente não bloqueia estas fases. Não
instalar plugins externos sem autorização. Separar testes automatizados,
simulados, de hardware e manuais; não declarar testes não executados aprovados.

## Fase 4 — MIDI e instrumentos virtuais

Objetivo: gravar, editar e reproduzir MIDI e instrumentos VST3.

### Motor MIDI

- Reprodução sincronizada com áudio e BPM; ticks musicais ↔ samples.
- Note On/Off, velocity, canais 1..16, CC, Program Change e Pitch Bend.
- Eventos no offset correto dentro dos buffers de áudio.

### Piano Roll

- Teclado vertical, grade musical, inserir/mover/redimensionar/excluir notas.
- Seleção múltipla, copiar/colar, quantizar e editar velocity.
- Zoom horizontal/vertical, Snap to Grid, Undo/Redo.

### MIDI Tracks

- Criar faixas, clips MIDI, edição não destrutiva e persistência.
- Gravar dispositivos MIDI, selecionar entrada e instrumento.
- Mute/Solo, volume e pan.

### Instrumentos VST3

- Identificar VST3i, instanciar, enviar MIDI e processar áudio gerado.
- Interface, presets, persistência de estado e instrumentos ausentes.
- Testar notas produzindo áudio real, além da descoberta do módulo.

### Recursos musicais

- BPM, metrônomo, compasso, régua Bars/Beats, quantização e loop MIDI.
- Importar/exportar Standard MIDI File quando viável.

Entregáveis: motor MIDI, Piano Roll interativo, tracks, gravação MIDI,
instrumentos VST3, metrônomo, persistência, testes automatizados e
`docs/phase4-report.md`.

Aceitação: criar uma faixa MIDI, inserir notas no Piano Roll, reproduzir por
instrumento virtual, alterar tempo, salvar/reabrir sem perder os dados.

## Fase 5 — Gravação profissional

Objetivo: gravação multipista confiável, entradas, monitoramento, punch e takes.

### Gravação multipista

- Várias entradas simultâneas, mono/stereo, dispositivo e canais selecionáveis.
- Record Arm por faixa, transporte Record, WAV e alinhamento à timeline.

### Monitoramento

- Input Monitoring por faixa, indicadores/medidores em tempo real.
- Ganho quando suportado, buffer e mudanças de dispositivo.
- Não habilitar monitoramento automaticamente em configurações perigosas;
  evitar realimentação acústica.

### Punch e loop

- Punch-in/out, pre-roll/post-roll, loop recording.
- Criar/gerenciar takes e selecionar take ativo.

### Segurança

- Gravação não destrutiva, fontes preservadas, buffers adequados.
- Escrita de disco fora do callback; detectar interrupções/falhas de escrita.
- Proteger arquivos gravados e recuperar quando tecnicamente possível.

### Sessões

- Persistir entradas, configuração de gravação/monitoramento, arquivos, takes
  e regiões; manter compatibilidade com sessões anteriores.

Entregáveis: multipista, entradas, monitoramento, punch, loop/takes, WAV válidos,
testes automatizados e `docs/phase5-report.md`.

Aceitação: armar duas faixas com entradas diferentes, gravar simultaneamente,
parar, reproduzir, salvar/reabrir conservando gravações. Se hardware multicanal
estiver indisponível, usar entradas simuladas e registrar validação real pendente.

## Fase 6 — Edição avançada

Objetivo: ferramentas avançadas preservando arquivos originais.

### Clips

- Split, Trim, Slip, Ripple, Move, Copy/Paste/Delete, duplicação.
- Seleção múltipla, Snap/Grid e Undo/Redo.

### Fades

- Fade In/Out e Crossfade; curvas lineares, exponenciais e equal-power.
- Editar curvas visualmente; curvas devem afetar o áudio real.

### Time stretching

- Alterar duração com pitch aproximadamente preservado, offline.
- Preview quando viável, cache, fonte original preservada.
- Implementação própria ou biblioteca de licença compatível.

### Pitch shifting

- Semitons e cents quando viável; duração aproximadamente preservada.
- Offline, reversão ao original; documentar artefatos/limites do algoritmo.

### Comping

- Organizar takes, selecionar trechos, combinar, crossfades e persistência.
- Edição não destrutiva.

### Consolidação

- Consolidate Clips, Bounce Selection e render offline em novos arquivos.
- Fontes preservadas, gerenciamento de temporários.

Entregáveis: edição avançada, crossfades, stretching, pitch, comping,
consolidação/render offline, Undo/Redo, testes e `docs/phase6-report.md`.

Aceitação: importar, dividir/mover, crossfades, alterar duração/pitch, selecionar
takes e consolidar, preservando os arquivos originais.

## Entrega final

Informar estado de cada fase, recursos concluídos/parciais/pendentes, testes,
commits locais, caminho do executável e pendências de intervenção humana.
