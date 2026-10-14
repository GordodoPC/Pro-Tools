# NOVA DAW 0.2.0 — continuação da Fase 2

Data: 2026-10-14. Ambiente e validação histórica da Fase 1:
[environment-report.md](environment-report.md).

A continuação concluiu a integração da interface com o modelo de edição que
já estava no projeto. As vistas Edit/Mix agora compartilham o motor, os controles
e a seleção; edições estruturais executam com o callback de hardware desanexado.

## Funcionalidades entregues

- Tracks separado da timeline, rolagem vertical vinculada e régua musical/segundos.
- Lista Clips, divisores de largura, zoom horizontal/vertical e alturas por faixa.
- Smart/Grabber/Trim/Selector/Split; mover entre faixas, aparar/recuperar áudio
  disponível, dividir, copiar/recortar/colar, excluir e Undo/Redo de até 64 operações.
- Nome/cor/altura de faixa, saída Master stereo/L/R e controles Edit/Mix vinculados.
- BPM, compasso, Snap/Grid, seleção de intervalo e loop com retorno dentro do callback.
- Sessões versão 2 com regiões, mixagem, apresentação de faixas, tempo e loop;
  leitura compatível com versão 1 e rejeição transacional de dados inválidos.
- Preferências locais de largura/zoom/vista/Snap, com gravação por arquivo temporário.
- Limites de 24 horas/128 regiões por faixa aplicados à edição e à abertura de sessões.
- Compilação e conversão explícita UTF-8 nos rótulos da interface.

## Validação

| Verificação | Resultado |
|---|---|
| Build Ninja x64 Debug | PASS |
| Build MSBuild x64 Debug | PASS |
| CTest Ninja | 3/3 PASS |
| CTest MSBuild | 3/3 PASS |
| AudioAndSessions | 97 verificações PASS por gerador |
| Janela nativa Ninja | PASS, retorno 0 |
| Janela nativa MSBuild | PASS, retorno 0 |

Os testes numéricos cobrem reprodução/mixagem, sample rates distintos, regiões
movidas/aparadas/divididas, preservação da fase das amostras, compartilhamento de
mídia, Undo/Redo, integridade dos WAV originais, persistência, dados inválidos,
limite de 24 horas, loop dentro de um callback, roteamento e preferências.

O modo `--validate-ui` exercita handlers de mouse e atalhos, bindings de controles,
layout em 1000×650, rolagem vinculada, salvamento/reabertura da sessão editada,
Play/Pause/retomada/Stop e loop ativo no hardware. Todos os campos dos resultados
JSON passaram. Dispositivo: `Alto-falantes (Realtek(R) Audio) | 48000 Hz | 480 samples`.
Snapshots Edit/Mix foram inspecionados visualmente.

Evidências locais:

- `.setup/phase2-ui-build.log`, `.setup/phase2-build-msvc.log`.
- `.setup/phase2-tests-ninja.log`, `.setup/phase2-tests-msvc.log`.
- `build/{ninja-debug,msvc}/Testing/Temporary/LastTest.log`.
- `build/ui-validation-phase2-final/{ui-result.json,edit.png,mix.png,edited-ui.novadaw}`.
- `build/ui-validation-phase2-msvc/{ui-result.json,edit.png,mix.png,edited-ui.novadaw}`.

Executáveis: `build/ninja-debug/NovaDAW_artefacts/Debug/NOVA DAW.exe` e
`build/msvc/NovaDAW_artefacts/Debug/NOVA DAW.exe`.

## Limites

Esta entrega é um build Debug local. Gravação, MIDI, VST3, ASIO, buses, sends,
automação e exportação seguem pendentes; REC está desabilitado e Inserts/Sends
são indicados como pendentes. Não há streaming, fades automáticos, smoothing
de ganho/pan ou reamostragem de alta qualidade. Regiões sobrepostas somam sinais.
As referências de mídia são externas; os WAV não são coletados ou copiados automaticamente.

Undo/Redo cobre regiões, nomes, cores e alturas; importar/abrir/nova sessão
reinicia o histórico. Volume/pan/mute/solo/master/tempo não entram nesse histórico.
Importação permanece síncrona, com até 64 faixas e 512 MiB de áudio decodificado.

A validação observou amostras não nulas entregues ao dispositivo, sem medição
externa de audibilidade ou latência. Diálogos nativos não foram todos exercitados
manualmente. Ainda faltam testes de longa duração/carga máxima e empacotamento.
