# Auditoria complementar da fork — execução, entrada e localização

> Referência de trabalho: `main` em `e137c55a0a1b88218210a7d43ad8c8a54f4ba735`. Esta é uma análise estática dos arquivos **desta fork**, com leitura dos fluxos de inicialização, destruição, entrada, localização, legendas e controladores. Não executei testes nativos nem partidas do jogo. O documento registra evidências de código e hipóteses a reproduzir, sem antecipar conclusões sobre impacto prático.

## Objetivo e delimitação

Quero uma segunda revisão aprofundada, mas **sem repetir a revisão de leitura de TIM, WAV/RIFF, STR como formato de vídeo, extração ISO9660, salvamentos, escrita de configurações, autoatualizador ou importação glTF**. Esses assuntos já estão registrados em outra investigação e não fazem parte desta solicitação.

Esta revisão prioriza comportamentos observáveis **na minha fork** e as integrações de localização, legendas e controles modernos. Não quero refatoração genérica ou reparo especulativo. Primeiro confirmem cada cenário e eliminem falsos positivos.

## P0 — Ordem de destruição da engine e do contexto gráfico

**Evidência estática:** `src/gen/main.cpp` instancia `Game game;` no escopo da função principal, mas ao sair do loop executa `platform->DestroyContext(ctx)` e `tPlatform::Destroy()` **antes** da destruição automática de `game`. O mesmo caminho executa `g_textManager->Shutdown()` após destruir o contexto. Já `Game::~Game()`, em `src/gen/game.cpp`, libera `introTexture`, tela de título, fontes, `ScreenDraw` e managers, alguns com recursos GPU e áudio.

- [ ] Confirmar com o ciclo de vida real de `tPlatform`, `p3d::context`, `Game::~Game`, `TextManager` e dos managers se recursos gráficos ou dispositivos de áudio são usados após a destruição do contexto.
- [ ] Reproduzir saída normal, saída após filme, saída durante menus, finalização com falha de inicialização e fechamento com recursos gráficos ativos. Verificar também logs após `Log::Shutdown()` e possíveis riscos para callbacks/threads.
- [ ] Se confirmado, corrigir a **ordem de shutdown** com encerramento explícito, idempotente e verificável, preservando o fluxo de inicialização e os testes existentes.

**Classificação:** P0 para investigação de ciclo de vida; impacto concreto de crash ou vazamento ainda não medido.

## P1 — Valores inválidos em bindings e conversões numéricas

**Evidência estática:**
- `src/pc/inputaction.cpp`, `ActionInput::SetDesktopBindingCode`: para `code < 0` calcula `(-code) - 1`. Para `INT_MIN`, a negação tem **overflow assinado em C++**.
- `src/extra/prompticons.h`, `PromptIcons::ResolveDesktopBindingCodePrompt`: usa a mesma expressão em códigos negativos.
- `src/pc/settings.cpp`, `ParseSettingValue`: usa `strtol` e converte a `s32` sem verificar `errno` ou todos os limites necessários.
- `src/extra/prompticons.h`, `ParseInlineDesktopBindingToken`: lê um `long` por `strtol` e converte para `s32` sem validação explícita. `ActionInput::SetKeyBindingSlot` também aceita teclas inteiras arbitrárias.

- [ ] Testar `0`, `INT_MIN`, `INT_MAX`, números fora do intervalo, texto não numérico e códigos de teclado inexistentes, inclusive via arquivo INI e marcador `<BND:...>`.
- [ ] Rejeitar ou normalizar valores inválidos **sem overflow, sem alterar a configuração válida anterior** e sem quebrar atalhos.
- [ ] Incluir testes de limites e sanitizers apropriados. Preservar a codificação atual de mouse/teclado e compatibilidade com configurações existentes.

**Classificação:** UB da negação é demonstrável na expressão, mas o caminho de uso com valor inválido precisa de teste.

## P1 — Rótulos de teclas e contrato de tamanho do buffer

