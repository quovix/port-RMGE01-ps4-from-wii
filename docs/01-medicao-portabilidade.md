# 01 — Medição de portabilidade do Petari para o porte Orbis

## Objetivo e premissa

Medir o código de jogo do decomp local com clang antes de implementar o backend
Orbis. O spike de `AnmPlayer.cpp` mostrou um caso favorável, mas não uma taxa
representativa. Esta medição de **11/10/2026** substitui aquela extrapolação.

> Este documento registra o **baseline sem patches**. A
> [ponte de heap implementada depois](03-ponte-heap-e-pkg.md) elevou a aprovação
> para 852 TUs e também foi incluída no PKG da casca.

O harness original está em [`tools/portability`](../tools/portability/README.md).
Nenhum source do Petari, dump ou objeto derivado foi adicionado ao repositório.

## Ambiente e método verificados

| Item | Valor |
|---|---|
| Petari local, commit | `e31bfcd828265e250e6d734108e7fde9dad465fb` |
| Compile DB, SHA-256 | `b385d240e6855183c4ae18bb47392d5ca98bed5b681aa4ef04eea1c4ac4c746d` |
| Entradas no compile DB | 2.178 |
| Subconjunto selecionado | 1.604 comandos de `src/Game` |
| Host | macOS arm64, Apple clang 21.0.0 |
| Compiler para x86-64 | Homebrew clang 18.1.8 |
| Target para x86-64 | `x86_64-pc-freebsd12-elf`, o mesmo triple da casca |
| Linguagem/includes/defines | por TU, preservados do compile DB do Petari |
| Headers/runtime na medição | inclui MSL original; ainda sem substituição por libc/libc++ |
| Flag de compatibilidade | `-fdelayed-template-parsing`, exceto perfil estrito |

Cada comando roda no `directory` do compile DB, via argv e sem shell. O harness
remove o target PPC e as saídas/ações originais, preserva `-nostdinc` e os demais
flags de análise e emite suas saídas apenas no diretório escolhido. Não foi
necessário criar includes fictícios, editar o Petari ou reconstruir seu build.

O conjunto é o do build configurado para RMGE01, não todos os arquivos físicos
nem todos os subsistemas de plataforma. Aprovação significa exit code zero;
warnings permanecem registrados. Há limite de 20 erros por TU.

## Resultados

| Perfil | TUs aprovados | Taxa | Includes ausentes | Timeouts |
|---|---:|---:|---:|---:|
| Host arm64, análise, templates adiados | 141 / 1.604 | 8,79% | 0 | 0 |
| x86-64, análise, templates adiados | 141 / 1.604 | 8,79% | 0 | 0 |
| x86-64, geração de objetos, templates adiados | 141 / 1.604 | 8,79% | 0 | 0 |
| x86-64, análise, parsing de templates estrito | 85 / 1.604 | 5,30% | 0 | 0 |

Os 141 TUs aprovados são os mesmos nos três primeiros perfis. Foram gerados
**141 objetos**; a inspeção do primeiro confirmou ELF64 relocatable, little-endian,
machine x86-64 e OS/ABI FreeBSD. Não houve link de motor, uso do SDK Orbis no
decomp nem execução desses objetos no console.

O flag de templates aumenta o conjunto aprovado de 85 para 141 TUs. Ele é útil,
mas não elimina os bloqueios compartilhados. O catálogo estrito também expõe
lookup de `get`/`set` em templates que o perfil adiado não analisa integralmente.

## Bloqueios catalogados

Contagens abaixo são de **TUs afetados** no perfil x86-64 com templates adiados;
um TU pode ter vários erros. As categorias não são uma partição do conjunto e
não estimam quantas falhas desaparecerão com uma única correção.

