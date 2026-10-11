# Arquitetura do pipeline — `[PORT][RMGE01][PS4<-WII]`

> **Proposta histórica de design** para lift/codegen PPC. O projeto adotou depois
> o porte do decomp local: a estratégia atual está no [README](README.md) e na
> [fronteira de plataforma](docs/PLATFORM-BOUNDARY.md). As seções de lift/codegen
> abaixo são uma alternativa de pesquisa, não um pipeline implementado. A
> [medição da Fase A](docs/01-medicao-portabilidade.md) descreve o estado verificado.
> Identidade do artefato: `asi:FR69GJ5CI:orbis:quovix/rmge01:recomp@1.2.0`.

Objetivo: pegar um **dump do próprio disco** (game ID `RMGE01`, Wii, CPU
PowerPC "Broadway") e produzir um executável **nativo** de PS4 (plataforma
"Orbis", x86-64) via **recompilação estática** — não emulação. O código do jogo
vira código x86-64 compilado ahead-of-time; só as chamadas de hardware/OS do Wii
são interceptadas e reimplementadas.

## Postura (cena limpa)

- **BYO-dump.** O repositório distribui **apenas ferramenta e runtime**. Nenhum
  asset, binário, chave ou `main.dol` da Nintendo é incluído ou redistribuído.
- **Sem chaves.** A partição do disco de Wii é criptografada (AES, common key).
  As chaves vêm do **dump/console do próprio usuário**, nunca do repo.
- **Sem marca.** Identificação é pelo **game ID** (`RMGE01`), não pelo nome
  comercial.
- **Proveniência obrigatória.** Todo código derivado referencia sua origem no
  binário (endereço no `main.dol`). Regra em [`docs/PROVENANCE.md`](docs/PROVENANCE.md).

## Visão geral do pipeline

```
      ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
      │  1. INGEST   │   │  2. LIFT     │   │  3. CODEGEN  │
 dump │  disco       │   │  front-end   │   │  back-end    │
 RMGE │  (BYO)       │──▶│  PPC/Broadway│──▶│  PPC → C/IR  │
 01   │  decrypt+FST │   │  disasm+CFG  │   │  → x86-64    │
      └──────┬───────┘   └──────────────┘   └──────┬───────┘
             │ assets (ficam locais, nunca no repo)│ .o recompilado
             ▼                                     ▼
      ┌─────────────────────────────┐   ┌────────────────────────┐
      │  4. RUNTIME / HLE (Orbis)    │   │  5. PACKAGE            │
      │  GX→GNM · AX→audio ·         │◀──│  link eboot.bin (ELF)  │
      │  WiiRemote→DualShock ·       │   │  → fpkg (OpenOrbis)    │
      │  IOS/OS · DI(disco)→FS local │   │  content ID derivado   │
      └──────────────┬──────────────┘   └───────────┬────────────┘
                     │                              │ .pkg (só código+shaders)
                     └──────────────┬───────────────┘
                                    ▼
                        ┌────────────────────────┐
                        │  6. EXEC no PS4          │
                        │  monta dump do usuário,  │
                        │  roda código nativo      │
                        └────────────────────────┘
```

---

## 1. Ingest — leitura do disco (BYO-dump)

**Entrada:** imagem do disco do usuário. Formatos aceitos: ISO/GCM, WBFS, RVZ/WIA,
NKit (normalizar tudo para um reader comum).

**Passos:**
1. Ler o header (offset 0) e confirmar game ID = `RMGE01`, região NTSC-U, revisão.
   Verificar integridade (hashes tipo Redump) — rejeitar dumps corrompidos cedo.
2. Montar a estrutura de partições do Wii; decriptar a partição DATA com a
   common key fornecida pelo usuário.
3. Extrair: **`main.dol`** (executável PPC), o **apploader**, a **FST** (árvore de
   arquivos) e os assets.

**Saída:** `main.dol` + FST + assets, **tudo local ao usuário**. Essa fronteira é
o que mantém o projeto limpo: nada daqui sobe pro repo ou pro PKG.

## 2. Lift — front-end PowerPC/Broadway

O `main.dol` é PowerPC 32-bit **big-endian**. Broadway = derivado do PowerPC 750CL
(linhagem Gekko), com extensões que importam muito:

- **Paired singles** (`ps_*`): SIMD de 2× float32 nos FPRs — mapeiam bem para
  SSE/AVX no x86-64, mas precisam de semântica exata.
- **GQRs** (graphics quantization registers): controlam quantização/dequantização
  dos load/store de paired single. Estado peculiar que o codegen tem que modelar.

**Passos:**
1. Parsear seções do DOL (text/data/bss), entry point, relocations.
2. Disassemblar, descobrir funções, construir CFGs.
3. Marcar os pontos difíceis: **branches indiretos** (jump tables, ponteiros de
   função, **vtables** — o jogo é C++ pesado), código auto-modificável, computed
   gotos.

Referência de abordagem: N64Recomp (lifta MIPS) e o recomp estático de Wii já
demonstrado publicamente (Mario Kart Wii) provam que PPC/Broadway → nativo é
viável. Avaliar reusar/adaptar um lifter de PPC existente antes de escrever do zero.

## 3. Codegen — back-end (PPC → nativo)

