# 10 — Plano de implementação do porte (RMGE01 → Orbis)

## Propósito

Plano único, ordenado, do que falta para o porte sair de "engine inicializa" até
"jogo roda". Escrito em **13/10/2026**, logo após o primeiro boot observável no
emulador shadPS4 (macOS). Serve de mapa de marcos e de referência de decisão.
Não substitui os docs por subsistema; consolida a sequência.

Regras que atravessam todo o plano (de `AGENTS.md`):
- Publicar só nosso patch/backend/casca/ferramentas/documentação. Motor e objetos
  derivados são **locais** (`out/` gitignored). Não copiar corpos de função do
  decomp para o repo publicável.
- Consultar Dusklight/Aurora/Borealis como **referência de arquitetura**, revalidando
  no upstream antes de basear implementação; nunca incorporar source deles.
- Registrar separadamente: TU compilado, motor linkado, PKG validado, boot no
  hardware, boot no emulador. São evidências distintas.

## Dois ambientes de iteração (decisão registrada)

A partir de 13/10/2026 passamos a ter **dois** canais de teste, e o plano assume
os dois:

1. **Emulador shadPS4 macOS** (`~/Workspace/shadps4-macos/app/shadps4`, v0.19.0,
   x86_64 via Rosetta). Carrega nosso fself homebrew sem modificação, mapeia em
   `0x400000` **sem slide** (endereços do guest simbolizam direto contra
   `out/engine-jump/eboot.elf`), e **expõe stdout/klog** — canal que está morto no
   hardware. Iteração em segundos, sem reboot. É o laço de desenvolvimento primário.
2. **PS4 real (GoldHEN + ps4debug + FTP)**. Validação final e fonte de verdade de
   comportamento de hardware. Iteração cara (reboot, FTP instável). Reservado para
   confirmar marcos, não para caça a bug linha a linha.

Divergência possível entre os dois (timing, layout de memória, semântica de HLE):
todo marco fechado no emulador é **revalidado** no hardware antes de considerado
concluído.

## Estado atual (evidência de 13/10/2026, emulador)

Confirmado no `shad_run2.log`:
- fself carrega; regiões de memória criadas a partir de `0x400000`.
- **635 construtores globais rodam** (loop do `main.c`), log `ctors TODOS OK`.
- Entra no `qvx_game_entry` (o `main` real do jogo).
- Executa início de `GameSystem::init()`; imprime `arenaLo/Hi`.
- **Crash duro** logo em seguida, dentro de `HeapMemoryWatcher::createRootHeap`,
  sem nenhuma HLE/stub chamada depois (código puro do motor). `shad_log.txt`
  truncado no meio de linha confirma morte abrupta, não saída limpa.

Linkado no `eboot` v01.15: **24 objetos JSystem reais** (heaps, ARAM completo,
FileLoader, Archive, MemArchive, DvdRipper, DvdFile, JUTVideo), stubs ≈ 1458.

### Atualização 13/10 (tarde) — bugs corrigidos e fronteira atual

Correções fechadas no backend (os.c), todas de ABI/mapa-de-memória Wii→host:
1. **Globais do SO em 0x80000000**: `OSPhysicalToCached(p)` é macro `(void*)(p +
   0x80000000)` embutida em todos os objetos; `initArena` derefa `code->memorySize`
   em 0x80000000 → SIGSEGV. Fix: `mmap(MAP_FIXED)` de 16KB zerado em 0x80000000.
2. **Arena acima de 4GB truncada**: `JKRHeap::initArena` usa o LITERAL `0xFFFFFFE0`
   (não a macro `OSRoundDown32B`), que trunca ponteiro de 64 bits. Com a arena em
   `malloc`→0x201000000, `ramStart` virava lixo. Fix: arenas em endereços FIXOS
   < 4GB via `mmap` (MEM1=0x81000000/24MB, MEM2=0x90000000/64MB), masking de 32b
   fica lossless. Confirmado: `original arenaLo = 0x81000000`.
