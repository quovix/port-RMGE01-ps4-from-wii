# Fase A — spike de portabilidade (resultados)

Pergunta da fase A: **o código de jogo do decomp compila com clang pra host?**
(antes de investir no backend Orbis). Spike feito localmente contra o Petari; o
source do decomp **não** vive neste repo.

> Registro do spike inicial. A [medição ampla de 11/10/2026](01-medicao-portabilidade.md)
> supersede sua extrapolação: apenas 141 de 1.604 TUs passam. Há bloqueios reais
> em headers compartilhados, assembly PPC e extensões de linguagem.

## Ambiente
- macOS arm64, Apple clang 21.0.0, `-fsyntax-only` (só análise, sem codegen/link).
- Flags: `-std=c++98` + set de includes do decomp + defines de versão (RMGE01).

## Achado principal (positivo)
Um TU de jogo representativo — `src/Game/Animation/AnmPlayer.cpp` — **compila
limpo no clang do host: 0 erros, exit 0**, assim que se adiciona:

```
-fdelayed-template-parsing
```

A única parede real de C++ encontrada foi **two-phase name lookup**: o CodeWarrior
era leniente com nomes dependentes não-qualificados dentro de templates (ex. nos
headers de matemática do JSystem, `JGeometry/TVec.hpp`, `TMatrix.hpp`); o clang,
conforme ao padrão, reclama (`use of undeclared identifier`). O
`-fdelayed-template-parsing` faz o clang adiar o parse do corpo do template (como
o MSVC) e resolve — sem tocar no source.

**Não** apareceram, nesse TU, erros de PPC (intrínsecos/asm). Isso não verifica
endianness nem comportamento: foi apenas análise sintática. A medição ampla
posterior mostrou que o resultado desse arquivo não representa todo `src/Game`.

## Limitação desta medição
A tentativa de medir uma **taxa de aprovação** sobre uma amostra de 134 arquivos
de `src/Game` esbarrou em **include paths por-arquivo** (cada subsistema precisa
do seu set; um `-I` fixo não cobre todos → "file not found", que é plumbing, não
portabilidade). A medição ampla e confiável precisa dirigir o clang pelos
**comandos de compilação reais do build** (compile DB), não por um set fixo.

## Próximos passos concretos
1. **Harness de compile-DB (concluído em 11/10/2026):** gerar/extrair os comandos por-arquivo do build e
   rodar o clang com o include set exato de cada TU → **taxa real de
   portabilidade** de `src/Game` com `-fdelayed-template-parsing`.
2. **Catalogar os casos que exigem fix de source** (não só flag): big-endian,
   intrínsecos de paired-single/PPC, inline asm — se existirem.
3. Definir os **knobs de toolchain do porte**: clang + `-fdelayed-template-parsing`,
   trocar **MSL_C/MSL_C++/Runtime** por libc/libc++, dropar **MetroTRK**.

## Conclusão da fase A (parcial, honesta)
Um arquivo de jogo real cruza a análise do clang com um flag de compatibilidade
e zero mudança de source. A medição ampla já foi feita: existe trabalho de
compatibilidade C++/ABI antes do backend Orbis; veja o relatório atualizado.
