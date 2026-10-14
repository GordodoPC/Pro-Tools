# REA — verificação de conexão, 2026-10-14

Pergunta: o MCP do REA está conectado e quais limites afetam investigações nativas?

Observação direta: `binary_session({})` respondeu pelo servidor `rea` 6.5.0,
iniciado em `2026-10-14T15:39:20.514Z`. O catálogo retornou 207 ferramentas MCP,
153 comandos CLI e 6 prompts. Nenhum alvo estava aberto. O cliente conectado
é Codex; não há confirmação de uma integração ativa no Claude Code nesta sessão.

O provedor Ghidra retornou `unavailable`: `GHIDRA_INSTALL_DIR` não configurado.
IDA retornou `unavailable`: registro upstream MCP não configurado. Hopper é
incompatível com win32. Essas limitações não afetam a compilação da NOVA DAW.
Nenhum desses provedores foi instalado ou configurado durante esta fase.

Fontes consultadas:
- https://github.com/morluto/rea/blob/main/README.md
- https://github.com/morluto/rea/blob/main/docs/windows-ghidra-p0.md

Segundo a documentação atual, Ghidra no Windows é experimental e requer
Windows 10+ x64, alvos locais NTFS, Ghidra 12.1.x e JDK completo 64 bits compatível.
Node.js 24.22.0 foi observado localmente e satisfaz a faixa indicada para REA.
Isso não demonstra a existência de Ghidra ou de um JDK compatível neste computador.

Não houve inspeção de Pro Tools, de código proprietário ou de comportamento
runtime de terceiros. Não foram produzidas evidências de engenharia reversa.
A arquitetura e os recursos desta fase são implementações próprias baseadas
nos requisitos fornecidos e nas APIs públicas do JUCE.
