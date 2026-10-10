# `[PORT][RMGE01][PS4<-WII]`

Porte **nativo** de um título de Wii (game ID **`RMGE01`**, NTSC-U) para o
PlayStation 4, a partir da decompilação da comunidade. Distribui **apenas
ferramenta e runtime** — nenhum asset, binário ou chave proprietária. Rodar exige
o **dump do seu próprio disco** (BYO-dump).

| | |
|---|---|
| **Identidade (ASIRN)** | `asi:FR69GJ5CI:orbis:quovix/rmge01:recomp@1.2.0` |
| **Forma curta (ASIC)** | `asi:FR69GJ5CI` |
| **Alvo** | `orbis` (PlayStation 4, homebrew / OpenOrbis) |
| **Origem** | Wii (PowerPC "Broadway"), game ID `RMGE01` |
| **Método** | porte do decomp (source reconstruído → backend Orbis) |
| **Padrão de identificação** | [`quovix/asirn`](https://github.com/quovix/asirn) |
| **Licença** | Apache-2.0 |

> ⚠️ **Não contém o jogo.** Este repositório não inclui nem redistribui nenhum
> conteúdo da Nintendo. O produto final **verifica a mídia** do usuário em runtime
> e importa os assets do dump do próprio dono do disco.

## Cena limpa (a regra que rege tudo)

1. **BYO-dump.** Nada do jogo entra no repo nem no PKG. O usuário fornece o dump
   do próprio disco; o produto verifica (game ID + hash) antes de rodar.
2. **O decomp não vive aqui.** O source reconstruído da Nintendo
   ([SMGCommunity/Petari](https://github.com/SMGCommunity/Petari)) fica **local**,
   nunca é commitado. Este repo tem só **código nosso** (o backend Orbis e a casca
   do produto) + documentação.
3. **Proveniência obrigatória.** Todo código derivado referencia sua origem no
   binário; código original marca `@orig none`. Ver [`docs/PROVENANCE.md`](docs/PROVENANCE.md).
4. **Sem marca.** Identificação pelo **game ID**, não pelo nome comercial.

## Como funciona

O caminho não é emular nem recompilar binário: é **portar o source do decomp** —
manter o código de jogo intacto e trocar a camada de plataforma Wii por uma
**Orbis**.

```
dump RVZ (BYO) ──▶ decomp (Petari, local) ──▶ source de jogo (intacto)
                                                     │
                       camada de plataforma Wii ◀────┤
                       (RVL_SDK, nw4r, …)            │
                              │ reimplementar         ▼
                              ▼                 produto (casca)
                       BACKEND ORBIS      boot · import+verify · config
                       GX→GNM, áudio,     · mount do motor
                       input→DualShock         │
                                                ▼
                                         PKG fpkg  ──▶  PS4 (ps4debug)
```

- **Fronteira de plataforma:** [`docs/PLATFORM-BOUNDARY.md`](docs/PLATFORM-BOUNDARY.md)
- **Pipeline técnico:** [`ARCHITECTURE.md`](ARCHITECTURE.md)
- **Produto (casca-primeiro, estilo Dusk):** [`docs/PRODUCT.md`](docs/PRODUCT.md)

## Status

| Fase | O quê | Estado |
|---|---|---|
| Ingest | dump RVZ → `main.dol` verificado (`tools/ingest`) | ✅ scaffold + extração via dtk |
| Decomp | buildar o Petari (local) | ✅ matching byte-perfect (arm64/Rosetta) |
| Verificação de mídia | SHA-1 + manifesto RMGE01 (`product/media`) | ✅ validado no host |
| Fase A | source de jogo compila com clang (host) | ✅ 1 TU limpo (`-fdelayed-template-parsing`); medição ampla pendente |
| Casca v0 | PKG mínimo que boota no PS4 | 🔨 em andamento (toolchain OpenOrbis) |
| Backend Orbis | GX→GNM, áudio, input | ⬜ próximo grande bloco |

## Requisitos

- **Seu próprio dump** do disco `RMGE01` (RVZ/ISO/WBFS).
- **PS4 jailbroken** com **ps4debug** ativo (teste em hardware real).
- **Build:** OpenOrbis PS4 Toolchain + LLVM; em macOS arm64 via Rosetta.

## Build (resumo)

```sh
# verificação de mídia (host) — prova o módulo contra seu main.dol
clang -I product/media -o verify_test \
  product/host_test/verify_test.c product/media/media_verify.c product/media/sha1.c
./verify_test <caminho>/sys/main.dol

# PKG da casca (requer OO_PS4_TOOLCHAIN) — ver package/
```

## Estrutura

```
ARCHITECTURE.md        pipeline técnico (disco→port→backend Orbis→PKG)
docs/PLATFORM-BOUNDARY  fronteira Wii(RVL)→Orbis, por tiers
docs/PRODUCT.md         produto casca-primeiro (import/verify/config)
docs/PROVENANCE.md      regra de proveniência obrigatória
docs/PHASE-A-SPIKE.md   resultados do spike de portabilidade (clang)
tools/ingest/           dump RVZ → main.dol + proveniência
product/media/          verificação de mídia (SHA-1 próprio + manifesto)
package/                casca v0 do PKG (eboot, build OpenOrbis)
```

## Licença

Apache-2.0 — ver [`LICENSE`](LICENSE). Aplica-se ao **código deste repositório**
(nosso). Não se estende a conteúdo de terceiros, que não é distribuído aqui.
