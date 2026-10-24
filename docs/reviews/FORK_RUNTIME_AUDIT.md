# Auditoria complementar da fork — execução, entrada e localização

> Os tópicos iniciais abaixo preservam o levantamento estático. Os resultados de reprodução, correções e limitações estão na seção **Resultados da auditoria**, ao final. Uma hipótese inicial não equivale a um defeito confirmado.

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

## Resultados da auditoria — 2026-10-24

Base examinada: `e137c55a0a1b88218210a7d43ad8c8a54f4ba735`. O documento inicial foi registrado em `beca5c7`.

| Prioridade | Item | Classificação | Resultado |
| --- | --- | --- | --- |
| P0 | Ordem de destruição de Game, fontes e contexto | CONFIRMADO | Recursos da aplicação são destruídos antes da plataforma; log encerra por último |
| P1 | Domínio numérico dos bindings e slots | CONFIRMADO | Validação antes de negar, subtrair ou converter; entradas inválidas preservam bindings válidos |
| P1 | Buffer de um byte em KeyToLabel | CONFIRMADO | Capacidade mínima produz somente terminador; demais rótulos preservados |
| P1 | Par de alocação nas células da interface | CONFIRMADO | ASan identificou `new[]` com `delete`; corrigido para `delete[]` |
| P2 | Scroll horizontal tratado como vertical | CONFIRMADO | Componente vertical zero não contribui para a navegação |
| P2 | Falha na troca de idioma | CONFIRMADO | Carga transacional, rollback e fallback inglês para entradas ausentes |
| P2 | SRT externo com linhas excessivas e rejeição silenciosa | CONFIRMADO | Entradas inválidas são contadas/diagnosticadas; apresentação simultânea limitada |
| P2 | GUID ambíguo na seleção normal SDL | DESCARTADO | O caminho normal seleciona objetos/instâncias; testes com GUIDs idênticos passaram |
| P2 | GUID ambíguo no fallback GLFW/SDL | CONFIRMADO | Correspondência única obrigatória; instância SDL usada para reutilização |
| P2 | Subsistemas SDL mantidos após shutdown | CONFIRMADO | Referências inicializadas pelo backend são encerradas; referências externas preservadas |
| P2 | Caches estáticos de texturas de prompts | CONFIRMADO | Liberação explícita e idempotente antes da destruição do contexto |
| P3 | Colisão nos identificadores atuais | DESCARTADO | 212 identificadores por idioma sem colisões; acrescentada prevenção para alterações futuras |
| QA | Touchpad físico, dois controles iguais, Steam Input e Linux | PENDENTE | Testes virtuais/Windows não certificam esses ambientes |

### Ciclo de vida

`main()` destruía o contexto antes de `Game::~Game()` e do backend de fontes. A inspeção confirmou liberações de texturas e programas OpenGL nesses destruidores. A execução original de saída de fase retornou zero; isso não comprovava validade do contexto durante a liberação.

O proprietário de `Game` agora é destruído explicitamente antes das fontes, caches e plataforma. `Game::~Game()` chama `Close()` enquanto todos os managers ainda existem, descarrega referências do mundo e só depois elimina os managers. O encerramento utiliza `AI::UnPopulate(1)` para remover o jogador das listas antes de destruí-lo. Na validação da ordem intermediária, a exclusão direta do jogador provocou um use-after-free reproduzido pelo ASan em `ccMinList::RemNode`, através de `World::Unload`; a ordem final eliminou esse acesso. O comportamento da AI durante o jogo não foi alterado.

O caminho de falha na criação do contexto libera a plataforma e encerra o log. O caminho opcional de comparação de legendas também libera seu Game não aberto, fontes, caches e input antes do contexto. Workers existentes são encerrados antes da plataforma e do log; o áudio é encerrado pelo manager existente, sem duplicar seu shutdown.

Os testes gráficos verificam que os proprietários de Game/texto/input já desapareceram e o áudio está parado enquanto o contexto GLFW ainda está corrente. Depois da plataforma, verificam os ponteiros globais nulos. Foram exercitados: saída por página de encerramento do menu, fechamento no menu, saída de fase, fechamento durante fase, fechamento durante PROLOG e falha controlada de criação da janela. Sucesso retorna zero; a falha de inicialização retorna 1. Menu, fase e cutscene também foram executados com ASan.