**Evidência:** `src/pc/inputaction.cpp`, `KeyToLabel`, escreve `outLabel[1] = '\0'` para teclas alfanuméricas embora a guarda da função aceite `outLabelLen == 1`. O contrato atual permite buffer de um byte e não protege essa escrita.

- [ ] Criar teste unitário com `outLabelLen` igual a 0, 1, 2 e tamanho usual.
- [ ] Corrigir usando a capacidade declarada, preservando o texto existente e fallback para códigos desconhecidos.
- [ ] Revisar outros formatadores de prompts apenas quando o padrão for equivalente; evitar alterar a aparência dos ícones.

**Classificação:** escrita fora do limite sob um argumento válido de um byte; não foi observado esse tamanho em produção.

## P2 — Scroll horizontal interpretado como rolagem vertical

**Evidência:** `vendor/libp3d/pddi/gl/glrender.cpp`, `glDisplay::ScrollCallback`, encaminha `xoff` e `yoff`. Em `src/gen/main.cpp`, `OnWndProc` trata `PDDI_WND_SCROLL` com `(msg.fparam2 > 0.0) ? 1 : -1`. Um evento **horizontal apenas** (`yoff == 0`) é tratado como uma rolagem vertical para baixo quando um menu estiver ativo.

- [ ] Criar reprodução com eventos sintéticos `(xoff, yoff) = (1,0), (-1,0), (0,1), (0,-1), (0,0)`.
- [ ] Ignorar `yoff == 0`; manter sinais positivos e negativos da rolagem vertical. Confirmar comportamento com touchpad e mouse horizontal.
- [ ] Não interferir na captura e movimentação do cursor ou navegação por gamepad.

**Classificação:** correspondência direta entre produtor e consumidor do evento; reprodução no menu ainda pendente.

## P2 — Falta de recuperação explícita quando um catálogo de idioma não carrega

**Evidência:** `src/extra/customtext.cpp`, `CustomText::Init`, chama `ClearStrings()` antes de `LoadTextFile(langPath)` e não reage a uma falha de leitura. `SetLanguage` troca o idioma ativo antes de carregar e não faz rollback. Entradas ausentes podem passar para o fallback do jogo original (`xcSection::FindString`), mas **novas entradas da fork não necessariamente têm equivalente original**.

- [ ] Testar ausência, arquivo vazio e catálogo com entradas faltando, por idioma, em um ambiente de teste que não modifique os catálogos distribuídos.
- [ ] Verificar quais elementos efetivamente aparecem em branco ou em outro idioma; evitar alegar falha visual sem teste.
- [ ] Estabelecer fallback seguro para conteúdo oficial, estado consistente do idioma selecionado e logging útil, sem alterar traduções aprovadas nem o fluxo normal.
- [ ] Verificar que troca de idioma, recarga e renderização não deixam views/ponteiros de texto obsoletos.

**Classificação:** falha de carregamento não tratada na seleção de idioma; consequência visível precisa ser demonstrada.

## P2 — Legendas externas e validação em tempo de execução

**Evidência:** `src/radmovie/subtitles.cpp`, `MovieSubtitles::Parse`, aceita o arquivo caso reste ao menos um cue válido, mesmo ignorando blocos inválidos. Um cue também pode armazenar mais de duas linhas; `TextAt` concatena cues simultaneamente ativos. O validador de recursos distribuídos (`scripts/validate-localization.py`) aplica restrições mais estritas, mas não se aplica necessariamente a legendas modificadas após instalação.

- [ ] Criar testes com SRT parcialmente inválido, sobreposição de cues, mais de duas linhas, timestamps repetidos e dados mistos válidos/inválidos.
- [ ] Decidir o comportamento seguro e consistente: rejeitar, avisar ou limitar a apresentação, sem ocultar legendas oficiais válidas e sem criar clipping inesperado.
- [ ] Comparar os cinco vídeos de cada idioma e confirmar timing e estilo atual. Não refazer a análise do **decodificador STR** nem modificar tradução editorial.
- [ ] Evitar custos de alocação/desenho desnecessários por frame quando demonstravelmente relevantes, sem otimização especulativa.

