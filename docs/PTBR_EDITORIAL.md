# Revisão PT-BR — interface

Base oficial: `5029a5008c842232886ee709f58d5282c9f78103`, conferida em 20/10/2026.
Referência PT-BR: GABRIELNNK/ReChan, `backup-main`, commit
`100b853f64eeaebc0d3fc610e17d006bffdbc88e`, arquivo `res/pc/text/portuguese.txt`.
Código e tradução reutilizados permanecem sob a licença MIT existente.

Todos os textos do catálogo inglês distribuído foram lidos e comparados com a
tradução de referência. Os textos adequados foram preservados. As outras quatro
línguas foram consultadas nas ambiguidades; algumas traduzem Counter como
contador, por isso o inglês e o comando de combate prevalecem: contra-ataque.
O catálogo contém apenas um override de texto PSX. Não há uma imagem original
ou recursos extraídos neste checkout; a cobertura de textos contidos apenas no
disco e a correspondência audiovisual das legendas precisam de avaliação em jogo.
Carregamento e auditoria estrutural não constituem aprovação editorial visual.

## Glossário

| Original | PT-BR | Contexto |
|---|---|---|
| Level | Fase | Progressão |
| Zone | Zona | Subdivisão da fase |
| Gold Dragon | Dragão dourado | Colecionável |
| Red Dragon | Dragão vermelho | Colecionável |
| Grade | Nota | Resultados |
| Counter | Contra-ataque | Comando de combate |
| Strafe | Mover de lado | Movimento |
| Dive Roll | Rolamento | Movimento |
| Grab | Agarrar | Comando |
| Save slot | Espaço de salvamento | Persistência |
| Button prompts | Ícones de botões | Interface |
| Subtitles | Legendas | Vídeos |

## Alterações em relação à referência

