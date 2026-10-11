# 04 — Dusklight como referência de arquitetura do porte

## Objetivo e escopo da consulta

O usuário indicou [TwilitRealm/dusklight](https://github.com/TwilitRealm/dusklight)
como referência permanente de arquitetura e conhecimento. Consulta inicial em
**11/10/2026**, por leitura de source/documentação pública, sem executar o
projeto, clonar seu código ou incorporá-lo ao nosso repo/PKG.

O [README](https://github.com/TwilitRealm/dusklight/blob/main/README.md) descreve
um porte baseado no decomp de Twilight Princess e exige mídia do próprio usuário.
Sua base matching é GameCube; usar um disco Wii não transforma o motor em uma
base Wii. Portanto, a superfície de plataforma não deve ser tomada como idêntica
à do RMGE01/Petari. A consulta não altera nossa política de publicar só código
próprio.

As fontes consultadas apontam para `main`; não foi fixado um commit único nesta
consulta e o navegador pode servir snapshots de datas diferentes. A análise é
uma orientação inicial. Antes de implementar uma decisão, revalidar e registrar
o SHA dos arquivos relevantes. Não foram demonstrados backend Orbis, desempenho
no PS4 ou equivalência de semântica para o nosso jogo.

## Separação de responsabilidades verificada

O [build do Dusklight](https://github.com/TwilitRealm/dusklight/blob/main/CMakeLists.txt)
separa código interno do porte de arquivos do jogo e bibliotecas JSystem. Ele
liga módulos de compatibilidade distintos e serviços do aplicativo, usa C++20
e dispõe de opção de AddressSanitizer. Isso é evidência da estrutura do build;
não prova a cronologia de desenvolvimento por uma estratégia de casca primeiro.

Os [submodules declarados](https://github.com/TwilitRealm/dusklight/blob/main/.gitmodules)
identificam **Aurora** e **Borealis** como projetos separados.

| Camada observada | Responsabilidade | Aplicação proposta aqui |
|---|---|---|
| Motor/decomp e JSystem | código específico do título no build do Dusklight | Petari privado e build local, fora do repo público |
| Aurora | compatibilidade GameCube/Wii no nível do source | contratos próprios da fronteira RVL→Orbis |
| Borealis | serviços compartilhados do aplicativo | separar import, paths, config e logs do motor |
| Código interno do porte | integração e adaptações específicas | nossa casca e adapters de RMGE01 |

**Proposta nossa:** manter interfaces pequenas por subsistema e targets de build
separados. Dependências e flags do motor privado não devem contaminar a casca
publicável. A separação já praticada pelo linker explícito do nosso PKG é um
ponto de partida, não uma reprodução integral do build deles.

## ABI, alocação e estado nativo

[`GameABIConfig.cmake`](https://github.com/TwilitRealm/dusklight/blob/main/cmake/GameABIConfig.cmake)
centraliza includes e defines compartilhados entre motor e SDK de mods. Há
opções explícitas para target PC, UB e compatibilidade debug/release. Ele também
força um header comum no target que precisa dessa ordem de inclusão.

**Aplicação proposta:** centralizar nosso perfil de ABI do motor privado e
registrar tamanho de pointer, `size_t`, alinhamento, signedness e flags por TU.
Nossa ponte de heap já usa um preinclude próprio e tamanho nativo; isso não
resolve layouts de recursos em disco, serialização ou vtables automaticamente.
Avaliar padrões de linguagem mais novos como experimentos separados, com nova
medição, preservando o baseline C++98. Não transplantar defines específicos de
Twilight Princess para o Petari.

A [implementação consultada de OSThread](https://github.com/TwilitRealm/dusklight/blob/main/src/dusk/OSThread.cpp)
usa estado auxiliar para threads nativas, em vez de colocar diretamente os
objetos de sincronização do host na estrutura acessada pelo jogo. O arquivo
consultado também apresenta limites de suspensão de outras threads e operações
não implementadas; não deve ser tratado como reprodução completa do scheduler.

**Aplicação proposta:** estudar side tables para ownership de allocations e
handles de threads no backend próprio. No heap, isso ajuda a formular o próximo
contrato de `delete` comum, que nossa ponte ainda não implementa. No OS, escrever
testes de sleep/wakeup, TLS, reentrada, join e ausência de lost wakeups antes de
executar código do motor. A estrutura de threads do Wii e seu timing ainda
precisam de validação específica.

## Disco, validação e dados do aplicativo

[`iso_validate.cpp`](https://github.com/TwilitRealm/dusklight/blob/main/src/dusk/iso_validate.cpp)
usa um catálogo de mídia com game IDs, revisões e hashes de 128 bits, delega a
verificação ao serviço de disco do Borealis e traduz resultados para erros do
produto. Inspeção de mídia e verificação são operações diferentes.

**Aplicação proposta:** preservar essa distinção na nossa casca. Hoje ela
verifica apenas o SHA-1 de `sys/main.dol`; isso identifica o executável esperado,
mas não verifica todos os assets ou a imagem inteira. Uma importação completa
deve validar a mídia/versão, manter estados de progresso/cancelamento/erro e
liberar o mount do motor somente após sucesso. O catálogo será próprio de
RMGE01, não uma cópia dos IDs ou hashes do outro jogo.

[`data.cpp`](https://github.com/TwilitRealm/dusklight/blob/main/src/dusk/data.cpp)
encapsula inicialização de paths, cache, localização customizada/portável e
status de migração por meio de um manager. O projeto
[Borealis](https://github.com/encounter/borealis) fornece serviços compartilhados
para ports baseados em Aurora.

**Aplicação proposta:** separar mídia importada, cache descartável, configurações
e saves. No PS4, um path temporário de USB deve ser distinguido do storage local
do aplicativo. Import interrompido não pode deixar um estado aparentemente
pronto; usar staging e um registro próprio de conclusão validado. Essa é uma
decisão de design a implementar, não comportamento já presente na nossa casca.

## GX e renderer — referência útil, adaptação ainda aberta

A [documentação da Aurora](https://github.com/encounter/aurora/blob/main/README.md)
descreve uma camada source-level GameCube/Wii, com GX sobre WebGPU/Dawn e APIs
gráficas D3D12, Vulkan e Metal. Também descreve módulos de PAD, DVD e CARD.
Essa lista de plataformas/APIs não demonstra execução no PS4.

O [target GX](https://github.com/encounter/aurora/blob/main/cmake/aurora_gx.cmake)
lista separadamente processamento de comandos/display lists/registradores,
pipeline/shader e recursos, conversões de texturas, cache e trabalho de render.
Sua dependência direta de Dawn confirma que o backend gráfico exige essa camada.

**Aplicação proposta:** organizar nosso renderer com uma representação do estado
GX separada da emissão de comandos Orbis. Começar por clear/framebuffer e
geometria simples, então formatos de textura, TEV e caches. Fixtures originais
devem verificar o estado interpretado e resultados pequenos antes de carregar
assets do usuário. A escolha entre uma camada intermediária e emissão GNM ainda
exige um spike próprio; esta consulta não justifica prometer Aurora pronta no PS4.

Input exige análise adicional: RMGE01 usa WiiRemote/Nunchuk e ponteiro IR. Um
modelo orientado a PAD/GameCube não demonstra solução para essa interação.

## Reuso e próxima ordem de consulta

O repo do Dusklight publica [CC0](https://github.com/TwilitRealm/dusklight/blob/main/LICENSE.md)
e o README da Aurora declara MIT. Nesta tarefa o aproveitamento foi de arquitetura
e conhecimento, sem importar source, headers, submodules, código de jogo ou
dependências. A regra do usuário sobre código próprio continua sendo o critério
de publicação; uma licença exibida pelo upstream não substitui essa regra.

| Etapa de referência | Estado |
|---|---|
| Identificar arquitetura e dependências pelo build | ✅ |
| Consultar ABI, validação de disco e paths | ✅ |
| Consultar organização de GX e estado auxiliar de threads | ✅ leitura estática inicial |
| Importar código externo para este repo/PKG | ❌ não realizado |
| Fixar SHAs antes de aplicar decisões a cada subsistema | ⏳ |
| Avaliar headers/matemática e ownership do heap | ⏳ próximo bloco de consulta |
| Implementar/testar renderer Orbis e input Wii | ⏳ |

Antes do próximo patch de matemática, consultar o módulo MTX/headers relevantes
da Aurora e comparar semântica numérica com a API usada pelo Petari. O arquivo
`aurora_mtx.cmake` e `src/dusk/memory.h` não foram obtidos nesta consulta; nenhuma
conclusão específica sobre seu conteúdo foi adotada. Depois, aprofundar OS/heap
e o mount dos dados. Consultar GX por componente quando houver a base de render
Orbis, preservando nosso ciclo host→PKG→hardware por marco funcional.

## Consulta seguinte com arquivos fixados por hash

O [marco de matemática](05-matematica-escalar-e-overlays.md) recuperou os arquivos
antes indisponíveis via API do GitHub, validando o SHA Git de cada blob: FastCast
e target MTX da Aurora e `src/dusk/memory.h` do Dusklight. Os hashes e decisões
estão registrados naquele documento. A indisponibilidade acima vale para esta
consulta inicial; não é um bloqueio atual.
