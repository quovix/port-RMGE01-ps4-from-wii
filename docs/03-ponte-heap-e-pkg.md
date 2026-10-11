# 03 — Ponte de heap e novo PKG da casca

> Registro do marco de heap. O [marco seguinte](05-matematica-escalar-e-overlays.md)
> adicionou matemática/overlays, elevou a compilação para 965 TUs e substituiu
> o PKG local por um build com ambos os autotestes. O hash abaixo identifica
> o pacote anterior, não o arquivo atual.

## Alteração original — 11/10/2026

[`runtime/compat/heap_new.hpp`](../runtime/compat/heap_new.hpp) declara overloads
de alocação com o tipo de tamanho nativo do compiler, `__SIZE_TYPE__`, e heap
opaco. O header é injetado com `--preinclude` pelo harness; não foi necessário
editar o Petari ou habilitar `__MWERKS__`.

[`heap_new.cpp`](../runtime/compat/heap_new.cpp) implementa dispatch para callbacks
próprios de allocate/release, preservando tamanho, owner e alinhamento. Sem
backend, ou se allocation falhar, o processo aborta. Não há fallback silencioso
de um heap solicitado para malloc. Os overloads de delete com argumentos extras
fazem cleanup quando um constructor lança.

O backend precisa ser registrado no startup, sem allocations/operações em
andamento. Não há sincronização de reconfiguração, registry de ownership nem
implementação do `delete` comum do motor neste marco. Objetos vivos nos testes
são destruídos e liberados explicitamente pela ponte. O adapter real de JKRHeap
e o tratamento de alignment negativo continuam pendentes; o valor negativo é
preservado no dispatch e rejeitado pelo backend de teste nativo da casca.

Todo o código publicado é original. A superfície de API é de compatibilidade;
nenhuma implementação, classe completa ou corpo de função do jogo foi copiado.

## Medição no decomp privado

Mesmo compile DB/commit/compiler/target do
[baseline](01-medicao-portabilidade.md), com o header próprio pré-incluído:

| Perfil | Baseline | Com ponte | Includes ausentes | Timeouts |
|---|---:|---:|---:|---:|
| x86-64, análise | 141 / 1.604 | 852 / 1.604 (53,12%) | 0 | 0 |
| x86-64, geração de objetos | 141 / 1.604 | 852 / 1.604 (53,12%) | 0 | 0 |

Foram gerados 852 objetos locais. O checker não encontrou mais os erros de
overload de `operator new`/`new[]`. A etapa comprova compilação individual;
o motor completo ainda não foi linkado ou executado. Os 752 TUs restantes
encontram principalmente assembly PPC, sintaxe de headers nw4r, builtin `__fabs`,
unions não triviais e diferenças de ABI/linkage.

Evidência privada e gitignored em `out/phase-a-heap-syntax/` e
`out/phase-a-heap-object/`: relatórios, argv, diagnósticos e objetos. O checkout
do Petari permaneceu sem alterações. Nenhum desses objetos entrou no PKG.

## Validação do código próprio

O teste [`product/host_test/heap_test.cpp`](../product/host_test/heap_test.cpp)
passou com AddressSanitizer e UndefinedBehaviorSanitizer, `-Wall -Wextra -Werror`.
Exercita os seis overloads, propagação de owner/alignment, size acima de 32 bits,
callbacks incompletos, cleanup de scalar e array quando constructors lançam e
destruição dos elementos já construídos. Todos os fixtures são originais.

```sh
mkdir -p out/host-test
clang++ -std=c++98 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  runtime/compat/heap_new.cpp runtime/compat/heap_smoke.cpp \
  product/host_test/heap_test.cpp -o out/host-test/heap_test
out/host-test/heap_test
```

Os cinco testes do harness, incluindo `--preinclude`, passaram. Os seis testes
da proteção de publicação também passaram, e a auditoria com o Petari privado
não encontrou coincidência literal longa no código publicável.

## PKG local da casca

O Makefile compila somente os três TUs C próprios e os dois TUs C++ da ponte e
autoteste. O linker usa uma lista explícita de objetos; não incorpora `.o`
arbitrários de `package/build`. Os módulos locais também são listados
explicitamente. Dependências de headers usam `-MMD -MP`.

Build local:

```sh
make -C package OO_PS4_TOOLCHAIN=/caminho/PS4Toolchain
```

O empacotador precisou executar fora da sandbox: a API criptográfica do macOS
falhou com CSSM/AppleCrypto dentro dela. O Makefile agora usa staging do PKG e
só substitui o pacote final após sucesso; `.DELETE_ON_ERROR` remove alvos
incompletos. As dependências da toolchain continuam externas e não commitadas.

O boot da casca chama `qvx_heap_smoke_test` antes da verificação de mídia. O
autoteste usa apenas objetos originais e owner NULL, verifica tamanho nativo,
scalar/array e alinhamentos de 64/128 bytes, libera tudo e desliga a ponte.
Seu resultado vai ao klog; ainda falta observá-lo em hardware.

| Evidência do PKG | Estado |
|---|---|
| Compilação e link OpenOrbis da ponte/casca | ✅ |
| Geração do PKG | ✅ 6.619.136 bytes |
| Hashes e assinatura (`pkg_validate`) | ✅ |
| `eboot.bin` extraído idêntico ao input | ✅ |
| Três módulos extraídos idênticos aos inputs e à toolchain local | ✅ |
| Conteúdo interno PFS | ✅ eboot, três módulos e keystone gerado |
| Metadados/arte no GP4 | ✅ param.sfo e ícone original; hashes validados no PKG |
| Jogo, assets ou objetos do decomp no pacote | ❌ nenhum incluído |
| Boot/autoteste no PS4 | ⏳ pendente de hardware |

O PKG local está em
`package/UP0001-QVOX00001_00-7E0C9848AC532465.pkg`, gitignored, SHA-256:
`1806f77d178ed46e64de38162d1d2f7ad7e56a7214236e8739690ad86d57ba17`.
Ele contém a casca própria e dependências de runtime da toolchain, sem motor.
Nenhum commit, push, release ou instalação no console foi realizado.

## Próximo marco

Tratar os headers de matemática PPC/nw4r com uma camada própria e medir de novo.
Em paralelo ao próximo build, validar no PS4 o log `heap: autoteste OK` desta
casca. Adapter de heap do motor, ownership de delete e ABI/endianness têm
validações específicas; os 852 objetos não demonstram essas propriedades.
