# Produto final — como vamos construir

Estratégia: **casca primeiro, motor atrás.** A gente constrói agora o produto que
dá boot no PS4 e faz tudo **menos** renderizar o jogo — import, verificação de
mídia, config, melhorias — e o motor (código de jogo + backend Orbis) preenche o
buraco conforme a camada de plataforma nasce. É o modelo dos ports de cena limpa
(o **Dusk**, de Twilight Princess, faz assim).

## Por que casca-primeiro
- Dá **marco em hardware real cedo** (PKG que boota no teu PS4 com ps4debug).
- A casca é **100% código nosso** (Apache-2.0), sem depender do motor pronto.
- Trava a **cena limpa em runtime**: o app exige o teu jogo e **verifica a mídia**
  antes de rodar — nada de asset embarcado.

## Camadas do produto

```
┌──────────────────────────────────────────────┐
│ BOOT (eboot Orbis)                             │  ← PKG fpkg, OpenOrbis
│  ├─ FIRST-RUN: import + verificação de mídia   │  ← estilo Dusk (BYO-dump)
│  ├─ CONFIG: settings + melhorias               │
│  └─ ENGINE MOUNT  ───────────────┐             │
└──────────────────────────────────┼────────────┘
                                    ▼
              [ motor do jogo + backend Orbis ]     ← preenche depois (fases B–D)
```

## 1. Import + verificação de mídia (o "faz isso também" do Dusk)
No primeiro run o app:
1. **Acha** os dados do jogo do usuário (USB/HDD do PS4, ou push via rede).
2. **Verifica a mídia**: game ID == `RMGE01`, região NTSC-U, e **hashes
   conhecidos** batem (ex. `sys/main.dol` sha1 `9a71008a…`, amarrado ao nosso
   ingest e ao content ID da ASIRN). Mídia errada/corrompida → recusa, igual Dusk.
3. **Importa** pra storage local do PS4 os assets necessários. Nada vem no PKG.

Isso é o enforcement do BYO-dump em runtime. Manifesto esperado em
`product/media/manifest_rmge01.json`; lógica em `product/media/`.

## 2. Config + melhorias
Store de settings + menu (scaffolding agora, preenche com o motor):
- **Vídeo:** resolução (720p/1080p), widescreen 16:9 (SMG é 4:3 nativo),
  framerate alvo.
- **Controle:** mapeamento WiiRemote→DualShock, ponteiro (giroscópio/touchpad/stick).
- **Idioma/região.**
- **Melhorias (toggles):** render interno maior, texturas, etc. — ligadas conforme
  o backend GX suportar.

## 3. Build + deploy (loop de hardware)
- **Toolchain:** OpenOrbis PS4 Toolchain (LLVM/clang → Orbis ELF → fpkg).
  Em arm64 macOS: via Rosetta (binários x86-64) ou container Linux (colima/docker).
- **param.sfo / content ID:** derivados da ASIRN
  (`UP0001-QVOX00001_00-7E0C9848AC532465`; ver `quovix/asirn`).
- **Deploy:** payload/PKG pro PS4 via **ps4debug** (rede) — teste em hardware real.
- **Primeiro marco:** PKG mínimo que boota ("casca vazia") no console → prova o
  loop build→sign→deploy→boot inteiro, antes de qualquer gráfico.

## 4. Ponto de montagem do motor
Interface limpa onde o motor portado pluga atrás da casca. Enquanto o backend
Orbis (GX→GNM, áudio, input) não existe, o mount roda um **stub** (tela de status
/ "engine not loaded"), e a casca já é um produto funcional e testável.

## Sequência
1. **Casca booting** no PS4 (PKG mínimo via OpenOrbis + ps4debug). ← primeiro marco
2. **Import + verificação de mídia** (módulo já em código, testável no host).
3. **Config/melhorias** (store + menu).
4. **Mount do motor** conforme fases B–D do `ARCHITECTURE.md` entregam o backend.

## Cena limpa
Tudo em `product/` é **código original** (Apache-2.0, `@orig none`). Os assets do
jogo **nunca** entram no PKG; são importados do dump do usuário e verificados em
runtime. O motor (decomp portado) fica atrás da fronteira do `PLATFORM-BOUNDARY.md`.
