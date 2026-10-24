# Revisão de robustez do código-base

> Escopo: auditoria inicial baseada em leitura estática de módulos do código-base e comparação com os arquivos correspondentes desta fork. As observações são pontos para investigação, não falhas reproduzidas. Não considero a lista exaustiva.

## Objetivo

Quero identificar possíveis problemas de estabilidade, integridade de salvamentos, segurança de leitura de recursos e confiabilidade dos sistemas de atualização e extração. Antes de modificar qualquer implementação, quero confirmar cada suspeita com o código atual, testes e análise de impacto.

Quero manter esta revisão **separada das alterações experimentais de gameplay e desempenho**. Não desejo alterações nos meus textos, legendas, recursos PlayStation/DualSense ou no comportamento original sem necessidade comprovada.

## Pontos para investigação

### P1 — Validação de limites ao ler recursos

- [ ] **TIM — `src/pc/tim.cpp`, `Tim::LoadFromMemory` (região 82–190).** Encontrei checagens para os cabeçalhos, mas não identifiquei validação integral do tamanho das seções CLUT e imagem antes do `memcpy` e do acesso às linhas de pixels. Verificar `cs->size`, `cs->w * cs->h`, dimensões zero e multiplicações de alocação. Reproduzir com exemplos sintéticos válidos, truncados e com tamanhos inconsistentes; rejeitar entradas malformadas sem leituras fora do buffer.
- [ ] **RIFF/WAV — `src/extra/modloader.cpp`, `ReadWavPCM16` (região 748–798).** Revisar a aritmética `u32` de `chunkStart + chunkSize`, a progressão dos chunks, o alinhamento RIFF e a validade de `dataSize`, canais e taxa de amostragem. Testar chunks truncados, tamanhos inválidos e PCM desalinhado. Conferir também `src/extra/wavloader.cpp` e se sua implementação tem consumidores ativos.
- [ ] **STR — `src/radmovie/movieplayer.cpp`.** Auditar limites de dimensões informadas por metadados, `frameWidth * frameHeight`, `sectorCount * 4032`, buffers e alterações de dimensão entre frames. Confirmar os riscos usando a versão **atual desta fork**, que já possui modificações próprias no módulo. Não misturar esta investigação com mudanças editoriais de legendas.

### P2 — Extração, persistência e recursos importados

- [ ] **ISO9660 — `src/extra/psxdiscextractor.cpp`.** Verificar overflow de `(size + 2047u) / 2048u`, deslocamentos de setores, validade de extents, travessia recursiva de diretórios e possibilidade de diretórios cíclicos. Conferir normalização das partes do caminho e garantia de que arquivos extraídos permaneçam dentro das pastas permitidas. Preservar a validação do disco correto.
- [ ] **Saves — `src/gen/savegame.cpp`, `ReadBlobAtPath` e `WriteBlobAtPath`.** Investigar gravação diretamente no destino, corrupção após interrupção, validação de valores carregados e estratégia de gravação temporária/substituição atômica quando apropriada. Priorizar **compatibilidade com todos os salvamentos existentes** e evitar perda de progresso. Criar testes de falha de I/O e arquivo parcial.
- [ ] **Configurações — `src/pc/settings.cpp`, `WriteWholeFile` e `GameSettings::Save`.** Verificar se uma escrita parcial é tratada como sucesso, se há alternativa segura em caso de falha e se a versão da fork já contém tratamento adicional.
- [ ] **GLB/glTF — `src/extra/gltfloader.cpp`.** Revisar uso de URI de textura externa em `LoadTextureImage` (normalização e limites de caminho), validação de dados importados e limpeza em falhas de alocação. Investigar especialmente a substituição de `jointOrderMap` em `BuildSkeleton` após liberar a alocação anterior, verificando se há tratamento para nova alocação nula.

### P2 — Atualização e recuperação de erros

- [ ] **Autoatualizador — `src/extra/autoupdater.cpp` e `.h`.** Verificar se um arquivo temporário antigo pode ser confundido com download válido quando `curl` falha; confirmar a confiabilidade da seleção da URL e do tamanho esperado, a validação do pacote e o tratamento de caracteres especiais em caminhos Windows. Conferir também `CancelDownload` durante operação bloqueante, atualização real do progresso e sincronização dos campos acessados entre threads. Propor correções proporcionais e testáveis.
- [ ] **Estado após falha.** Investigar se falhas de arquivo, áudio, extração ou importação deixam objetos parcialmente inicializados, arquivos temporários não removidos ou recursos perdidos. Priorizar caminhos realmente executados pelo jogo.

## Método e critérios

1. **Primeiro verificar a versão atual da minha fork.** Os arquivos do núcleo podem ter divergido do código-base; descartar itens já corrigidos. Não modificar a integração experimental em andamento.
2. Registrar para cada ponto: **confirmado, descartado ou pendente**, evidência técnica, impacto, gravidade, condições de reprodução e menor alteração capaz de resolvê-lo.
3. Fazer testes automatizados determinísticos com arquivos sintéticos pequenos (válidos e inválidos), além de diagnósticos como AddressSanitizer quando adequado. Não incluir recursos ou dados proprietários do jogo nos commits.
4. Priorizar compilação, testes, profiling e relatórios no computador local usando Premake, Visual Studio/MSBuild, PowerShell e Python. Evitar builds remotos repetidos e processamento desnecessário.
5. Não fazer mudanças especulativas extensas nem alterar a jogabilidade, a tradução PT-BR, as legendas, controles PlayStation/DualSense, os saves existentes e o ModLoader sem justificativa concreta.
6. Após confirmar problemas, implementar correções objetivas **nesta branch de revisão**, com testes de regressão, mantendo a alteração exclusivamente nesta solicitação.
7. Não fazer merge automático. Ao final, quero um relatório com resultados, testes executados, limitações, riscos residuais e instruções detalhadas para qualquer QA manual necessário.
8. Manter documentação e comentários técnicos, preservando as licenças e a atribuição do projeto. Não criar referências ou menções em outros repositórios.

## Resultado esperado

Quero um conjunto de correções verificadas, com impacto mensurável sobre robustez e confiabilidade, sem regressões e sem perda de dados. Não quero que hipóteses dessa triagem sejam tratadas como bugs confirmados antes dos testes.
