# 09 — Primeiro link do motor (link-test com stubs)

## Objetivo

A medição de portabilidade mostrou que 1.335 TUs de `src/Game` **compilam**
isoladamente. Compilar por-arquivo não prova que os objetos **compõem** num
binário. Este link-test verifica isso e **enumera a superfície de plataforma**
(a lista de TODO do backend Orbis), usando stubs weak no-op.

Ferramenta: [`tools/portability/link_engine.sh`](../tools/portability/README.md)
(código nosso, publicável). Entradas (objetos derivados), stubs gerados e o
binário ficam num **diretório privado** (`out/`), nunca no Git.

## Método

1. `llvm-nm` em todos os objetos → símbolos **definidos** × **indefinidos**.
2. **Externos** = indefinidos − definidos (o que o jogo referencia e nenhum
   objeto de jogo define: plataforma + símbolos de TUs ainda não-compilados).
3. Gera um stub **weak no-op** (`ret`) para cada externo.
4. Linka objetos + stubs + `main` dummy + libc com `ld.lld`.

## Resultado

| Item | Valor |
|---|---|
| Objetos de jogo linkados | **1.335** |
| Símbolos definidos (união) | 44.032 |
| Símbolos externos (stubados) | **2.681** |
| Binário gerado | `engine.elf`, **9,65 MB** |
| `.text` (código) | **6,23 MB** |
| Formato | ELF64 x86-64, dynamically linked |

O link **completa**: os 1.335 objetos compõem num executável real. Não é
emulação nem recompilação de binário — é o código de jogo decompilado virando
um binário x86-64.

## Superfície de plataforma (o TODO do backend Orbis)

Categorização dos 2.681 externos (heurística por namespace/prefixo):

| Grupo | ~qtd | O que é |
|---|---:|---|
| RVL_SDK (C) | ~248 | `OS` / `GX` / `VI` / `AX` / `DVD` / `PAD` / `MTX` / `NAND`… |
| JSystem | ~581 | framework: `J3D` / `JKR` / `JUT` / `JGeometry`… |
| nw4r + MSL + intrínsecos | resto | middleware, libc/MSL, intrínsecos PPC (`__fabs`, `__frsqrtef`) |
| símbolos de jogo faltantes | parte | classes dos 269 TUs que ainda não compilam |

Esses grupos de plataforma são exatamente o que o backend Orbis precisa
**implementar de verdade** (hoje são no-ops). O caminho: GX→GNM primeiro
(apoiado no renderer `SceVideoOut` da casca), depois áudio e input.

## O que falta para um eboot rodável (não só linkável)

1. **`-fPIC` nos objetos de jogo.** O eboot PIE do Orbis exige PIC; o harness de
   medição compilou sem. O link-test usou `-no-pie`. Recompilar com `-fPIC`.
2. **ODR.** Funções inline e alguns globais do RVL (`__OSBusClock`, `GXWGFifo`)
   são emitidos como símbolo **forte** em vários TUs → `duplicate symbol`. O
   link-test usou `--allow-multiple-definition`; o build real precisa de
   COMDAT/weak (visibilidade/linkage dos inlines).
3. **`main`.** O entry do jogo está num TU ainda não-compilado (usei dummy).
4. **Stubs → implementações.** Trocar os 2.681 no-ops pela camada Orbis real,
   começando pelos tiers 1–2 do [`PLATFORM-BOUNDARY`](PLATFORM-BOUNDARY.md).

## Limites

Linkar **não** é rodar. Este binário não roda (stubs no-op, non-PIE, main dummy,
sem backend gráfico). O marco prova **composição** e **dimensiona o backend** —
não comportamento, timing, endianness nem execução de RMGE01. As três medidas
seguem separadas: compila (✅ 1.335), **linka (✅ este doc)**, roda (❌ falta
backend).