**Classificação:** validação permissiva comprovável no código; impacto depende de SRT externo e timing.

## P2 — Associação de controles no caminho de fallback

**Evidência:** `vendor/libp3d/pddi/gl/glgamepadrumble_sdl.cpp`, `glGamepadRumbleUpdateActive`, utiliza `GuidEquals` para aceitar/reutilizar um binding. GUID identifica tipo/modelo, mas pode não identificar unicamente um controle físico. No caminho normal de polling SDL, a seleção ativa usa objetos controladores; verificar **somente o fallback/interoperação com GLFW** e cenários com dois dispositivos iguais.

- [ ] Reproduzir com dois controles do mesmo modelo (ou dispositivos virtuais equivalentes) e alternar o ativo, verificando onde vão vibração, LEDs e indicadores.
- [ ] Confirmar se a rota de fallback realmente é alcançável no Windows com a configuração distribuída; **descartar o item** se não houver risco aplicável.
- [ ] Quando aplicável, preferir identidade estável de instância em vez de GUID isolado e testar desconexão/reconexão.

**Classificação:** risco potencial restrito à rota de fallback; não é erro confirmado no caminho normal SDL.

## P3 — Prevenção de colisões futuras de hashes de localização

**Evidência:** `CustomText::LoadTextFile` identifica as entradas por `xcHash` e substitui silenciosamente o texto se um hash se repetir. A verificação de 212 identificadores no `english.txt` atual não encontrou colisões: **não existe colisão comprovada nesse conjunto**.

- [ ] Adicionar teste preventivo do hash efetivo dos identificadores entre todos os catálogos, incluindo tokens numéricos, caso não exista teste equivalente na revisão em curso.
- [ ] Em caso de hash duplicado entre tokens distintos, emitir diagnóstico de build claro antes de distribuir a tradução.
- [ ] Não substituir o algoritmo `xcHash` e não modificar as entradas existentes sem colisão.

**Classificação:** prevenção, não defeito atual.

## Método de execução

1. Trabalhar sobre o estado atualizado da minha fork, começando por verificar `git status`, branches e trabalhos pendentes. **Esta branch de auditoria é independente das revisões anteriores**.
2. Não duplicar análises de TIM, WAV, STR demux/buffers, ISO, saves, escrita INI, autoatualizador ou glTF; manter **apenas os tópicos inéditos descritos acima**.
3. Para cada ponto, registrar evidência, cenário reproduzível, gravidade justificada, status **confirmado/descartado/pendente** e mudança mínima proposta.
4. Corrigir apenas itens confirmados, com testes unitários, de integração e sanitizers quando apropriado. Usar Premake, Visual Studio/MSBuild, PowerShell e Python disponíveis **localmente**; evitar GitHub Actions repetitivo e consumo desnecessário de franquia.
5. Preservar recursos PT-BR, layout e estilo das legendas, DualSense/DualShock 4, HUD, mods, saves, funcionamento original e créditos existentes.
6. Não sobrescrever alterações de outra branch, não fazer force-push nem merge automático. Não publicar comentários/menções em repositórios externos.
7. Preparar relatório com observações descartadas e limitações explícitas; não declarar teste de gameplay ou hardware se não foi realmente executado.

## Critérios de aceitação

- Ciclo de vida de GPU/áudio termina sem uso de recursos destruídos.
- Valores de binding inválidos e buffers mínimos não geram UB.
- Scroll horizontal não aciona menu vertical.
- Falha na localização ou SRT externo tem resultado previsível e documentado.
- Múltiplos controles não cruzam feedback em rotas suportadas.
- Todos os testes e recursos previamente aprovados permanecem funcionais.
