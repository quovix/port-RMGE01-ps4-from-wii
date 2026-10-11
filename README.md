# `[PORT][RMGE01][PS4<-WII]`

Porte **nativo** de um título de Wii (game ID **`RMGE01`**, NTSC-U) para o
PlayStation 4, a partir da decompilação da comunidade. Distribui **apenas
ferramenta e runtime** — nenhum asset, binário ou chave proprietária. Rodar exige
o **dump do seu próprio disco** (BYO-dump).

O motor derivado do decomp é compilado **localmente pelo usuário**. Source,
objetos e executáveis que contenham esse código não são artefatos públicos;
publicamos as ferramentas e alterações próprias necessárias ao porte.

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
5. **Publicação só do nosso patch/código.** Source do jogo, objetos derivados e
   diffs com contexto do decomp ficam locais. Hooks e CI auditam índice e
   histórico; ver [`docs/02-publicacao-apenas-codigo-proprio.md`](docs/02-publicacao-apenas-codigo-proprio.md).

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
- **Medição de portabilidade:** [`docs/01-medicao-portabilidade.md`](docs/01-medicao-portabilidade.md)
- **Referência de arquitetura (Dusklight/Aurora/Borealis):** [`docs/04-referencia-dusklight.md`](docs/04-referencia-dusklight.md)

## Status

| Fase | O quê | Estado |
|---|---|---|
| Ingest | dump RVZ → `main.dol` verificado (`tools/ingest`) | ✅ scaffold + extração via dtk |
| Decomp | buildar o Petari (local) | ✅ matching byte-perfect (arm64/Rosetta) |
| Verificação de mídia | SHA-1 + manifesto RMGE01 (`product/media`) | ✅ validado no host |
| Fase A — medição | 1.604 TUs via compile DB; host e x86-64 | ✅ baseline 141; com heap + matemática + friend privado, 1.335 objetos (83,23%); nenhum include ausente |
| Compatibilidade C++/ABI | alocadores, headers, asm PPC, extensões CodeWarrior | ⏳ patches de heap/matemática/friend disponíveis; 269 TUs ainda falham |
| Casca v0 | PKG com autotestes de heap e matemática | ✅ instalado, boot e dois autotestes confirmados no PS4; mídia ausente |
| Backend Orbis | GX→GNM, áudio, input | ⏳ após camada de compatibilidade |

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
docs/01-medicao-portabilidade.md  medição ampla e bloqueios verificados
docs/03-ponte-heap-e-pkg.md  patch de alocação, 852 TUs e casca reconstruída
docs/05-matematica-escalar-e-overlays.md  adapters próprios, 965 TUs e novo PKG
docs/07-validacao-de-boot-no-ps4.md  main, autotestes e loop confirmados em hardware
docs/08-compatibilidade-friend-em-nw4r.md  receita privada, 1.335 TUs em C++98
tools/ingest/           dump RVZ → main.dol + proveniência
tools/portability/      harness de compile DB (source e objetos ficam locais)
product/media/          verificação de mídia (SHA-1 próprio + manifesto)
runtime/compat/         ponte original de heap + autoteste host/Orbis
package/                casca v0 do PKG (eboot, build OpenOrbis)
```

## Licença

Apache-2.0 — ver [`LICENSE`](LICENSE). Aplica-se ao **código deste repositório**
(nosso). Não se estende a conteúdo de terceiros, que não é distribuído aqui.
