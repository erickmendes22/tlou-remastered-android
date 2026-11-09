# TLOU Android v0.17 — Compute 1024 → 64 threads

## Escopo

Opt-in `TLOU_SPLIT_CS64=1` para dois shaders de transformação por vértice,
identificados pelos hashes completos `0x22e6e19e` e `0x61d67259`.
Todos os demais programas mantêm o comportamento anterior.

Os perfis da v0.16 atribuíram aproximadamente 14,5 ms combinados a esses dois
shaders, mas os timestamps e as quebras de passes introduzidos pelo profiler
podem alterar significativamente o custo medido. O experimento exige teste
de FPS com **Medir passes da GPU desligado**.

## Sem alterar os cálculos por vértice

O módulo SPIR-V continua contendo as operações originais de transformação.
O que muda é o tamanho físico do grupo e a apresentação dos BuiltIns:

- Cada workgroup original de 1024 × 1 × 1 corresponde a 16 workgroups físicos
  de 64 × 1 × 1.
- Num dispatch direto, a quantidade de grupos em X é multiplicada por 16.
- `virtual_group_x = physical_group_x / 16`
- `virtual_local_x = (physical_group_x % 16)*64 + physical_local_x`
- `virtual_workgroup_count_x = physical_count_x / 16`

WorkgroupId, WorkgroupIndex, LocalInvocationId e LocalInvocationIndex são
reconstruídos para reproduzir o shader original. O índice linear
`virtual_group_x*1024 + virtual_local_x` é idêntico a
`physical_group_x*64 + physical_local_x`.

## Barreiras de segurança

A opção só atua sobre esses dois hashes e grupos de 1024 × 1 × 1 sem
shared memory e operações coletivas de grupo. Não é aplicada a programas
com grupos de dimensões diferentes. Dispatch indireto, grupos com X=0
e dispatch que excederia o máximo X do driver recebem pipeline nativo de
1024 threads, com identidade de cache distinta e argumentos originais.

Shaders SPIR-V otimizados usam o sufixo `_tlou64v1` e verificação do hash
dos binários para impedir mistura entre cache original e experimental.

A opção fica desligada por padrão, pode ser desligada individualmente,
e o botão de restaurar experimentos também a desliga. Não depende de
patches de resolução, Readback, perfil GPU ou demais experimentos.

## Validação

`gow3_pc/tests/test_tlou_compute_split.cpp` verifica todos os 1024 índices
de cada grupo em configurações com 1, 2, 7 e 53 grupos, com unicidade,
preservação de índices e guardas para recusa. Ele integra
`android/bionic/test_runtime_contracts.sh` e o workflow de compilação ARM64.

Os testes matemáticos e a compilação não demonstram, sozinhos, ganho
de FPS nem ausência de glitches em hardware Adreno. Para medir, use o
mesmo save/trecho e configurações idênticas, com a opção OFF/ON em
execuções separadas e o profiler GPU desligado. Se houver regressão,
desative somente essa opção; o restante do runtime permanece disponível.