3. **Build anti-stale**: `build_engine_jump.sh` só movia o eboot.bin novo se não
   houvesse um em OUTDIR → carregava binário velho. Fix: sempre `mv -f` + guarda
   que aborta se eboot.bin for mais antigo que eboot.elf.
4. **Dump montado no emulador**: symlink `~/Library/Application Support/shadPS4/
   data/quovix/RMGE01` → `/Users/.../smg-decomp/rmge01-fs` (o jogo resolve
   `/data/quovix/RMGE01/files/...`).

**Fronteira atual (M1 quase fechado):** crash dentro de `JKRExpHeap::createRoot`
(construção do root heap), logo após `initArena` imprimir `original arenaLo =
0x81000000`. A linha seguinte (`OSGetMEM2ArenaLo` → mmap 0x90000000) não roda, o
que localiza o crash no ctor do root `JKRExpHeap`. Leitura descartou os null-deref
óbvios (findFromRoot/JKRDisposer são null-safe; arena mapeada e gravável). Próximo
passo: instrumentar o caminho do engine (overlay local em JKRHeap/JKRExpHeap com
OSReport) pra pinpoint, já que debugger (Rosetta perde conexão) e handler de sinal
(shadPS4 não entrega SIGSEGV ao guest) estão mortos no emulador.

## Marcos

Cada marco: objetivo, subsistemas, tamanho honesto, critério de pronto.

### M1 — Passar do `createRootHeap` (o crash atual)
- **Objetivo:** engine conclui a criação da hierarquia de heaps sem crashar.
- **Trabalho:** capturar backtrace sob lldb (ajustar `process handle -s true
  SIGSEGV SIGBUS` para parar antes do handler do shadPS4), simbolizar contra
  `eboot.elf`, identificar se é (a) `OSGetArenaLo/Hi` devolvendo região com
  tamanho/alinhamento que o `JKRExpHeap::createRoot` não aceita, ou (b) símbolo
  JSystem ainda stubado no caminho. Corrigir no **nosso backend** (os.c arena) ou
  compilar o objeto real que falta.
- **Tamanho:** pequeno (1 bug). Dias.
- **Pronto quando:** log passa de `createRootHeap` e chega em `JKRAram::create`.

### M2 — Cadeia de init de memória/sistema até o frame loop
- **Objetivo:** `GameSystem::init()` completa e entra em `frameLoop()`.
- **Subsistemas na ordem do boot:** `JKRAram::create` → `createHeaps`
  (`JKRSolidHeap` de áudio, stationed heaps) → `FileRipper::setup` →
  `GameSystemException::init` → `initGX` (ver M4) → `GameSystemObjHolder::init`
  (áudio, render mode, resource holders, message resource, controller, display).
- **Trabalho:** iterar frontier por frontier no emulador; cada stub que o caminho
  toca e que devolve lixo vira um objeto real compilado (padrão já validado:
  compila limpo com o overlay; exceção pontual recebe patch estrutural via overlay
  em `out/`, como `JKRMemArchive`).
- **Tamanho:** médio. Algumas semanas de rodadas rápidas.
- **Pronto quando:** engine entra no `frameLoop` e repete frames sem crashar
  (ainda sem imagem).

### M3 — Dados do jogo montados (filesystem)
- **Objetivo:** a carga de archives do disco encontra os arquivos.
- **Trabalho:** no **emulador**, montar o dump RMGE01 no filesystem falso
  (`--mount` / `override-root`) no caminho que nosso backend DVD serve
  (`/data/quovix/RMGE01/files`). No **hardware** já está em `/data` (3.2GB
  enviados). Validar `JKRDvdRipper`/`JKRMemArchive` lendo `.arc` reais
  (stationed archive, ErrorMessageArchive).
- **Tamanho:** pequeno-médio (sobretudo plumbing de path e descompressão Yaz0/SZS).
- **Pronto quando:** `exeLoadStationedArchive` carrega um `.arc` e o jogo consome
  sem erro.

### M4 — Gráficos: GX → backend de apresentação (o maior bloco)
- **Objetivo:** o que o jogo desenha aparece na tela.
- **Problema:** o jogo renderiza via **GX** (pipeline do Wii: FIFO de comandos,
  GXInit, display lists, retrace via JUTVideo). Hoje só há shims de `video_out`/
  `gx_vi`; não há tradução de comandos GX para pixels.
