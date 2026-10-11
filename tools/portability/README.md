# Medição de portabilidade do decomp local

`measure.py` usa o `compile_commands.json` **clang** gerado pelo Petari. Preserva
o working directory, os includes por TU, os defines de versão e as opções de
linguagem; substitui o target PPC pela plataforma escolhida. O padrão é
`src/Game` com `-fdelayed-template-parsing`. O formato segue a
[especificação do clang](https://clang.llvm.org/docs/JSONCompilationDatabase.html).

O Petari e o dump continuam fora deste repo. A ferramenta não executa comandos
de build nem escreve nos destinos de objetos/dependências do decomp. Comandos
com wrappers, response files ou as flags de saída adicional explicitamente
rejeitadas precisam ser normalizados antes. O harness foi validado com o compile
DB do Petari; não é um executor genérico de comandos de compilação arbitrários.

## Uso

Execute na raiz deste projeto. Use um `--out` novo a cada medição: a ferramenta
recusa destinos que já contêm `report.json` ou `results.jsonl`.

```sh
# Host: análise sem geração de código.
python3 tools/portability/measure.py /caminho/Petari/compile_commands.json \
  --out out/phase-a/host-syntax

# Target da toolchain da casca: análise x86-64.
python3 tools/portability/measure.py /caminho/Petari/compile_commands.json \
  --compiler /opt/homebrew/opt/llvm@18/bin/clang++ \
  --target x86_64-pc-freebsd12-elf --out out/phase-a/x86-syntax

# Geração de objetos x86-64; sem link, sem SDK Orbis, sem execução.
python3 tools/portability/measure.py /caminho/Petari/compile_commands.json \
  --compiler /opt/homebrew/opt/llvm@18/bin/clang++ \
  --target x86_64-pc-freebsd12-elf --mode object --out out/phase-a/x86-object

# Comparação sem o flag que adia o parsing de templates.
python3 tools/portability/measure.py /caminho/Petari/compile_commands.json \
  --compiler /opt/homebrew/opt/llvm@18/bin/clang++ \
  --target x86_64-pc-freebsd12-elf --strict-templates --out out/phase-a/x86-strict
```

`--prefix` muda o subconjunto; `--limit` seleciona os primeiros N comandos após
ordenação por caminho (amostra determinística, não aleatória). `--jobs` controla
a concorrência de processos clang; `--timeout` limita cada TU em segundos.

`--preinclude` injeta um header próprio antes do TU, sem editar o decomp. Pode
ser repetido; o relatório registra paths e SHA-256 desses headers. O primeiro
patch de compatibilidade usa:

```sh
python3 tools/portability/measure.py /caminho/Petari/compile_commands.json \
  --compiler /opt/homebrew/opt/llvm@18/bin/clang++ \
  --target x86_64-pc-freebsd12-elf --mode object \
  --preinclude runtime/compat/heap_new.hpp --out out/phase-a/heap-object
```

O perfil de matemática usa um VFS overlay do clang, com headers privados
redirecionados para adapters próprios. Não altera nem copia o source do jogo;
o manifesto guarda caminhos e hashes. Use paths canônicos correspondentes ao
compile DB; aliases absolutos por symlink podem não coincidir com o nome VFS.

```sh
python3 tools/portability/create_overlay.py /caminho/Petari \
  --out out/compat-math/overlay.json
python3 tools/portability/measure.py /caminho/Petari/compile_commands.json \
  --compiler /opt/homebrew/opt/llvm@18/bin/clang++ \
  --target x86_64-pc-freebsd12-elf --mode object \
  --preinclude runtime/compat/heap_new.hpp \
  --preinclude runtime/compat/math_profile.h \
  --vfs-overlay out/compat-math/overlay.json --out out/phase-a/math-object
```

`--vfs-overlay` pode ser repetido; seu path/hash entra no relatório. O perfil
cobre FastCast u16, valor absoluto e seleção escalar. Paired singles em geral,
`frsqrte`, FPSCR e endianness de recursos ainda exigem implementação/validação.

O patch opcional de `friend` gera um header **derivado e privado**, separado do
repo público e do checkout Petari. Apenas a receita original, sem contexto do
jogo, é publicável. O script recusa diretório privado existente e inputs com
declarações diferentes/ambíguas. O manifesto registra source/output hashes.

```sh
python3 tools/portability/create_overlay.py /caminho/Petari \
  --out out/compat-friend/overlay.json --linklist-friend \
  --private-dir /caminho/privado/novo-diretorio
python3 tools/portability/measure.py /caminho/Petari/compile_commands.json \
  --compiler /opt/homebrew/opt/llvm@18/bin/clang++ \
  --target x86_64-pc-freebsd12-elf --mode object \
  --preinclude runtime/compat/heap_new.hpp \
  --preinclude runtime/compat/math_profile.h \
  --vfs-overlay out/compat-friend/overlay.json --out out/phase-a/friend-object
```

A transformação troca a referência via typedef pelo nome da classe envolvente.
Mantém C++98, layouts e corpos de funções. Os testes usam templates sintéticos,
incluindo acesso a constructor/campo privado, em C++98 com `-pedantic-errors`.

## Evidência local

`report.json` contém compiler/version, target, SHA-256 do compile DB, totais,
timeouts, falhas de include e contagens por subsistema. `common_errors` conta
ocorrências de diagnósticos; `common_error_tus` conta comandos afetados por cada
mensagem, uma vez por TU. Um TU pode apresentar várias mensagens; as categorias
não se somam. `warning_tus` preserva a visibilidade das advertências mesmo nos
TUs que passaram. Os erros estão limitados a 20 por invocação.

`results.jsonl` contém argv efetivo, caminho, exit code, tempo e diagnósticos por
comando. `objects/` contém objetos apenas no modo `object`. **Tudo em `out/`,
gitignored**: os objetos são derivados do decomp e permanecem locais.

Exit code: `0` quando todos os comandos selecionados passam; `1` quando algum
falha ou sofre timeout; `2` quando argumentos/configuração são inválidos.
Compile DBs podem ter mais de um comando por arquivo: cada entrada é medida.
Não encontrar um include é falha da medição, contabilizada separadamente.

Passar nesta medição não comprova link, ABI compatível, endianness correta ou
comportamento no PS4. O flag de templates adia a análise de corpos ainda não
instanciados; a comparação com `--strict-templates` ajuda a expor essa limitação.

## Verificação do harness

```sh
python3 -m unittest discover -s tools/portability -p 'test_*.py' -v
```

Os testes compilam um fixture original e conferem includes/defines com paths
contendo espaços, leitura do campo `command`, emissão isolada de objetos,
preservação dos objetos/dependências originais, recusa de sobrescrever relatórios,
erro de include e rejeição de response files.
Também compilam/linkam/executam os adapters C e C++ contra headers sintéticos,
verificam outputs volatile e a preservação dos arquivos privados e relatórios.
