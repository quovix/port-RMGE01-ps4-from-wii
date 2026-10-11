# 06 — Instalação da casca no PS4

Em **11/10/2026**, o usuário autorizou instalar o build do
[marco de matemática](05-matematica-escalar-e-overlays.md) e reiniciar o agente
de Remote Play que estava travado. A instalação foi concluída pelo Package
Installer do GoldHEN, substituindo a casca antiga de título `QVOX00001`.

O PKG foi enviado com um nome novo, preservando o arquivo de instalação anterior.
O SHA-256 da cópia recebida foi conferido antes da instalação. Como o modo de
background recusava o conteúdo já instalado, foi usado o modo de primeiro plano
e confirmada a reinstalação do arquivo correto. A configuração de background
foi restaurada após a conclusão e o console voltou à home.

A confirmação final foi independente da tela de progresso: o arquivo instalado
`/user/app/QVOX00001/app.pkg` foi lido de volta e comparado ao build validado.

| Verificação | Resultado |
|---|---|
| Title ID | `QVOX00001` |
| PKG enviado | `QVOX00001-math-5d5cd9af.pkg` |
| Tamanho instalado | 6.619.136 bytes |
| SHA-256 local, recebido e instalado | `5d5cd9af9f593b5684c988de1b2634c06040d8d856792df3d3d4ce870f6e2181` |
| Substituição da casca antiga | ✅ confirmada pelo conteúdo instalado |
| Configuração de background restaurada | ✅ |
| Código/assets do jogo no pacote | ❌ nenhum incluído |
| Boot e autotestes deste build em hardware | ✅ validados depois; ver [registro de boot](07-validacao-de-boot-no-ps4.md) |

Logs, endereço do console, capturas e a cópia de verificação permanecem privados
em `out/install-session/` e temporários locais, fora do Git. Nenhum commit, push
ou release público foi realizado.

O [registro de boot](07-validacao-de-boot-no-ps4.md) documenta a execução feita
depois da instalação. Instalação, startup da casca e execução do motor são
evidências distintas; o motor do jogo ainda não foi executado.
