# TLOU 0.16 — limpeza dos experimentos e readback de acessos CPU

## Evidência e remoções

Nos logs 24/25, `TLOU_BATCH_SRT_READBACKS=1` mantém `multi-table=0` e
`combined-requests=0` nas janelas estáveis da área pesada. Os FPS continuam
próximos de 23. O agrupamento DMA ligado/desligado também não mostra ganho
consistente. Seus controles, variáveis, caminhos de agrupamento e testes
específicos foram removidos; as cópias DMA normais continuam funcionando.
A divisão de regiões grandes em trechos de até 1 GiB foi preservada.

O controle de afinidade em núcleos rápidos também foi retirado do launcher:
a preferência antiga não é aplicada. O agendamento fica com o Android;
o suporte nativo anterior permanece disponível para outros consumidores.
As outras opções antigas de preparação/compilação não foram avaliadas
isoladamente nesses logs e não são tratadas como ganho comprovado.

O readback direto da 0.14 permanece ligado por padrão, pois o usuário e os
contadores mostraram ganho. Ele continua limitado a 64 KiB no `DownloadRanges`.

## Nova opção independente

Vídeo e desempenho → Experimentos de desempenho →
**Readback direto para acessos da CPU**. Desligado por padrão.
`TLOU_MAPPED_FAULT_READBACKS=1`. Requer reabrir o jogo.

O perfil registra 29–40 chamadas a `DownloadMemory` em cinco segundos,
com cerca de 350–430 ms esperando. Esse caminho ainda sempre copiava para
staging e terminava a fila atual, mesmo com a memória coerente mapeada.
A nova opção usa a mesma seleção de intervalos e janela original de
512 KiB. Não reduz a janela nem muda proteção, lazy writes ou política
para dados mais recentes do guest.

A seleção de origem mapeada/espera/mesclagem é compartilhada com a 0.14.
Só permite memória coerente, mapeamento completo e barreira HOST_READ ativa.
Espera o maior tick conservador das páginas/janelas de origem; histórico
ausente exige o tick atual. Tick futuro ou mapeamento inválido mantém o
staging. Resolve readbacks assíncronos anteriores antes de selecionar dados.
`WriteBackGpuData`, `EraseLazyPages` e a limpeza do tracker permanecem nos
mesmos pontos. Escritas guest mais novas continuam preservadas pelo merger.

Para as janelas de falha, o snapshot temporário é limitado a 1 MiB,
incluindo padding dos intervalos; acima disso usa o staging antigo.
O limite padrão de 64 KiB para tabelas não foi ampliado. A nova opção não
depende de ligar/desligar a opção original.

`TLOU mapped fault readbacks (5 s)` informa leituras diretas, bytes,
leituras que evitaram nova submissão e fallback. Isso não significa
espera zero: as escritas GPU necessárias continuam sendo aguardadas.

## Compute: evidência necessária para otimizar sem adivinhar

Os shaders 22e6e19e e 61d67259 continuam somando aproximadamente 14.5 ms por
quadro, tanto em 540p quanto em 720p. Os ZIPs trazem hashes/tempos, mas não
suas instruções. Não foi adicionado HLE ou removida barreira a partir de hash.

Com **Medir passes da GPU** ligado, o renderer captura uma vez por processo
os binários GCN de apenas esses dois programas (até 256 KiB cada), antes
da consulta ao cache. Assim não exige apagar o cache ou recompilar todos
os shaders. Os arquivos `tlou/perf-cs-22e6e19e.bin` e
`tlou/perf-cs-61d67259.bin` entram no ZIP de logs. Sem perfil não há captura.
Não há dumps contínuos nem snapshots de buffers por quadro.

## Validação e comparação

Testes nativos verificam leitura só depois da espera com PROT_NONE,
limites separados de 64 KiB e 1 MiB, uma janela de 512 KiB com intervalos
esparsos, dados byte a byte, histórico, lacunas e overflow, além dos 3000
planos aleatórios anteriores. Sanitizadores de endereço/comportamento
indefinido, testes de runtime, preparação/patches, FPS e compilação Java
passam. O CI valida ARM64, dependências e APK assinado.

Compare a nova opção OFF/ON na mesma cena e resolução, reabrindo o jogo e
mantendo o restante igual. Perfil GPU desligado ao medir FPS. O ganho no
aparelho ainda não foi medido; o custo de compute e as dependências entre
CPU/GPU continuam sendo alvos de investigação, não problemas resolvidos.