| Bloqueio observado | TUs | Origem observada no decomp local |
|---|---:|---|
| overload de `operator new` indisponível | 1.340 | `Functor.hpp` e outros callers de alocação |
| sintaxe de assembly CodeWarrior | 423 | principalmente `nw4r/math/arithmetic.h` |
| constraint PPC `=f` inválida para x86 | 422 | inline de conversão em `arithmetic.h`/RVL OS |
| typedef `Self` usado com class specifier | 418 | `nw4r/ut/LinkList.h` |
| builtin `__fabs` não declarado | 189 | `MSL_C/include/cmath` |
| membros não triviais em anonymous structs/unions | 94 | headers de Player; outros membros afetam 91 TUs |
| linkage C/C++ inconsistente | 57 | declarações de defaults de J3D TEV |
| overload de `operator new[]` indisponível | 53 | callers de alocação de arrays |

O erro em `Functor.hpp:129` aparece em **1.331 TUs**. Inspeção local mostrou que
os overloads de alocação com heap/alignment em `JKRHeap.hpp` estão protegidos por
`__MWERKS__`: as declarações não ficam visíveis ao clang. Esse é um ponto concreto
para iniciar a compatibilidade; não se deve simplesmente definir `__MWERKS__`
globalmente, pois ele também seleciona código específico do compiler original.

Há também uma diferença de ABI verificável: o header MSL seleciona `size_t`
como `unsigned int` fora de `__MWERKS__`, enquanto o target x86-64 usa tamanho de
ponteiro de 64 bits. Conversões de pointer para esse tipo falham em TUs como
`GameSequenceFunction.cpp`. Substituir MSL exige separar tipos do formato em
disco dos tipos nativos de memória; uma troca global de typedef não comprova
correção de offsets ou serialização.

Mesmo o TU aprovado do spike apresenta warnings de função sem retorno, acesso
fora do array e inline sem definição. Por isso **objeto gerado não significa
comportamento correto**. Endianness, layouts binários, paired singles, precisão
FP e link dos símbolos de plataforma ainda exigem validação própria.

## Estado e próximo marco

| Trabalho | Estado |
|---|---|
| Harness de compile DB com includes por TU | ✅ implementado e testado |
| Medição ampla host/x86-64 e objetos | ✅ executada |
| Comparação sem templates adiados | ✅ executada |
| Catálogo inicial de bloqueios compartilhados | ✅ baseado nos diagnósticos |
| Compatibilidade de alocação e headers | ⏳ próximo marco |
| Link de subconjunto com backend de heap/OS | ⏳ após compatibilidade |
| Execução do motor no PS4 | ❌ ainda não demonstrada |

O próximo marco é construir uma camada de compatibilidade local sem alterar o
checkout do decomp: tornar visíveis os overloads de heap/alignment com o tipo de
tamanho nativo e conectar sua implementação ao backend de heap. Depois, medir
novamente o conjunto e resolver os headers de matemática/nw4r sem assembly PPC.
As extensões de templates, unions e linkage precisam de análise separada para
decidir o que cabe em headers alternativos e o que exigirá patches locais.

Não há base para prometer prazo de jogabilidade a partir desta taxa. Ela mede
aceitação pelo compiler, não esforço linear: headers compartilhados concentram
falhas, e renderer, áudio e input continuam sendo etapas grandes. O objetivo de
manter `src/Game` intacto permanece uma hipótese a validar.

## Evidência e reprodução

Relatórios desta sessão, todos locais e gitignored:

```text
out/phase-a-host-syntax/         Apple clang, análise arm64
out/phase-a-x86-syntax/          clang 18, análise x86-64
out/phase-a-x86-object-final/    clang 18, objetos + contagens por TU/subsistema
out/phase-a-x86-strict/          clang 18, templates estritos
```

Cada diretório tem `report.json` e `results.jsonl`; o perfil de objetos também
tem `objects/`. Os primeiros dois relatórios precedem a adição das contagens por
TU/subsistema ao harness, mas contêm os diagnósticos individuais completos.
Os [comandos de reprodução e testes](../tools/portability/README.md) não dependem
de paths desta máquina. Os quatro testes de integração passaram.