| Identificador | Referência | Revisão | Justificativa |
|---|---|---|---|
| `FE_AUTO_WARN` | Este jogo possui um recurso de salvamento automático. Por favor, não desligue o jogo quando vir este ícone. | Este jogo salva automaticamente. Não feche o jogo enquanto este ícone estiver na tela. | Mensagem direta e natural; preserva o aviso de salvamento. |
| `FE_TLHD` | Resultados do Nível | Resultados da fase | Level é fase no contexto de progressão. |
| `FE_TLFG` | Luta | Combate | Categoria de pontuação; confirmado por espanhol, francês, italiano e alemão. |
| `FE_TLGO` | Dragões de Ouro | Dragões dourados | Gold dragon designa o colecionável, não seu material. |
| `FE_TLRD` | Dragões Vermelhos | Dragões vermelhos | Padronização dos colecionáveis. |
| `FE_TLBG` | BÔNUS +1 DRAGÃO DE OURO | BÔNUS +1 DRAGÃO DOURADO | Mesmo nome do colecionável. |
| `FE_TLMB` | BÔNUS DE FILME DESBLOQUEADO | VÍDEO BÔNUS DESBLOQUEADO | Movie bonus é o vídeo desbloqueado, não um bônus de filme. |
| `FE_SLE` | Slot de Salvamento %d Vazio | Espaço de salvamento %d vazio | Slot é espaço de salvamento; placeholders preservados. |
| `FE_SLD` | Slot de Salvamento %d - %s | Espaço de salvamento %d - %s | Terminologia de salvamento; placeholders preservados. |
| `FE_SVMAN` | SLOTS DE SALVAMENTO | ESPAÇOS DE SALVAMENTO | Terminologia de salvamento. |
| `FE_SVC_SLOT` | SLOT | ESPAÇO | Título curto de coluna. |
| `FE_SVC_RED` | VERMELHO | VERMELHOS | Refere-se aos dragões. |
| `FE_SVC_GOLD` | OURO | DOURADOS | Refere-se aos dragões. |
| `FE_LPOR` | Português | Português (Brasil) | Identifica explicitamente a variante regional. |
| `FE_CST` | DESLOCAMENTO LATERAL | MOVER DE LADO | Verbo de ação curto para a ilustração do controle. |
| `STRAFE` | Deslocamento Lateral | Mover de lado | Mesmo comando da ilustração. |
| `FE_KBD` | Mapeamento de Teclas | Configurar teclas | Rótulo curto e natural. |
| `FE_KBPR` | Pressione uma tecla ou mouse | Pressione uma tecla ou botão do mouse | Mouse não é uma tecla. |
| `FE_KBSLT` | <ACT:MENU_LEFT>/<ACT:MENU_RIGHT> Slot | <ACT:MENU_LEFT>/<ACT:MENU_RIGHT> Espaço | Slot é a posição da associação. |
| `FE_KBCLR` | <ACT:MENU_CLEAR> Remover Mapeamento | <ACT:MENU_CLEAR> Desvincular | Rótulo curto para remover a associação. |
| `FE_NTD` | Não Usado | Sem uso | Rótulo conciso. |
| `FE_VYS` | Sincronização Vertical: | Sincronia vertical: | Reduz o comprimento do rótulo. |
| `FE_DSS` | Suavização de Sombra: | Suavização das sombras: | Concordância com sombras dinâmicas. |
| `FE_VCV` | Volume da Voz: | Volume das vozes: | Abrange os diálogos do jogo. |
| `FE_DLQ` | Tem certeza de que deseja excluir este slot de salvamento? | Deseja excluir este espaço de salvamento? | Terminologia consistente. |
| `FE_SVQ` | Tem certeza de que deseja salvar neste slot? | Deseja salvar neste espaço? | Terminologia consistente. |
| `FE_SVD` | Arquivo de jogo salvo com sucesso. | Jogo salvo com sucesso. | Remove construção artificial sem perder o sentido. |
| `FE_DLD` | Arquivo de jogo excluído com sucesso. | Jogo salvo excluído com sucesso. | Distingue a exclusão do salvamento da exclusão do jogo instalado. |
| `FE_GRD` | CONCEITO: | NOTA: | Mesma tradução de Grade na tela de resultados. |
| `FE_LVL` | Nível %d | Fase %d | Level é fase no contexto de progressão. |
| `FE_EXL` | Sair do Nível | Sair da fase | Terminologia de progressão. |
| `FE_EXLR` | Tem certeza de que deseja sair deste nível? | Tem certeza de que deseja sair desta fase? | Terminologia de progressão. |
| `FE_LOC_WTR` | Waterfront | Zona portuária | Nome de local; alemão Hafenviertel auxilia a interpretação de Waterfront. |
| `FE_LOC_SWR` | Esgoto | Esgotos | Nome natural para o conjunto de fases. |
| `FE_LOC_ROF` | Telhado | Telhados | Nome natural para o conjunto de fases. |
| `FE_LOC_TMP` | Templo Shaolin | Templo de Shaolin | Construção natural e consistente com as legendas. |
| `FE_DTG` | TOTAL DE DRAGÕES DE OURO  %d | TOTAL DE DRAGÕES DOURADOS  %d | Mesmo nome do colecionável e placeholder intacto. |
| `FE_UPD_UTD` | Você está atualizado! | O jogo está atualizado! | A atualização é do jogo, não do jogador. |
| `FE_UPD_CHKNOW` | Verificar agora | Verificar atualizações | Explicita a ação do botão original. |
| `FE_CH_LEVL` | Todos os Níveis: | Todas as fases: | Terminologia de progressão. |
| `FE_CH_GOD` | Modo Deus: | Invencibilidade: | Expressa o efeito do God Mode em português natural. |
| `FE_CH_PNCH` | One Punch Man: | Um soco só: | Expressa a função da trapaça. |
| `FE_CH_QUAKE` | Stuntquake: | Terremoto acrobático: | Nome descritivo, auxiliado por espanhol e italiano. |

`FE_PS` e `FE_SUB` são novos rótulos. Removido o delimitador Markdown solto no fim da referência. Não foram alterados hashes, placeholders ou códigos de ação.
