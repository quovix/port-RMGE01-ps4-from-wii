# 08 — Compatibilidade de friend em nw4r mantendo C++98

## Problema e alteração — 11/10/2026

O perfil anterior falhava em 419 TUs porque um header compartilhado declarava
friend com um class specifier aplicado a um typedef. Desses TUs, 370 tinham
somente esse erro. Foi criada uma receita própria em
[`create_overlay.py`](../tools/portability/create_overlay.py) para usar o nome
injetado da classe envolvente nessa declaração.

O input/output são privados. Só a ferramenta original é publicada; o header
derivado fica **fora do repo público e fora do checkout Petari**, em diretório
novo escolhido por `--private-dir`. Não há unified diff ou contexto de source
do jogo no conteúdo público. O overlay aponta para o resultado local e registra
hashes; nunca descreve esse header derivado como código original nosso.

A receita valida que existem exatamente um owner esperado, seu typedef e uma
declaração friend correspondente. Inputs diferentes/ambíguos, destinos já
existentes e diretórios derivados dentro dos repos são rejeitados. O input não
é editado. Só a declaração muda; corpos de funções e campos permanecem intactos.

## Referências e semântica

A [configuração de ABI do Dusklight](https://github.com/TwilitRealm/dusklight/blob/main/cmake/GameABIConfig.cmake)
foi reconsultada por Git blob SHA-1
`ef3b5818dc43f29b01742b781bad87dc0fae2f85`, validado contra os bytes recebidos.
Ela é referência para centralizar o perfil de includes/defines. Nenhum flag
específico do outro jogo foi transplantado.

A regra de [declarações friend do C++](https://eel.is/c++draft/class.friend)
foi consultada. Neste patch foi preservado o padrão C++98 existente, usando o
nome da classe em vez de migrar globalmente o build para C++11/20. O teste
sintético verifica que a classe externa continua acessando constructor e campo
privados da classe interna, com `-std=c++98 -pedantic-errors`.

O manifesto desta execução registra:

| Input/output privado | SHA-256 |
|---|---|
| Header original | `ce69dcd759bf1817dd681d8ca468fccfbfd72e3e8d3e3a3c48123bd1735a8e46` |
| Header derivado | `8b17e454557eaf66f711ba46074e8f14dc6de02f905a215a939f5e752873a5b6` |

O tamanho mudou de 12.544 para 12.548 bytes. A mudança não pretende corrigir
outros problemas de layout/ABI ou semântica de containers; remove esse bloqueio
de declaração específico.

## Resultados verificados

Mesmo compile DB, compiler, target x86-64 e C++98 do marco de matemática:

| Perfil, 1.604 TUs | Antes | Depois | Includes ausentes | Timeouts |
|---|---:|---:|---:|---:|
| Análise | 965 (60,16%) | 1.335 (83,23%) | 0 | 0 |
| Geração de objetos | 965 | 1.335 | 0 | 0 |

Foram gerados **1.335 objetos** privados. O conjunto aprovado é igual nos dois
perfis: 370 ganhos e zero regressões dos 965 anteriores. O erro de friend/typedef
desapareceu. Restam 269 TUs com falha, principalmente unions/anonymous structs,
linkage J3D, lookup em templates e tipos MSL incompatíveis com pointer de 64 bits.
As taxas medem compilação individual, não percentual do port concluído.

Relatórios/manifestos locais: `out/compat-friend/`, `out/phase-a-friend-syntax/`
e `out/phase-a-friend-object/`. O header derivado desta execução ficou em
`/private/tmp/qvx-rmge01-friend-20261011/LinkList.h`. O Petari permaneceu sem
alterações. A [documentação do harness](../tools/portability/README.md) contém
comandos portáveis de reprodução.

## Validação e limites

| Evidência | Estado |
|---|---|
| Fixture falha antes do patch e compila/executa depois em C++98 estrito | ✅ |
| Acesso privado preservado no fixture | ✅ |
| Guardas de origem/destino e input ambíguo | ✅ |
| Treze testes do harness/overlays | ✅ |
| Auditoria de conteúdo público com comparação ao Petari | ✅ |
| 1.335 objetos locais | ✅ |
| Link/execução do motor do jogo | ❌ ainda não demonstrados |

Não houve alteração executável da casca, portanto seu PKG permanece o build já
instalado e [validado em hardware](07-validacao-de-boot-no-ps4.md). Esse novo patch
é consumido pelo build privado do motor; seus objetos não entram na casca nem
em releases públicas. Nenhum commit, push ou publicação foi realizado.

Próximo marco: resolver os blocos restantes de linguagem/ABI, mantendo medidas
separadas de aceitação pelo compiler, link e comportamento no hardware.
