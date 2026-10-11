# 02 — Publicar somente o código próprio do porte

## Regra

O conteúdo público é o nosso patch/compatibilidade, backend, casca, ferramentas,
documentação e arte original. **Nenhum código de jogo é publicado**, nem source
do decomp, código recompilado, trecho copiado, objeto linkado ou asset do dump.
O Petari e os resultados derivados permanecem locais, fora do conteúdo Git.

Patches futuros são aplicados no checkout privado por ferramentas próprias.
Não publicar unified diffs contendo linhas/contexto do jogo; `diffs/` é privado.
Uma anotação `@orig none` declara a origem, mas não transforma código copiado em
original: a revisão de origem continua obrigatória.

## Proteções implementadas

[`tools/publication/check.py`](../tools/publication/check.py) audita três fontes:

- **Workspace:** arquivos rastreados e novos não ignorados.
- **Índice:** os blobs efetivamente staged, inclusive arquivos ignorados
  adicionados com `git add -f`; não se baseia na working copy.
- **Histórico:** todos os commits alcançáveis por HEAD, branches, tags e refs
  remotas disponíveis localmente, incluindo arquivos já removidos. O hook de
  push acrescenta os OIDs propostos, inclusive commits fora dessas refs.

Há uma lista explícita de paths/tipos permitidos. Binários, dumps, objetos,
logs/relatórios de medição, decomp, arquivos gerados, submodules, symlinks e
pointers LFS são rejeitados. Código/build/hooks/workflows exigem `@orig none`
no cabeçalho; qualquer anotação de código derivado bloqueia a publicação.
Os únicos binários permitidos são os PNGs originais de ícone/splash, com hashes
revisados e fixados no checker, incluindo o ícone original histórico.

Quando o Petari privado está disponível, o checker também compara janelas de
96 tokens, desconsiderando comentários e whitespace. Isso identifica cópias
literais longas mesmo com nome e cabeçalho trocados. É uma **heurística adicional**;
não prova autoria nem detecta toda cópia curta, reescrita ou renomeada.
Nenhum source privado, trecho coincidente ou corpus é exportado pelo checker.

## Uso e hooks

```sh
# Auditoria completa. Descobre automaticamente ../smg-decomp/Petari se existir.
python3 tools/publication/check.py

# Para outro layout privado:
python3 tools/publication/check.py --private-source /caminho/Petari

# Testes sem jogo/dump: todos os fixtures são originais e sintéticos.
python3 -m unittest discover -s tools/publication -p 'test_*.py' -v

# Ativar hooks em outro clone (ativados neste workspace em 11/10/2026):
git config --local core.hooksPath .githooks
```

O hook `pre-commit` audita o índice. O `pre-push` audita workspace, índice,
histórico e commits propostos. `QVX_PRIVATE_SOURCE` pode indicar o checkout
privado para ambas as verificações. Esses hooks não fazem commit nem push.

O [workflow de CI](../.github/workflows/publication.yml) executa os testes e a
auditoria estrutural com histórico completo e sem jogo/dump. O checkout usa
`fetch-depth: 0`, conforme a
[documentação do actions/checkout](https://github.com/actions/checkout).
A CI não faz upload de PKG, objetos, relatórios ou conteúdo local.
Ela será executada no GitHub quando este workflow for commitado e enviado;
esta alteração não realizou publicação.

Um clone shallow é rejeitado para auditoria do histórico. Refs remotas não
presentes no clone não são auditadas. Os hooks e a CI complementam a revisão
do diff; não autorizam distribuir um motor que contenha código do jogo.

## Auditoria inicial — 11/10/2026

| Evidência | Estado |
|---|---|
| Índice atual: 23 arquivos sem jogo/dump/objetos derivados | ✅ |
| Histórico público local: 13 commits sem conteúdo proibido | ✅ |
| Comparação adicional com Petari privado | ✅ sem coincidência literal detectada |
| Ícones/splash atuais e ícone histórico revisados | ✅ arte própria |
| Fixtures de índice, `git add -f`, histórico e cópia disfarçada | ✅ 6 testes passando |
| Hooks locais | ✅ ativados |
| Workflow de CI | ✅ preparado; execução remota pendente do envio |

Os objetos da Fase A continuam em `out/`, gitignored. O decomp permanece no
workspace privado adjacente; não foi importado ou adicionado ao índice público.