Emitir **C/C++ portável** (compilado por-alvo) ou **LLVM IR**. Para Orbis, compilar
com o toolchain clang-based do OpenOrbis para x86-64.

**Modelo de estado do PPC:** GPRs r0–r31, FPRs f0–f31 (64-bit, + metade superior do
paired single), CR, LR, CTR, XER, GQRs, FPSCR.

**Pontos de atenção:**
- **Endianness:** big-endian do Wii sobre little-endian x86-64 → modelo de memória
  BE (byteswap nos acessos ou região espelhada).
- **Memória:** MEM1 (24 MB) + MEM2 (64 MB); modelar o espaço de endereços; MMIO cai
  em HLE (estágio 4).
- **Paired singles → SSE/AVX**, respeitando NaN/arredondamento do Gekko.

**Saída:** objeto(s) recompilado(s) do jogo, linkáveis com o runtime.

## 4. Runtime / HLE — a camada Orbis

Onde o código recompilado chama "o hardware/OS do Wii". Cada subsistema é
interceptado e reimplementado nativamente. É aqui que mora a maior parte do risco.

| Subsistema Wii | Vira no PS4 | Dificuldade |
|---|---|---|
| **GX** (GPU Hollywood, pipeline TEV fixo) | **GNM/GNMX** — emular estágios TEV em shaders | ⚠️ a parte mais dura (research-grade) |
| **AX/DSP** (microcode de áudio) | mixer HLE → saída de áudio do PS4 | alta |
| **WiiRemote + Nunchuk** (ponteiro IR, motion) | **DualShock 4** (touchpad/giroscópio/sticks) | alta — é design de gameplay, não só técnico |
| **DI** (drive/disco) | redireciona pra FS local com o dump do usuário | média |
| **IOS/OS** (OSThread, OSAlloc, caches, IRQs) | threading + alocador nativos | média |
| **NAND/save** | save data do PS4 | baixa |

> **O ponteiro do WiiRemote é o desafio que define o port deste título
> específico** (mira/coleta dependem dele). Mapear pro DS4 é decisão de UX, não só
> de engenharia.

## 5. Package — montar o PKG Orbis

1. Linkar objeto recompilado + runtime + shaders em um **`eboot.bin`** (ELF Orbis).
2. Empacotar em **PKG** com `param.sfo` e o **content ID derivado** da ASIRN
   (`UP0001-QVOX00001_00-7E0C9848AC532465`; ver mapeamento no spec).
3. **Assinatura homebrew:** fpkg (fake-signed) via OpenOrbis, para consoles
   jailbroken.

> O PKG carrega **só código, shaders e metadados** — **nenhum asset do jogo**. Os
> assets são lidos em runtime do dump que o usuário instala separadamente no
> console (BYO-dump preservado até o fim).

## 6. Exec — rodando no PS4

O `eboot` sobe, o runtime monta os dados RMGE01 do usuário, o entry point
recompilado roda, chamadas GX vão pro GNM, input vem do DS4, áudio sai pelo PS4.
Como é código **nativo** (não emulado), não sofre o gargalo dos cores Jaguar que
inviabiliza emular Wii no PS4.

---

## Problemas difíceis (honestidade)

1. **GX/TEV → shaders** é o maior risco; é reimplementar um pipeline gráfico
   inteiro. Dolphin é referência de *semântica* (não de código a copiar).
2. **C++ pesado** → muitos branches indiretos/vtables; resolução incompleta
   trava o lift.
3. **Paired singles / FP** com bit-exatidão do Gekko.
4. **UX do ponteiro** WiiRemote no DualShock.
5. **Timing / sincronização** entre subsistemas HLE.

## Fases (incremental, de-risk primeiro)

1. **Reader de disco + extração** RMGE01 (verificável, BYO-dump).
2. **Lift+codegen de um `.dol` homebrew trivial → nativo em PC** (prova o core do
   recompilador sem o peso do PS4 nem do GX).
3. **Mesmo `.dol` → Orbis** (toolchain OpenOrbis, fpkg, "hello" no PS4).
4. **Runtime OS/IO HLE** + rodar código real do jogo sem gráficos.
5. **GX → GNM** incremental (primeiro clear/framebuffer, depois TEV).
6. **Áudio, input (DS4), save** → jogabilidade.

Cada fase é um marco testável antes de encarar a próxima.

## Layout de repositório (proposto)

```
tools/ingest/      reader de disco, decrypt, extração de FST
tools/lift/        front-end PPC: disasm, CFG
tools/codegen/     back-end: PPC → C/IR → x86-64
runtime/gx/        GX → GNM
runtime/audio/     AX/DSP → áudio PS4
runtime/input/     WiiRemote → DualShock
runtime/os/        IOS/OS, threading, alocador, FS
package/           eboot, param.sfo, fpkg (OpenOrbis)
docs/              este arquivo e design por subsistema
```

## Prior art / referências

- **N64Recomp / Zelda 64: Recompiled** — modelo de recomp estático (MIPS).
- **Recomp estático de Wii (Mario Kart Wii)** — prova que Broadway/PPC → nativo é viável.
- **OpenOrbis PS4 Toolchain** — compilar/empacotar/fpkg para Orbis.
- **Dolphin** — referência de *semântica* de GX/AX/Broadway (não de código).
- **[`quovix/asirn`](https://github.com/quovix/asirn)** — identificação, versionamento e mapeamento para git.
