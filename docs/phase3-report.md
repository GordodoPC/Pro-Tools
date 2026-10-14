# NOVA DAW — Fase 3

Data: 2026-10-14. Referência: `e8312597a50496898a5075ab5952187ba3815e91`.

## Auditoria antes das alterações

Workspace limpo; build Ninja x64 Debug aprovado; CTest 3/3 aprovado
(97 verificações em AudioAndSessions). Inspecionados AudioEngine, Session,
EditModel, Preferences, MainComponent, EditorComponents, App, CMake e testes.

O motor existente soma regiões de 64 faixas, reamostra linearmente WAVs em
memória e oferece volume/pan/mute/solo/master e saídas stereo/L/R. Timeline,
gestos, histórico e loop já funcionam. MIX reutiliza os controles de Edit,
mas inserts/sends são apenas rótulos pendentes. Não existem buses, VST3, DSP
de efeitos ou automação. Formatos de sessão 1/2 são validados antes de substituir
o estado e escritos por arquivo temporário. WAVs permanecem externos.

O callback não lê disco nem usa mutexes; estruturas são alteradas com callback
desanexado. Essa convenção será preservada para impedir ponteiros invalidados.
Controles são atômicos, mas ganho/pan não têm smoothing. A pausa estrutural
é segura, embora possa causar uma interrupção audível. Importação é síncrona.

## Implementação e validação

Em andamento. A Fase 3 ainda não está declarada concluída.
