# Build experimental PT-BR e PlayStation

Data: 20/10/2026. Base oficial conferida: `5029a5008c842232886ee709f58d5282c9f78103`.
O checkout estava limpo e já correspondia à `main` oficial. Nenhuma branch da
referência foi mesclada. Não houve push, pull request ou publicação de release.

Esta é uma implementação para avaliação experimental, não uma versão homologada
em jogo. Não há imagem original ou recursos PSX extraídos neste ambiente.

## Etapas e arquivos

| Etapa | Alteração e justificativa | Arquivos principais | Compatibilidade e validação |
|---|---|---|---|
| 1 — Auditoria | Leitura de README, licença MIT, BUILDING, MODDING, documentação de arquitetura e caminhos de texto, fonte, configuração, vídeo e entrada. SDL 2.30.11 confirmada; GLFW é o fallback. | Arquivos oficiais existentes; relatório atual | Base oficial atual preservada. Sem normas adicionais de contribuição ou AGENTS no checkout. |
| 2 — Idioma | Acrescentado idioma 5, sem renumerar inglês/alemão/francês/italiano/espanhol. Seletor e persistência existentes aproveitados. | `src/extra/customtext.*`, `fecustommenumgr.cpp`, `res/pc/text/portuguese.txt` | Seis catálogos com 212 identificadores; carregamento real e ida/volta de configurações executados. |
| 3 — Editorial | Catálogo distribuído comparado integralmente com o inglês e a referência PT-BR; quatro línguas auxiliares nas ambiguidades. Traduções adequadas preservadas. | `docs/PTBR_EDITORIAL.md`, `res/pc/text/` | Glossário e alterações registradas. Textos embutidos exclusivamente no disco, contexto audiovisual e encaixe final em tela continuam pendentes. |
| 4 — Legendas | SRT nativo em `pc/subtitles/pt-BR`, seleção por idioma e nome do vídeo, fonte Legal existente, branco com contorno e sombra. Opção em Som. Novo parser pequeno, independente do renderizador, usa o relógio monotônico já utilizado pelo vídeo. | `src/radmovie/movieplayer.*`, `subtitles.*`, `src/pc/settings.*`, `fecustommenumgr.*`, `res/pc/subtitles/pt-BR/` | 117 blocos lidos; UTF-8 estrito, BOM/CRLF, quebras, intervalos semiabertos, sobreposição e entradas inválidas testados. Sem dependência de `~mods`; vídeos intocados. |
| 5 — Controle ativo | Família exposta pela interface pddi existente; PS3/PS4/PS5 via SDL_GameControllerGetType. Poll mantém dispositivos abertos e acompanha nova atividade com limiar contra drift; fecha handles desconectados e interrompe vibração ao trocar de dispositivo. | `vendor/libp3d/pddi/gamepadfamily.h`, `pddidev.h`, `gl/glgamepadrumble_sdl.*`, `glrender.*`, `p3d/input.*` | Mesmos comandos normalizados e mesma API de vibração. Onze tipos virtuais e alternância entre dois dispositivos testados. Nenhuma atualização da SDL ou troca do GLFW. |
| 6 — Estilos | Preferência salva separada do estilo resolvido. Menu: Automático, Xbox, PlayStation, Nintendo Switch. IDs persistidos: Xbox=0, Switch=1, PlayStation=2, Auto=3. Novas configurações usam Auto. Desconhecido usa Xbox no PC; manual prevalece. | `controllerpromptstyle.h`, `inputaction.cpp`, `settings.cpp`, `fecustommenumgr.cpp` | Precedência, IDs legados, ordem do menu, caminhos de recursos e persistência testados. Dispositivo virtual é tratado conforme a identidade exposta pela biblioteca, sem adivinhar o controle físico. |
| 7 — Arte | Um estilo PlayStation baseado no DualSense, incluindo touchpad, Options, Share/Create, direcional, analógicos e símbolos. L/R originais e setas preservados; L3/R3 abaixo dos ícones de clique. Fontes SVG mantidas para revisão e rasterização precisa. | `res/art/controller_*_playstation.svg`, `res/pc/textures/frontend/controller_*_playstation.png`, `prompticons.h` | Overlay 1024x1024 e atlas 632x1808, RGBA; inspecionados visualmente. A geração raster de referência não preservava alinhamento; recursos finais usam fontes vetoriais determinísticas. Encaixe com rótulos e legibilidade em jogo pendentes. |
| 8 — Integração | Testes independentes opcionais, teste nativo sem imagem de disco e ajuste da seleção de toolset no processo Premake oficial. | `tests/`, `scripts/run-support-tests.ps1`, `validate-localization.py`, `src/pc/autotest.*`, `src/gen/main.cpp`, `premake5.*` | Windows x64 Release compilado com VS 2022/v143. Parser e estilos também executados com AddressSanitizer sem diagnóstico. Linux e regressão de gameplay ainda não executados. |
| 9 — Pacote | Pacote experimental local com executável, recursos PC, licença existente, avisos SDL/GLFW e relatórios de avaliação. | `scripts/pack-win64.ps1`, `shipping/rechan-win64-experimental.zip` | O empacotador copia somente `res/pc` como dados de jogo. Nenhum vídeo, ROM, imagem de disco ou recurso PSX extraído foi incluído. Continua exigindo a cópia original SLUS-00684, conforme o README oficial. |

