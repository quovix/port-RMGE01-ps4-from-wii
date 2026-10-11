# 05 — Matemática escalar, overlays privados e novo PKG

## Objetivo — 11/10/2026

Remover os bloqueios de assembly PPC nos headers FastCast/nw4r e a ausência de
`__fabs`, preservando o checkout privado do Petari. Foram escritos módulo
escalar, adapters de API e gerador de mapeamento próprios. Nenhum corpo de
função/header completo ou contexto de patch do jogo entrou no repo público.

## Referências consultadas e identificadas

Três arquivos foram recuperados pela API do GitHub e seu Git blob SHA-1 validado
contra os bytes recebidos. O conteúdo ficou somente em `out/`, gitignored.

| Arquivo | Git blob SHA-1 |
|---|---|
| Aurora `include/dolphin/os/OSFastCast.h` | `6c57394e588f7e4eed7b0817d482f5471a768c29` |
| Aurora `cmake/aurora_mtx.cmake` | `580a942ff6943df2f8d091e6a3079f775e01cb91` |
| Dusklight `src/dusk/memory.h` | `2d24e5b95773a0a92376220c5ab8a5f1ed86eb75` |

O [FastCast da Aurora](https://api.github.com/repos/encounter/aurora/git/blobs/6c57394e588f7e4eed7b0817d482f5471a768c29)
apresenta conversões nativas e inicialização sem GQR. O
[target MTX](https://api.github.com/repos/encounter/aurora/git/blobs/580a942ff6943df2f8d091e6a3079f775e01cb91)
mantém matemática em uma biblioteca separada. Isso orientou nossa separação de
responsabilidades, sem transplantar implementações. Casts simples fora da faixa
ou com NaN não foram adotados como oracle.

O [header de memória do Dusklight](https://api.github.com/repos/TwilitRealm/dusklight/git/blobs/2d24e5b95773a0a92376220c5ab8a5f1ed86eb75)
apresenta seleção de tamanho de heap por target e utilidades de alinhamento.
Não adotamos seus macros. Árvores consultadas: Aurora
`40cf48fce6ce85366608596bae018c6188f2b2e7` e Dusklight
`34a144a493b7f57adeb4034d382317c826eb5c8d` — **tree SHAs**, não commit SHAs.

A [implementação paired load/store do Dolphin](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/PowerPC/Interpreter/Interpreter_LoadStorePaired.cpp)
foi consultada para escala/clamp/truncamento, sem copiar código ou tomar seu
caso NaN como prova de hardware. A
[referência PPC math da Aurora](https://github.com/encounter/aurora/blob/main/include/dolphin/ppc_math.h)
mostra uma estimativa específica para `frsqrte`; não a substituímos por `1/sqrt`.

## Contratos implementados

[`scalar_math.c`](../runtime/compat/scalar_math.c) usa tipos nativos, verificações
de largura/mantissa e linkage C, sem incluir MSL ou headers do jogo.

| Operação | Contrato |
|---|---|
| u16 → f32 | exata para os 65.536 valores |
| f32 → u16 | truncamento na faixa e saturação em `[0, 65535]` |
| NaN → u16 | zero por política explícita; equivalência no Wii pendente |
| valor absoluto | builtin nativo; cobertura de zeros, infinities e quiet NaN |
| seleção escalar | condição não negativa, incluindo `-0`, escolhe o primeiro operando; NaN escolhe o segundo |
| aliases `__fabs`/`__fabsf` | implementação própria, visível antes dos headers MSL |

FPSCR, exceções FP do PPC, GQR dinâmico, escalas arbitrárias, paired singles em
geral, estimate instructions e endianness não são modelados neste perfil.
Precisão de funções MSL como `fabsl` e typedefs/layouts binários também continuam
pendentes. Compilar os callers não comprova equivalência de toda a matemática.
A casca usa `-fno-fast-math -ffp-contract=off`.

## Overlay e medição

[`create_overlay.py`](../tools/portability/create_overlay.py) gera VFS JSON do
clang: dois headers privados são redirecionados para adapters próprios e um
nome virtual expõe o módulo escalar ao include set RVL. O manifesto registra
paths/hashes, sem source privado ou unified diffs. O preinclude `math_profile.h`
torna os aliases visíveis antes de MSL e fixa seu linkage.

Paths canônicos precisam corresponder ao compile DB. Includes relativos a um
adapter podem seguir o diretório lógico do header privado; o alias virtual
explícito evita isso. Os testes compilam/linkam/executam os adapters C/C++ contra
headers sintéticos, verificando outputs volatile e preservação dos arquivos originais.
O checkout do Petari permaneceu sem alterações.

| Perfil x86-64, 1.604 TUs | Heap apenas | Heap + matemática | Includes ausentes | Timeouts |
|---|---:|---:|---:|---:|
| Análise | 852 | 965 (60,16%) | 0 | 0 |
| Geração de objetos | 852 | 965 (60,16%) | 0 | 0 |

Foram gerados 965 objetos com o mesmo conjunto aprovado nos dois perfis.
113 TUs foram destravados e nenhum dos 852 anteriores regrediu. Desapareceram
os erros de assembly dos headers substituídos e de `__fabs`. O motor ainda não
foi linkado/executado.

Restam 639 TUs com falha: entre os diagnósticos estão o class specifier aplicado
ao typedef `Self` em nw4r (419 TUs), unions/anonymous structs de Player (94/91),
linkage de defaults J3D (57), lookup `get`/`set` (43/27) e cast de pointer para
`size_t` MSL de 32 bits (20). Os grupos se sobrepõem.

Relatórios locais: `out/phase-a-math-syntax/`, `out/phase-a-math-object/`.
Overlay usado: `out/compat-math-v2/overlay.json` e seu manifesto. Ver
[comandos de reprodução](../tools/portability/README.md).

## Testes e PKG

O teste numérico passou com AddressSanitizer, UndefinedBehaviorSanitizer e
`float-cast-overflow`, `-O2 -Wall -Wextra -Werror`. Cobre todos os u16, frações,
saturação/truncamento nos quatro modos de rounding, zeros, infinities e quiet
NaNs. A conversão de NaN é validada como política própria, não medição do Wii.
Os nove testes de harness/overlays passaram; os fixtures são originais.

O PKG compila os cinco TUs C e dois C++ próprios definidos no Makefile. Os
adapters que incluem RVL são usados apenas na medição privada. Depois do teste
de heap, a casca executa o autoteste escalar e registra `math: autoteste OK` no
klog. Mídia e motor do jogo continuam externos.

| Evidência | Estado |
|---|---|
| Módulo escalar e adapters originais | ✅ |
| Testes numéricos com sanitizers e nove testes de harness/overlays | ✅ |
| 965 objetos x86-64 locais | ✅ |
| Compilação/link/generação do PKG | ✅ |
| Hashes/assinatura e extração do PKG | ✅ |
| Eboot e módulos extraídos idênticos aos inputs | ✅ |
| Código/assets/objetos do jogo no PKG | ❌ nenhum incluído |
| Semântica especial FP completa do Wii | ⏳ |
| Boot dos autotestes no PS4 | ✅ validado posteriormente; ver [boot](07-validacao-de-boot-no-ps4.md) |

O pacote local, gitignored, está em
`package/UP0001-QVOX00001_00-7E0C9848AC532465.pkg`: 6.619.136 bytes, SHA-256
`5d5cd9af9f593b5684c988de1b2634c06040d8d856792df3d3d4ce870f6e2181`.
Foi gerado em staging, validado e extraído antes de substituir o build anterior.
Nenhum commit, push, release ou instalação no console foi realizado.

> Estado ao terminar o build. A instalação foi realizada depois, por solicitação
> do usuário; ver [registro de instalação](06-instalacao-da-casca-no-ps4.md).

## Próximo marco

Compatibilizar os headers C++ compartilhados de nw4r e investigar linguagem/ABI
com patches locais, preservando a fronteira de publicação. Observar no PS4 os
logs dos dois autotestes. Matrizes e `frsqrte` precisam de um marco numérico
específico antes de considerar completo o backend matemático.
