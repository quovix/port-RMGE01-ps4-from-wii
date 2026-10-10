# [PORT][RMGE01][PS4&lt;-WII]

Artefato de **recompilação** do padrão [ASIRN/ASIC](https://github.com/quovix/asirn):
porte nativo de um título de Wii (game ID **RMGE01**) para PS4.

| | |
|---|---|
| **Título de exibição** | `[PORT][RMGE01][PS4<-WII]` |
| **ASIRN** | `asi:FR69GJ5CI:orbis:quovix/rmge01:recomp@1.2.0` |
| **Forma curta (ASIC)** | `asi:FR69GJ5CI` |
| **Game ID** | `RMGE01` (NTSC-U, origem Wii) |
| **Alvo** | `orbis` (PlayStation 4) |
| **Método** | `recomp` (recompilação estática → código nativo) |
| **Branch** | `recomp` (canais em `recomp~<canal>`) |
| **Versão** | tag `v1.2.0` |
| **PKG content ID** | `UP0001-QVOX00001_00-7E0C9848AC532465` |

## Status

Scaffolding. Ainda não há pipeline de recompilação implementado — este repo é a
casa do artefato, nomeado pela convenção ASIRN → git (repo descritivo
`port-<GAMEID>-<console>-from-<source>`; método = branch; versão = tag).

## Conteúdo e requisitos (BYO-dump)

Este repositório distribui **apenas ferramenta e runtime**. Nenhum asset ou
binário proprietário é incluído, e o nome comercial do jogo não aparece aqui — a
identificação é pelo **game ID** do disco. A execução exige o **dump do próprio
disco do usuário** (bring-your-own-dump); nada de terceiros é redistribuído.

## Padrão

Identificação, versionamento e mapeamento para git seguem
[`quovix/asirn`](https://github.com/quovix/asirn).
