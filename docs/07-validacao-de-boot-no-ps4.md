# 07 — Boot da casca confirmado no PS4

Em **11/10/2026**, foi lançado no hardware o build instalado do marco de
matemática. Seu PKG é identificado pelo SHA-256
`5d5cd9af9f593b5684c988de1b2634c06040d8d856792df3d3d4ce870f6e2181`;
a instalação havia sido verificada pela cópia do `app.pkg` efetivamente instalado.

Uma captura de klog foi iniciada antes do launch, com janela de 45 segundos.
A notificação on-screen de casca iniciada foi observada. Os registros próprios
confirmam:

```text
[quovix] boot: main entrou
[quovix] passo 1 ok
[quovix] heap: autoteste OK (scalar/array, 64/128 bytes de alinhamento)
[quovix] math: autoteste OK (65536 valores u16 e casos de borda)
[quovix] passo 3 ok: media ausente
[quovix] passo 5: entrando no loop (vivo)
```

Esses logs são mensagens do código próprio. O klog bruto, capturas, endereço do
console e relatório completo permanecem privados em `out/boot-session/` e
temporários locais. O console foi devolvido à home pelo botão PS, sem encerrar
o aplicativo à força.

| Comportamento observado | Estado |
|---|---|
| Loader chega ao main da casca | ✅ |
| Hide splash retorna | ✅ |
| Alocação scalar/array e alinhamentos nativos | ✅ autoteste em hardware |
| Conversões dos 65.536 u16 e casos de borda do perfil escalar | ✅ autoteste em hardware |
| Notificação de startup | ✅ |
| Ausência de mídia nos paths configurados | ✅ tratada e registrada |
| Entrada no loop de espera | ✅ |
| Verificação de main.dol real no PS4 | ⏳ falta fornecer mídia |
| Renderer e motor do jogo | ❌ ainda não implementados/executados |

A tela preta após a notificação corresponde à casca atual, que ainda não
desenha frames. O marco comprova startup e esses autotestes durante a janela
observada; não mede estabilidade prolongada, precisão completa do PPC, áudio,
input de gameplay ou execução de RMGE01. No teste de conversão, NaN→zero continua
sendo uma política do porte, não uma medição da instrução original do Wii.

O bloqueio anterior de load da casca antiga não se repetiu nesse launch. O
próximo trabalho de portabilidade está no
[patch privado de friend/nw4r](08-compatibilidade-friend-em-nw4r.md).
