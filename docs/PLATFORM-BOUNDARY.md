# Fronteira de plataforma — Wii (RVL) → PS4 (Orbis)

> Análise para o porte via decomp. **O source do decomp não vive aqui** (fica
> local; ver política de cena limpa). Este documento descreve *quais* camadas
> precisam de um backend Orbis e *qual* a interface — é código/análise nossos.

## Modelo em três camadas

```
┌──────────────────────────────────────────────┐
│  CÓDIGO DE JOGO  src/Game/* + framework JSystem│  ← intacto (não mexer)
├──────────────────────────────────────────────┤
│  API de plataforma Wii  (RVL_SDK, nw4r, …)     │  ← a fronteira
├──────────────────────────────────────────────┤
│  BACKEND  Wii real  ──►  nosso backend ORBIS   │  ← o que a gente escreve (público, Apache-2.0)
└──────────────────────────────────────────────┘
```

**Estratégia:** reimplementar a **superfície de API** que o jogo chama (RVL_SDK /
nw4r / libc) com um **backend Orbis**, deixando `src/Game/*` e `JSystem`
inalterados. O jogo não sabe que mudou de hardware — só o backend muda.

Referência permanente indicada pelo usuário: [Dusklight/Aurora/Borealis](04-referencia-dusklight.md).
Consultar os contratos por subsistema antes de implementar. Seu uso de
WebGPU/Dawn não demonstra um renderer Orbis pronto; a adaptação é um marco
próprio. Manter source do jogo intacto continua sendo um objetivo a validar.

## RVL_SDK — 36 subsistemas, por tier

### Tier 1 — boot + render (obrigatório, difícil)
| Subsistema | O que é | Vira no Orbis |
|---|---|---|
| **gx** | pipeline gráfico (Flipper/Hollywood, TEV) | **GNM/GNMX** — emular estágios TEV em shaders ⚠️ research-grade |
| **gd** | montagem de display lists do GX | parte do backend GX |
| **vi** | video interface / framebuffer / display | video out do Orbis |
| **os** | threads, interrupts, tempo, cache, alloc base | pthreads/kernel Orbis |
| **mem** | heaps (expand/frame/unit heaps) | alocador nativo |

### Tier 2 — jogabilidade (obrigatório)
| Subsistema | O que é | Vira no Orbis |
|---|---|---|
| **wpad / kpad** | WiiRemote + Nunchuk (inclui **ponteiro IR**) | **DualShock 4** (touchpad/giro/sticks) — decisão de UX |
| **pad / si** | GameCube pad / serial interface | mapeável ao DS4 (fallback) |
| **ax / axfx / dsp / ai** | mixer de áudio, efeitos, microcode DSP, audio interface | mixer HLE → áudio PS4 |
| **dvd** | I/O do disco | FS local com o dump do usuário |
| **fs / nand / vf** | filesystem / NAND / virtual file | save data + FS do PS4 |

### Tier 3 — formatos/matemática (em boa parte portável)
| Subsistema | O que é | Ação |
|---|---|---|
| **mtx** | matrizes/vetores 3D (CPU) | portável — manter/reimplementar sem HW |
| **arc** | archives (RARC/U8) | loader de dados — portável |
| **tpl** | texturas (TPL) | decodificar p/ formato do GNM |
| **thp** | vídeo (codec THP) | decode p/ cutscenes |

### Tier 4 — stub / baixa prioridade (single-player)
`net`, `nwc24` (WiFi Connection 24), `bte`/`wud` (stack Bluetooth do WiiRemote —
**substituído pelo DS4**), `usb`, `exi`, `ipc` (ponte p/ IOS — substituída pelos
nossos serviços nativos), `rso` (módulos dinâmicos), `sc` (system config), `db`,
`esp`, `euart`, `aralt`, `wenc`, `bte`. → stub/mock no começo.

## nw4r (middleware sobre o GX)
| Módulo | O que é | Ação |
|---|---|---|
| **lyt** | layout/UI 2D | roda sobre o backend GX |
| **math** | matemática | portável |
| **ut** | utilidades | em boa parte portável |
| **db** | debug | stub |

## Toolchain / libs Metrowerks (trocar, não portar)
- **MSL_C / MSL_C++** (libc/libc++ da Metrowerks) → **libc/libc++ do clang** (Orbis).
- **Runtime** (runtime do CodeWarrior) → runtime do clang.
- **MetroTRK** (stub de debugger) → **dropar**.

## Esforço relativo (nº de arquivos no decomp, referência de tamanho)
`RVL_SDK` ~240 · `nw4r` ~44 · `MSL_C` ~36 · `MetroTRK` ~33 (drop) · `RVLFaceLib`
~29 (Mii) · `Runtime` ~5 · `src/Game` ~1892 (intacto). O grosso do *nosso*
trabalho é reimplementar os tiers 1–2 do RVL_SDK como backend Orbis; o tier 4 é
stub; `src/Game` não se toca.

## Fase A — portabilidade medida
Antes de escrever qualquer backend: **o source do jogo compila com clang pra
x86-64?** O decomp é escrito pra CodeWarrior/PPC (pragmas, big-endian, headers
Metrowerks). A fase A compila um subconjunto com clang no host e cataloga a
parede de portabilidade (pragmas CW, assunções big-endian, intrínsecos PPC,
headers MSL). Isso dimensiona o trabalho real **antes** de investir no backend.

A [medição de 11/10/2026](01-medicao-portabilidade.md) cobre 1.604 TUs do compile
DB: 141 passam e geram objetos x86-64. Os demais exigem compatibilidade de
headers/linguagem/ABI antes do backend. Manter `src/Game` intacto é um objetivo;
a viabilidade completa ainda não foi demonstrada.

Alvo de teste de hardware: **PS4 com ps4debug**. A casca permite testar boot cedo;
os objetos da Fase A ainda não constituem um motor linkado nem executável.

## Cena limpa
- Este doc e o backend Orbis são **código/análise originais** (público, Apache-2.0).
- O **source do decomp (Petari) nunca entra aqui** — fica local, BYO.
- Assets do jogo **nunca** são redistribuídos; vêm do dump do usuário em runtime.