## Compilação executada

Premake 5.0.0 oficial, ZIP Windows com SHA-256
`15e63506eed5dfd526b96c8e957a7feff2be6cfe70f46e353bfe4813b82e2fdc`.
Visual Studio Build Tools 2022 17.14, compilador MSVC 14.44/v143,
configuração Release, plataforma x64. Saída: `bin/rechan.exe`.
O script original ignorava os argumentos e forçava VS 2026/v145 apesar de
BUILDING indicar VS 2022. Agora respeita os argumentos e utiliza v143 para vs2022,
mantendo v145 para a geração vs2026.

Compilação concluída sem erros. Permanecem avisos C4244 em código preexistente de
câmera, HUD e popup do menu; essas operações não foram modificadas.
`dumpbin /dependents` verificou apenas DLLs de sistema Windows; SDL está estática.

## Testes executados e reprodução

```powershell
premake5 --tests vs2022
msbuild build/rechan.sln /m /p:Configuration=Release /p:Platform=x64
scripts/run-support-tests.ps1 -Python python
```

Os testes não precisam da imagem do disco. O teste de SDL exige que controles
físicos estejam desconectados para isolar os dispositivos virtuais.
O teste nativo escreve um INI em `build/tests/native`; não usa o INI do jogador.

| Verificação | Resultado executado |
|---|---|
| Identificadores, duplicatas, placeholders, códigos de ação e caracteres inválidos | PASS — 212 entradas em cada um dos seis idiomas |
| á à â ã é ê í ó ô õ ú ç e maiúsculas | PASS — cmap das três fontes distribuídas |
| Dimensões e transparência dos dois PNGs | PASS — atlas e overlay RGBA |
| Carregamento nativo e persistência | PASS — 6 idiomas × 4 estilos × 2 preferências de legenda; arquivos legados Xbox/Switch; defaults sem chaves novas |
| Parser SRT | PASS — 117 blocos, cem leituras/reinicializações por arquivo; UTF-8 inválido, tempos inválidos, arquivo vazio, limite de tamanho, ordenação, sobreposição e limites temporais |
| Consultas de tempo a 15/30/60/144 FPS | PASS — teste do parser; o relógio real do vídeo ainda depende de teste audiovisual |
| AddressSanitizer | PASS — testes independentes de parser e estilos; não é uma análise de vazamentos do ciclo de vídeo completo |
| Auto/manual, IDs legados, paths e nomes de mapeamentos GLFW | PASS |
| SDL virtual PS3, PS4, PS5, Xbox360, XboxOne, SwitchPro, JoyConLeft/Right/Pair, Virtual e Unknown | PASS — conexão, classificação, botões, callback de rumble e desconexão |
| Dois controles simultâneos | PASS — nova atividade troca o dispositivo; botão mantido no outro não rouba a seleção |
| Consulta de vibração em controle sem rumble | PASS — teste pela classe glGamepad com PS5 virtual sem rumble e Xbox virtual com rumble; consulta não muda o controle ativo nem seus ícones |
| Pacote experimental | PASS — CRC, ausência de entradas duplicadas, hash do executável, presença de tradução/SRTs/PNGs/licenças; teste nativo executado a partir do ZIP extraído |
| git diff --check | PASS |

## Avaliação em jogo ainda necessária

- Conferir fidelidade, falantes e sincronização de cada vídeo com a cópia original;
  os pontos editoriais de maior incerteza estão em `PTBR_SUBTITLES.md`.
- Conferir menus e os textos longos listados pelo validador, HUD e demais textos
  originais que não estão no catálogo distribuído.
- Inspecionar legendas e callouts do DualSense em 4:3, 16:9 e ultrawide.
- Pular vídeos durante falas, fechar janela, reproduzir novamente, alternar idioma
  e preferência; verificar memória no ciclo completo de vídeo/áudio/texturas.
- Testar controles físicos PS3/PS4/PS5 por USB/Bluetooth, reconexão, drift e vibração.
  Os callbacks virtuais verificam a API, não os motores físicos.
- Testar Steam Input e outros dispositivos virtuais; selecionar manualmente o estilo
  quando o driver expuser Xbox/genérico. A identidade física pode não estar disponível.
- Executar fallback GLFW com hardware, build Linux e `scripts/run-autotests.ps1`
  com recursos extraídos localmente, incluindo as regressões existentes de gameplay.

Sem dados originais, não há alegação de tradução integral dos recursos do disco,
sincronismo audiovisual homologado, ausência de vazamentos em jogo ou estabilidade
comprovada em hardware. IA, colisões, salvamento e vídeos originais não foram alterados.

## Futuras contribuições

Os commits locais separam catálogo, legendas, detecção, escolha de estilo, arte e
testes/empacotamento. Uma contribuição externa precisará de revisão individual
de escopo e das orientações atuais dos mantenedores: detecção/PlayStation,
infraestrutura geral, PT-BR e legendas devem ser avaliados separadamente.
Nenhuma contribuição externa foi aberta ou preparada como PR.
