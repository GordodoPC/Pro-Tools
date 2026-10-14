# Diretrizes NOVA DAW

## Escopo autorizado

Implementar no projeto existente o escopo de docs/roadmap-phases-4-6.md.
Concluir a estabilização crítica da Fase 3.5 antes de MIDI; executar 4 → 5 → 6.
Compatibilidade de plugins de terceiros indisponíveis não bloqueia essas fases.
Preservar Edit/Mix, mixer, buses/sends, DSP, automação, PDC e sessões anteriores.

## Implementação

- C++20/JUCE/CMake Windows, dependência JUCE fixada em dependencies.json.
- Ciclos pequenos e compiláveis; não recriar o projeto nem copiar código proprietário.
- Não instalar dependências/plugins, alterar configuração global, usar comandos
  destrutivos ou fazer push remoto sem autorização explícita do usuário.
- Não desativar testes; commits locais são autorizados após validação.
- Render não faz disco, alocações deliberadas, lifecycle ou esperas bloqueantes.
  Prealocar filas/buffers, limitar eventos e mover escrita/processamento offline
  para fora do callback. Documentar limites e tratamento de overflow.
- MutationGuard tem um proprietário de controle; edições diretas de regiões,
  inserts, grafo e automação exigem guard. Não destruir recursos em uso.
- Adaptações JUCE estão em patches/juce-9.0.4/realtime.json com hashes verificados
  por cmake/JuceRealtimePatch.cmake; alterações futuras devem ser reproduzíveis.
- Monitoramento de entrada permanece desligado por padrão; gravação e edições
  preservam fontes e arquivos capturados. Sessões devem migrar versões anteriores.

## Validação e comunicação

- Compilar Debug e Release; executar CTest apropriado em ambos e corrigir regressões.
- MIDI exige áudio real de instrumento; fades exigem sinal processado; gravação
  simulada não equivale a hardware; scanner não certifica plugins arbitrários.
- Separar evidência automática, simulação, hardware e roteiro manual pendente.
- Não apresentar parcial como concluído. Registrar limitações/artefatos e bloqueios
  externos; continuar funcionalidades independentes sem esperar recursos opcionais.
- Atualizar docs/phase3-stability-report.md e relatórios phase4/5/6 conforme o trabalho,
  docs/nightly-progress.md a cada ciclo e README com execução/reprodução.
- Informar caminho do executável validado, testes, commits locais e pendências.
