# Fase A — spike de portabilidade (resultados)

Pergunta da fase A: **o código de jogo do decomp compila com clang pra host?**
(antes de investir no backend Orbis). Spike feito localmente contra o Petari; o
source do decomp **não** vive neste repo.

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

**Não** apareceram, nesse TU, bloqueios de PPC (intrínsecos/asm) nem de
big-endian. Ou seja: no nível de **linguagem**, o código de jogo parece bem
portável; o trabalho pesado do porte está no **backend de plataforma** (GX,
áudio, input), não em `src/Game`.

## Limitação desta medição
A tentativa de medir uma **taxa de aprovação** sobre uma amostra de 134 arquivos
de `src/Game` esbarrou em **include paths por-arquivo** (cada subsistema precisa
do seu set; um `-I` fixo não cobre todos → "file not found", que é plumbing, não
portabilidade). A medição ampla e confiável precisa dirigir o clang pelos
**comandos de compilação reais do build** (compile DB), não por um set fixo.

## Próximos passos concretos
1. **Harness de compile-DB:** gerar/extrair os comandos por-arquivo do build e
   rodar o clang com o include set exato de cada TU → **taxa real de
   portabilidade** de `src/Game` com `-fdelayed-template-parsing`.
2. **Catalogar os casos que exigem fix de source** (não só flag): big-endian,
   intrínsecos de paired-single/PPC, inline asm — se existirem.
3. Definir os **knobs de toolchain do porte**: clang + `-fdelayed-template-parsing`,
   trocar **MSL_C/MSL_C++/Runtime** por libc/libc++, dropar **MetroTRK**.

## Conclusão da fase A (parcial, honesta)
Evidência inicial **encorajadora**: um arquivo de jogo real cruza o clang do host
com um único flag de compatibilidade e zero mudança de source. Falta a medição
ampla (via compile-DB) pra cravar a taxa — mas o custo do porte tende a se
concentrar no backend de plataforma, como o `PLATFORM-BOUNDARY.md` previa.
