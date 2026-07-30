# Compiler de cardápio do Última Luz

> **Status:** package de referência. Não existe compiler W executável.

Este package mantém um compiler pequeno no profile `bootstrap.w0`. O product
`menu-compiler` usa `w.host/build-transform@1`. Ele recebe um `String` chamado
`menu` e produz `Bytes` chamados `bytecode`.

O package não recebe filesystem, network, environment, clock, random ou secret.
O package principal o usa como `.build` dependency por meio do
[`workspace.w`](../../workspace.w).
