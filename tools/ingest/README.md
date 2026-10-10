# tools/ingest — fase 1 (BYO-dump)

Lê o **dump do próprio usuário** em **RVZ** e produz, **localmente**, o que o
recompilador precisa: `main.dol` verificado + FST + metadados. Nada do jogo é
redistribuído.

## Requisito

O RVZ é o formato comprimido do Dolphin (blocos + zstd/bzip2/lzma + criptografia
de partição). A autoridade do formato é o **DolphinTool**:

```sh
brew install --cask dolphin-emu
# binário: /Applications/Dolphin.app/Contents/MacOS/DolphinTool
```

## Uso

```sh
python3 -I tools/ingest/ingest.py <dump.rvz> --workdir out/ingest
```

Faz: lê header e **confirma game ID RMGE01** → verifica integridade → extrai
`main.dol` + FST → computa `sha1` do `main.dol` → grava `ingest-report.json`.

O `sha1` vira o **`src_sha1`** da proveniência (ver [`../../docs/PROVENANCE.md`](../../docs/PROVENANCE.md)):
identifica *qual* `main.dol` originou cada unidade recompilada, sem conter o binário.

## Saída

```
out/ingest/
  <FST extraída>        sys/main.dol + árvore de arquivos (LOCAL, não vai pro repo)
  ingest-report.json    game_id, tamanhos, sha1 do main.dol
```

> `out/` está no `.gitignore` — conteúdo do dump nunca é commitado.

## Status

Scaffold. Os subcomandos/flags do DolphinTool (`header -j`, `verify`, `extract`)
estão centralizados em `ingest.py` e são **validados no primeiro run contra o
dump real** — a CLI varia um pouco entre versões do Dolphin.