- **Estratégia a decidir (ponto de decisão arquitetural):**
  - (a) Traduzir GX → camada de apresentação do host. No emulador o alvo natural é
    o que o shadPS4 já expõe (Vulkan/Metal via GnmDriver); no PS4 seria GNM. São
    alvos diferentes — definir se mantemos uma abstração única ou dois backends.
  - (b) Começar por um backend mínimo que só processe retrace + framebuffer linear
    (prova de vida: cor de fundo, splash) antes do pipeline completo de display
    lists/texturas/TEV.
  - Referência: arquitetura GX do Dusklight (revalidar no upstream; não incorporar).
- **Tamanho:** grande. É um subsistema, não um bug. O cronograma real do "jogável"
  depende daqui.
- **Pronto quando:** primeiro frame com conteúdo visível (incremental: fundo →
  logo → layout → cena 3D).

### M5 — Áudio
- **Objetivo:** mixagem e saída de áudio.
- **Subsistemas:** `AudSystemWrapper`, ARAM (já compilado), stream/seq.
- **Tamanho:** médio. Pode ser adiado após vídeo (jogo roda mudo primeiro).
- **Pronto quando:** `exeInitializeAudio` completa e há saída (ou stub silencioso
  estável que não bloqueia o boot).

### M6 — Input
- **Objetivo:** controle mapeado (WPad/Wiimote+Nunchuk → DualShock/teclado).
- **Tamanho:** pequeno-médio. Mapeamento de botões/eixos + semântica de movimento.
- **Pronto quando:** menu responde a input.

### M7 — Estabilidade e paridade hardware
- **Objetivo:** o que roda no emulador roda no PS4.
- **Trabalho:** fechar divergências de ABI/timing/HLE; revalidar cada marco no
  hardware; tratar o que o emulador perdoa e o Orbis não (sandbox de fs, entrega
  de sinais, layout de memória).
- **Tamanho:** contínuo, cresce perto do fim.

## Ferramentas de suporte a construir/manter

- **Laço do emulador** (`scratchpad/shad_*.sh`): run + captura de stdout; run sob
  lldb com parada em SIGSEGV/SIGBUS; `symbolize.sh` (endereços → símbolo via
  `eboot.elf`). Promover de scratchpad para `tools/` quando estabilizar.
- **Checkpoints de stdout no backend**: como stdout funciona no emulador, instrumentar
  nossos shims (os.c, dvd.c, gx_vi.c) com marcadores finos no caminho de boot para
  localizar frontier sem backtrace. Baratíssimo e específico do nosso código.
- **Build/pacote** (`build_engine_jump.sh`, `repack_v101.sh`, `gen_offsets.sh`):
  já mecânico; manter o passo de `create-fself` com `OO_PS4_TOOLCHAIN` setado.
- **Poller de hardware** (`phase_poller.py`): segue válido para validação no PS4.

## Sequência de execução imediata

1. **M1 agora:** lldb com parada em sinal → backtrace → simbolizar → corrigir o
   crash do `createRootHeap`.
2. Descer M2 frontier a frontier no emulador até o `frameLoop`.
3. Em paralelo a M2, preparar M3 (montar o dump no emulador).
4. Ao bater em `initGX`, **parar e decidir a estratégia de M4** — é a decisão que
   define o cronograma do "jogável".
5. Revalidar cada marco fechado no hardware (v-bump do PKG, poller).

## Riscos e incógnitas honestas

- **M4 (GX) é o item que domina o cronograma** e ainda não começou. "Engine boota e
  loopa" (M1–M2) é perto; "jogo na tela" depende de M4.
- Divergência emulador × hardware pode esconder bugs que só aparecem no Orbis.
- Cascata de stubs em M2 pode trazer dependências fundas não previstas (threading
  de JKR, decomp Yaz0/SZS, ARAM assíncrono).
- Dependência de dados: sem o dump montado corretamente, M3 em diante não anda.
