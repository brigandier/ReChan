# Revisão linguística das legendas

Referência: [MOD-PTBR_Cutscenes](https://github.com/GABRIELNNK/ReChan/releases/tag/MOD-PTBR_Cutscenes),
arquivo `PTBReochan.zip`, SHA-256
`5bae7b6e6baf76c1f58f689a20e6d8a8c7bc9274136a9eba5eafb0eacdddb696`.
Os cinco SRTs foram aproveitados; nenhum vídeo foi copiado.
As branches `add-subtitle-support` (`1adc655`) e `backup-main` (`100b853`)
foram consultadas. A implementação antiga dependia de caminhos de mod e não foi
mesclada. O novo parser utiliza o relógio do MoviePlayer oficial.

## Correspondência

| Recurso | Vídeo nativo | Conteúdo da referência |
|---|---|---|
| PROLOG.srt | fe/movies/prolog.str | Entrega do pacote e sequestro do avô |
| FACTORY.srt | fe/movies/factory.str | Encontro na fábrica |
| VICTORY.srt | fe/movies/victory.str | Resgate e desfecho |
| CREDITS.srt | fe/movies/credits.str | Falas cômicas dos créditos |
| MAKING.srt | fe/movies/making.str | Entrevista e bastidores |

Os nomes foram confirmados no fluxo de Game::PlayMovie. A correspondência de
conteúdo vem dos SRTs e ainda precisa de conferência audiovisual. Os tempos foram
preservados integralmente; não há alegação de sincronismo confirmado em jogo.

## Critérios e limites

Revisão linguística completa dos 117 blocos disponíveis, com pontuação, remoção
de hesitações redundantes no making of, quebras de linha e concisão. Os rótulos de
falantes inconsistentes foram substituídos por travessões de diálogo representados
por hífen, sem inferir novos nomes. Música e interjeições adequadas foram mantidas.
Todas as modificações abaixo são propostas editoriais para avaliação experimental:
não há transcrição inglesa nem áudio original no checkout para validar a fidelidade.

Pendências especialmente importantes: PROLOG 14 (fala muito curta em 13,75 s),
MAKING 53 ("Eu?" em 15,1 s), CREDITS 9 ("blá" pode ser erro de transcrição),
FACTORY 3/8/11 (tom e troca de falante), MAKING 15 (helicóptero),
VICTORY 8/9 (gritos sobrepostos). Não foram inventadas falas para preencher lacunas.

## Textos alterados

| Arquivo/bloco | Referência | Proposta |
|---|---|---|
| PROLOG 2 | Jackie, Jackie, | Jackie! Jackie! |
| PROLOG 3 | quero que você leve este pacote para este endereço. | Quero que você leve este pacote / para este endereço. |
| PROLOG 4 | É muito importante que você dê a máxima atenção a este pacote. | É muito importante que você / cuide bem deste pacote. |
| PROLOG 5 | Se eu descobrir que você está perambulando pelo café de novo, | Se eu descobrir que você está / perambulando pelo café de novo, |
| PROLOG 6 | se empanturrando, ah... / Jackie: não se preocupe, vovô, | - se empanturrando... / - Não se preocupe, vovô. |
| PROLOG 7 | estou a caminho. | Estou a caminho. |
| PROLOG 8 | Ok, chefe, vamos lá, | Certo, chefe. Vamos lá. |
| PROLOG 9 | vamos pegar e trazer o velho. | Vamos pegar o velho / e trazê-lo para cá. |
| PROLOG 11 | Não é seu neto? | Não é o seu neto? |
| PROLOG 12 | Vovô, | Vovô! |
| PROLOG 13 | cuidado, | Cuidado! |
| PROLOG 14 | continue a perseguição! | Continue a perseguição! |
| PROLOG 15 | Jackie, vá atrás do caminhão, do pacote, Jackie, | Jackie, vá atrás do caminhão! / O pacote, Jackie! |
| PROLOG 16 | Jackie, do pacote, vovô, | - Jackie, o pacote! / - Vovô! |
| PROLOG 17 | vovô. | Vovô! |
| FACTORY 3 | Por que você está fazendo isso? / Chefe: Simplesmente, meu velho, porque eu posso. | - Por que você está fazendo isso? / - Ora, meu velho, porque eu posso. |
| FACTORY 4 | Tenho tão poucos prazeres na vida ultimamente. | Tenho tão poucos prazeres / na vida ultimamente. |
| FACTORY 5 | Às vezes, são as pequenas coisas que | Às vezes, são as pequenas coisas |
| FACTORY 6 | me fazem sorrir | que me fazem sorrir. |
| FACTORY 8 | Ah, desculpe, chefe, ele me pegou de surpresa, não foi? | Desculpe, chefe. Ele me pegou / de surpresa, não foi? |
| FACTORY 10 | por isso, | por isso. |
| FACTORY 11 | Vovô.  / Chefe: Ah, o entregador, nós... | - Vovô! / - Ah, o entregador. Nós... |
| VICTORY 2 | Ah, eu não duvidei de você nem por um segundo! | Eu não duvidei de você / nem por um segundo! |
| VICTORY 3 | Agora, me desamarre antes que eu perca / a circulação nas minhas mãos! | Agora me desamarre, antes que / eu perca a circulação nas mãos! |
| VICTORY 5 | Jackie | Jackie! |
| VICTORY 10 | Templo Shaolin | Templo de Shaolin |
| VICTORY 12 | Viu, vovô, eu te disse para não se preocupar. | Viu, vovô? Eu disse / para não se preocupar. |
| VICTORY 16 | Estou com fome. / Que tal algo para comer? | Estou com fome. / Que tal comer alguma coisa? |
| VICTORY 18 | Você está com uma solitária ou o quê? | Você está com solitária ou o quê? |
| VICTORY 19 | Se você não parar de comer desse jeito, / vai inchar que nem um baiacu. | Se continuar comendo desse jeito, / vai inchar que nem um baiacu. |
| VICTORY 20 | Hahaha. / Está tentando me matar? | Ha, ha, ha! / Está tentando me matar? |
| CREDITS 2 | Sou eu, o Jackie Chan Unplugged! | Sou eu, Jackie Chan Unplugged! |
| CREDITS 6 | Ai, minha cabeça. Ai, minha cabeça. | Ai, minha cabeça! / Ai, minha cabeça! |
| CREDITS 8 | Você está bem? Eu vou limpar isso. | Você está bem? / Eu vou limpar isso. |
| CREDITS 9 | Pára com isso, blá! | Para com isso, blá! |
| MAKING 2 | Olhem para mim. Eu nunca fiz isso antes. Quero tentar, ver o que acontece. | Olhem para mim. Nunca fiz isso antes. / Quero tentar, ver o que acontece. |
| MAKING 3 | E também, sabe, [limpa a garganta] o jogo e o computador nunca envelhecem. | E, sabe, o jogo e o computador / nunca envelhecem. [pigarreia] |
| MAKING 4 | Aí eu posso me ver no computador por muito tempo, sabe, por, sabe, só continuar | Então posso me ver no computador / por muito tempo, sempre... |
| MAKING 5 | na minha idade. Eu posso envelhecer, mas o computador nunca envelhece. | com a mesma idade. Eu posso envelhecer, / mas o computador nunca envelhece. |
| MAKING 6 | Então eu, eu acho melhor eu manter meus movimentos. | Então acho melhor / preservar meus movimentos. |
| MAKING 7 | Talvez algo que eu consiga mover hoje, eu talvez não consiga mover daqui a 10 anos. | O que consigo fazer hoje, / talvez não consiga daqui a dez anos. |
| MAKING 8 | Tenho que manter meus movimentos, manter minha, manter minha atuação, manter meu humor, colocar tudo | Preciso preservar meus movimentos, / minha atuação e meu humor. Colocar tudo |
| MAKING 9 | no computador. Aí um dia fica tipo, como uma memória. | no computador. Um dia, / isso vira uma lembrança. |
| MAKING 10 | É um jogo, eles mostram para o público, dão para as crianças jogarem, e também podem | É um jogo. Eles mostram ao público, / dão para as crianças jogarem e também podem |
| MAKING 11 | guardar, guardar, guardar a memória para mim mesmo. | guardar essa lembrança / para mim mesmo. |
| MAKING 13 | É totalmente diferente de, de, de filmes. | É completamente diferente dos filmes. |
| MAKING 14 | Sabe, uh, eles me fazem fazer alguma coisa, uh, tipo, uh, com ninguém, só | Eles me pedem para fazer algo / sem ninguém por perto, |
| MAKING 15 | atuando sozinho. Você tem que fingir que é, um, um helicóptero. | atuando sozinho. Você precisa / fingir que é um helicóptero. |
| MAKING 16 | Você tem que fingir que o caminhão está vindo, você faz assim... | Precisa fingir que o caminhão / está vindo. Você faz assim... |
| MAKING 17 | Sabe. Geralmente nós fazemos as coisas de verdade, sabe. | Geralmente fazemos / as coisas de verdade, sabe? |
| MAKING 18 | O caminhão vem, bum, você é atingido. Você realmente é... | O caminhão vem, bum! / Você é atingido de verdade... |
| MAKING 19 | Agora você realmente, você atua por nada. | Agora você atua sem nada ali. |
| MAKING 20 | Você só atua para o computador. Mas é muito interessante para mim, é. | Você atua só para o computador. / Mas acho isso muito interessante. |
| MAKING 22 | Eu jogo videogames porque o jogo realmente me dá muitas ideias e, e mantém | Jogo videogame porque os jogos / me dão muitas ideias e mantêm |
| MAKING 23 | minha mente funcionando. Mas agora, os jogos estão mais difíceis. | minha mente ativa. Mas, hoje, / os jogos estão mais difíceis. |
| MAKING 24 | Não é como, uh, 10 anos atrás, 15 anos atrás. | Não é como há dez ou quinze anos. |
| MAKING 25 | Era simples, dois botões. Mas agora, botões demais. | Era simples: dois botões. / Agora há botões demais. |
| MAKING 26 | Você só, nossa, te deixa muito ocupado. | Nossa, é muita coisa para fazer! |
| MAKING 28 | Eles me escaneiam a cada, a cada tipo, uh, cinco horas. | Eles me escaneiam / a cada cinco horas, mais ou menos. |
| MAKING 29 | Aí eu, eu acho que é bom. Por quê? Porque depois do escaneamento, meu rosto fica com essa | Acho isso bom. Por quê? / Porque, depois, meu rosto fica |
| MAKING 30 | idade, nunca muda. Assim espero. | sempre com a mesma idade. / Assim espero. |
| MAKING 32 | Eu quero colocar mais humor. Não é, uh, só a luta. | Quero colocar mais humor. / Não é só a luta. |
| MAKING 33 | Humor e comédia, isso é o mais importante. | Humor e comédia / são o mais importante. |
| MAKING 34 | Eu não gosto de violência. Mas entre violência e ação há um grande dilema. | Não gosto de violência. Mas há / um grande dilema entre violência e ação. |
| MAKING 35 | Todo mundo acha que ação é violência, mas olhem para os meus filmes. | Todo mundo acha que ação é violência, / mas olhem para os meus filmes. |
| MAKING 36 | Eu gosto de ação, então eu coloco o mínimo de violência possível. | Gosto de ação, então coloco / o mínimo de violência possível. |
| MAKING 37 | Até no jogo, eu digo a eles para colocarem mais comédia, mostrarem meus pulos físicos, | Até no jogo, peço que coloquem / mais comédia, que mostrem meus saltos |
| MAKING 38 | chutes. Hum, aí as crianças podem aprender com todos os movimentos de salto, giro, chute, | e chutes. Assim, as crianças podem / aprender com os saltos, giros, chutes |
| MAKING 39 | sabe, soco. É tipo, uh, exercício. | e socos, sabe? / É como fazer exercício. |
| MAKING 41 | Hoje em dia, nos filmes americanos, há muitas e muitas explosões, muitas trocas de tiro. | Hoje em dia, nos filmes americanos, / há muitas explosões e muitos tiroteios. |
| MAKING 42 | Então eu digo a mim mesmo: "Sem explosões, sem tiros." Eu quero algo totalmente diferente dos | Então digo a mim mesmo: "Sem explosões, / sem tiros." Quero algo bem diferente |
| MAKING 43 | outros astros de ação. Então, quando eles têm, tipo, sangue saindo pelo nariz, | dos outros astros de ação. / Quando o sangue sai pelo nariz |
| MAKING 44 | pela boca, beleza, no meu filme não tem sangue. Mesmo com bam, bam, bam, bam, bam, | ou pela boca, tudo bem. Mas, no meu filme, / não há sangue. Mesmo com tanta pancada, |
| MAKING 45 | as pessoas se levantam, sem hematomas, nada. É... | as pessoas se levantam / sem hematomas, sem nada. |
| MAKING 46 | Eu, eu, eu quero ser como uma história em quadrinhos. É por isso que eu luto com a escada, e, uh, | Quero ser como uma história em quadrinhos. / Por isso luto com uma escada, |
| MAKING 47 | com o bastão, com tudo. Mas quando eu venho para a América, muitos diretores me perguntam: | com um bastão, com tudo. Mas, nos EUA, / muitos diretores me perguntam: |
| MAKING 48 | "Como você consegue fazer isso? Como você consegue lutar com a escada? | "Como você consegue fazer isso? / Como consegue lutar com uma escada? |
| MAKING 49 | Como você consegue pular por cima do, do, do prédio? | Como consegue pular / por cima de um prédio? |
| MAKING 50 | Como você faz toda a, uh, uh, coreografia, todas as cenas de luta?" | Como faz toda a coreografia, / todas as cenas de luta?" |
| MAKING 51 | Essas coisas para mim são fáceis. Então eu acho que é um tipo de mundo diferente, é. | Para mim, isso é fácil. / Acho que é um mundo diferente. |
