# Regra de proveniência (obrigatória)

> **Publicamos somente o código próprio do porte.** Código de jogo ou derivado,
> mesmo anotado, nunca entra no Git público. A referência de origem é obrigatória
> para os artefatos derivados **locais**; não é autorização para redistribuí-los.
> A [política de publicação](02-publicacao-apenas-codigo-proprio.md) e seu checker
> prevalecem sobre os exemplos históricos de codegen abaixo.

## Por quê

1. **Cena limpa.** Traça uma linha nítida entre (a) código **derivado
   mecanicamente** do binário do usuário (recompilado) e (b) código **original
   nosso** (o runtime/HLE, Apache-2.0). Cada linha fica auditável.
2. **Reprodutibilidade.** A referência amarra cada unidade gerada a um endereço do
   binário e a uma versão da ferramenta — o mesmo dump + mesmo codegen reproduz o
   mesmo resultado.
3. **Honestidade técnica.** Referenciamos o **binário** (`main.dol`), que é o que
   temos; não há source code da Nintendo envolvido, e nenhum byte do jogo é
   redistribuído (BYO-dump).

## O que "referenciar a origem" significa

Referência ao **binário original**, por **endereço** (e símbolo quando conhecido),
**não** ao source da Nintendo (que não possuímos) e **nunca** colando bytes do
jogo no repo.

## Formato de anotação

**Código derivado** (saída do codegen, ou wrapper amarrado a uma função do jogo):

```c
// @orig  RMGE01/main.dol @ 0x80012345..0x800124f0
// @sym   __ct__9SomeClassFv            (se conhecido; senão: @sym none)
// @gen   recomp  asi:FR69GJ5CI  tool=codegen@<versão>
```

**Código original nosso** (runtime, HLE, build, ferramentas):

```c
// @orig  none — trabalho original (quovix), Apache-2.0
```

- `@orig` — fonte da derivação: `RMGE01/main.dol @ <addr>..<addr>`, ou `none`.
- `@sym`  — símbolo original, se vier de RE da comunidade; **cite a fonte do
  símbolo**, não embarque bancos de símbolos de terceiros sem licença compatível.
- `@gen`  — método (`recomp`), ASIC do artefato e versão da ferramenta que gerou.

## Manifesto de proveniência

O codegen emite `provenance.jsonl` (uma linha por unidade gerada), a fonte de
verdade da traçabilidade:

```json
{"unit":"sub_80012345","orig":"RMGE01/main.dol","addr":"0x80012345","size":428,"sym":null,"gen":"recomp","asic":"FR69GJ5CI","tool":"codegen@0.0.0","src_sha1":"<sha1 do main.dol do dump>"}
```

O `src_sha1` identifica **qual** `main.dol` (do dump do usuário) — sem conter o
binário.

## Enforcement (CI)

O checker implementado em `tools/publication/check.py` exige `@orig none` no
código público, rejeita conteúdo derivado e audita também índice e histórico.
Hooks locais e CI executam essa proteção. O `.gitignore` é uma primeira barreira;
o checker detecta inclusive arquivos proibidos adicionados à força.

Codegen, wrappers derivados e `provenance.jsonl` permanecem no build privado,
fora do Git. Sua verificação de proveniência local ainda é uma etapa futura,
se o caminho de codegen for retomado; não se confunde com publicar seu conteúdo.

## Fronteira que isso protege

| Categoria | `@orig` | Licença | Vai pro repo? |
|---|---|---|---|
| Código gerado (recomp) | `RMGE01/main.dol @ addr` | derivado — **não redistribuído como fonte**; gerado no build a partir do dump | não (gerado localmente) |
| Runtime/HLE original | `none` | Apache-2.0 | sim |
| Ferramentas | `none` | Apache-2.0 | sim |
| Assets do jogo | — | Nintendo | **nunca** |
