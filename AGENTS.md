# Instruções do projeto — RMGE01 para Orbis

## Referência de arquitetura

Consultar [TwilitRealm/dusklight](https://github.com/TwilitRealm/dusklight) antes
de implementar novos blocos de plataforma: ABI/memória, OS, disco, GX e input.
O usuário pediu explicitamente para aproveitar conhecimento e arquitetura
desse projeto. A primeira análise e os links por subsistema estão em
[`docs/04-referencia-dusklight.md`](docs/04-referencia-dusklight.md).

Estudar também Aurora e Borealis, identificadas como dependências do Dusklight.
Revalidar os arquivos relevantes no upstream antes de basear uma implementação
neles; os links da análise inicial apontam para branches móveis. Distinguir
evidência de source, hipótese de adaptação e comportamento observado em hardware.

## Fronteira de publicação

Publicar somente nosso patch, backend, casca, ferramentas e documentação.
Não copiar source ou corpos de funções do jogo, nem importar o decomp ou
unified diffs com contexto dele. Motor e objetos derivados são locais.
A consulta ao Dusklight não altera essa regra e não autoriza incorporar seu
source ou dependências. Ver `docs/02-publicacao-apenas-codigo-proprio.md`.

## Loop de implementação

Implementar por marco funcional, testar no host e reconstruir a casca em PKG
quando houver alteração executável. Auditar com `tools/publication/check.py`.
Relatórios/objetos em `out/` permanecem gitignored. Não confundir TU compilado,
motor linkado, PKG validado e boot no PS4: registrar cada evidência separadamente.