A inspeção SDL identificou um DualSense conectado durante a bateria final. Isso confirma execução com controle presente, sem constituir aprovação auditiva/visual de vibração ou LEDs nem teste físico de dois controles iguais.

O ASan encontrou ainda um par `new u8[16384]`/`delete` em `xcCellImage::Decode()`. A única alteração no decodificador é `delete[] expandBuf`; formatos, limites e algoritmo não foram redesenhados. Os testes finais ASan utilizam `alloc_dealloc_mismatch=1`, sem suprimir esse diagnóstico.

ASan não comprova ausência de vazamentos de GPU, e as bibliotecas/driver não estão integralmente instrumentados. Não foram injetadas falhas em todas as alocações possíveis.

### Entrada, buffers e menus

Os testes anteriores às correções falharam para `INT_MIN`, valores extremos, códigos inexistentes, perda de bindings válidos e canário de buffer. A validação agora ocorre antes da negação de códigos negativos. Zero continua desassociando; -1, -2 e -3 preservam a codificação do mouse. As faixas GLFW válidas incluem teclas internacionais, funções estendidas, teclado numérico e modificadores, rejeitando lacunas numéricas.

`ParseSettingValue()` e `<BND:...>` verificam consumo integral, `ERANGE` e limites antes da conversão. Sufixos de slots são verificados antes de subtrair um. Booleanos continuam disponíveis para configurações booleanas; bindings e slots exigem valores numéricos. Entradas inválidas não substituem bindings anteriores válidos. A escrita de configurações não foi modificada.

Rótulos foram testados com capacidades 0, 1, 2 e 16, ponteiro nulo, letras, dígitos, funções, teclas nomeadas, mouse e slots vazios. Buffers de heap com capacidade exata foram exercitados para todas as teclas aceitas sob ASan. Não foi identificado um consumidor de produção usando capacidade 1; o defeito confirmado era do contrato da função. O ASan do MSVC não detecta overflow assinado: a exclusão desse caminho é demonstrada pela validação de domínio e pelas regressões numéricas.

O teste de scroll chama o callback GLFW realmente registrado, com `(1,0)`, `(-1,0)`, `(0,0)`, `(0,1)`, `(0,-1)` e movimentos mistos fracionários, com menu ativo. Os deltas observados correspondem exclusivamente ao componente vertical. Teste físico de touchpad permanece pendente.

### Catálogos e legendas

O novo catálogo é montado em armazenamento temporário: inglês como base e o idioma selecionado como sobreposição. Somente uma carga bem-sucedida e não vazia confirma idioma, revisão e strings. Falhas preservam o catálogo anterior, inclusive seus ponteiros emprestados. Entradas ausentes usam inglês. Catálogos vazios e token final sem texto são rejeitados; hashes numéricos têm limites verificados.

Fixtures cobrem arquivo ausente, vazio, caminho ilegível, catálogo incompleto, falha ao recarregar o fallback inglês, nova tentativa após falha e shutdown/recarga. Os seis idiomas e seus round-trips de configurações passaram. Sem inglês e sem catálogo anterior no primeiro carregamento, não há como sintetizar as strings da fork: permanece o fallback original existente, com diagnóstico de falha. Falhas parciais de leitura do sistema operacional não foram injetadas separadamente; o leitor existente rejeita leituras curtas antes do commit.

O aproveitamento de cues válidos de um SRT misto é uma política de recuperação, não um erro por si só. Foram corrigidos a ausência de diagnóstico e o número ilimitado de linhas-fonte. Cues inválidos são ignorados e contados; cues com mais de duas linhas-fonte são rejeitados. UTF-8 inválido e arquivos acima do limite continuam rejeitando e limpando a pista. Sobreposições preservam ordem estável e cues completos até quatro linhas-fonte simultâneas; cues excedentes são omitidos sem cortar frases. O comportamento anterior de sobreposições normais permanece.

Testes cobrem timestamps inválidos/invertidos, blocos malformados, índice ausente, conteúdo misto, excesso de linhas, inícios repetidos, sobreposições, UTF-8, vazio, limpeza/repetição e independência do FPS. Os 30 SRTs distribuídos são aceitos sem rejeições, com 100 repetições por arquivo. Fonte Menu, RGB (215,135,0), contorno, escala 0,35, posição e alinhamento não foram alterados. Nenhum texto, tempo ou arquivo SRT foi editado.

O idioma da pista é selecionado ao abrir o filme. A reprodução bloqueante não oferece menu de idioma; a próxima abertura utiliza a seleção atual. Não foi introduzida troca de idioma no meio da cutscene. O orçamento de linhas considera linhas-fonte; textos externos excepcionalmente longos ainda podem ocupar mais linhas após quebra visual pela fonte.

### Controles e recursos estáticos

O caminho normal SDL foi testado com dois PS4 e dois PS5 virtuais de GUID idêntico: seleção por instância, vibração, LED, desconexão e reconexão com nova instância. Também foram preservados os testes de Xbox/PlayStation, indicador de jogador e controles sem vibração/LED.

O fallback é alcançável quando GLFW possui mapeamento e SDL enxerga apenas joysticks sem mapeamento. Dois dispositivos virtuais idênticos reproduziram a aceitação de uma associação ambígua. Índices GLFW/SDL são independentes; o fallback agora exige GUID único, ou exatamente um dispositivo quando não há GUID, e reutiliza a instância correspondente. Não escolhe mais arbitrariamente o primeiro dispositivo com vibração. Um joystick único conserva feedback. Dispositivos ambíguos podem continuar com comandos e seleção manual de prompts, mas sem feedback até haver mapeamento SDL confiável. Não foi criado outro sistema de entrada.

Um teste também reproduziu subsistemas SDL ainda inicializados após o shutdown do backend. O backend passa a encerrar apenas as referências de subsistemas que inicializou. Referências externas e reabertura permanecem funcionais. Testes físicos com dois controles iguais, Steam Input e diferentes ordens de enumeração continuam pendentes.

Os caches estáticos de prompts conservavam referências de texturas sem liberação no encerramento. `PromptIcons::Shutdown()` limpa ambos antes da plataforma. Um teste gráfico mantém referência própria e confirma exatamente uma liberação do cache, mesmo chamando shutdown duas vezes.

### Hashes e preservação de escopo

Não havia colisão nos 212 tokens de cada catálogo nem verificação preventiva equivalente no validador examinado. A verificação adicionada reproduz o algoritmo `xcHash`, com bytes sem sinal e interpretação dos tokens numéricos, e identifica ambos os tokens e o hash em colisão. Quatro testes Python cobrem vetores, nomes distintos, colisão sintética e alias numérico; uma asserção nativa confere o vetor de `FE_SUB` contra `xcHash()`.

Os catálogos não foram reorganizados nem editados. Recursos gráficos, fontes, traduções, HUD, salvamentos, ModLoader, updater, importação e arquivos audiovisuais permanecem fora das alterações desta auditoria. A integração com os outros trabalhos deve preservar as verificações de ordem dos catálogos já existentes, além desta verificação complementar de hashes.

### Reprodução dos testes

Na raiz do checkout, utilizando um Developer PowerShell com a ferramenta instalada:

```powershell
.\premake5.cmd --tests vs2022
msbuild build/rechan.sln /m:4 /p:Configuration=Release /p:Platform=x64
.\scripts/run-support-tests.ps1 -Python python
.\scripts/run-runtime-tests.ps1
```

Os testes de runtime trabalham em `build/tests/runtime` e executam a partir de `res`, sem substituir catálogos distribuídos ou saves. Os cenários gráficos opcionais exigem um runtime isolado com os recursos originais locais. Nesse runtime, executar o binário de testes com `--test-exit menu`, `--test-exit window`, `--test-exit movie`, `--autotest 0 --frames 30 --fps 60` ou `--test-context-failure`. No cenário de filme, fechar a janela durante PROLOG. O teste de menu exercita a página de encerramento real; não substitui toda a navegação humana até ela.

Para ASan, compilar `ReleaseASan` e executar `rechan-asan.exe` e `support-tests-asan.exe`, com o runtime ASan instalado no PATH e `ASAN_OPTIONS=alloc_dealloc_mismatch=1:halt_on_error=1`. A distribuição normal deve ser gerada novamente sem `--tests`.

Compilações Release x64 e ASan foram executadas localmente. Avisos C4244 preexistentes permanecem fora do escopo. Não houve build Linux, avaliação visual definitiva ou certificação física de todas as revisões de controles. Testes curtos de fase não aprovam gameplay, combate ou desempenho de carregamento. Não há publicação de release como parte desta auditoria.
