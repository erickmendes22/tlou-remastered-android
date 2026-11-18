# TLOU 0.15 — agrupamento opcional de readbacks SRT

## Alvo

Os logs 19/20 usados na 0.14 localizam cerca de 9300 tabelas SRT baixadas a
cada cinco segundos. O walker sincroniza cada nó individualmente. A 0.14
reduz a cópia intermediária em buffers coerentes; esta experiência reduz o
número de pedidos independentes quando tabelas irmãs já têm os seus ponteiros
resolvíveis. Atua na chamada identificada pelo perfil, sem modificar shaders
por hash ou políticas de threads.

## Opção

Vídeo e desempenho → Experimentos → **Agrupar leituras de tabelas GPU**.
`TLOU_BATCH_SRT_READBACKS=1`, desligado por padrão. Requer reabrir o jogo.
A opção funciona tanto com o readback direto da 0.14 quanto com o staging
anterior. Todos os demais padrões da 0.14 permanecem iguais.

Antes do download original, já após `DrainDrawPipe`, o planejador encontra
outras tabelas GPU-modificadas com o mesmo pai. Tabelas não raiz precisam
ser consecutivas: o planejador para ao encontrar outro ramo, pois um
descendente intermediário poderia alterar a origem dos ponteiros por alias. As raízes usam user data;
as outras usam exclusivamente o pai já sincronizado pelo walker. O
planejador não resolve descendentes nem muda a ordem da caminhada original.
A chamada `DownloadRanges` recebe os intervalos exatos de tabelas irmãs,
e faz a espera e a mesclagem de bytes existentes uma vez para o grupo.

Limites conservadores: 16 intervalos, 64 KiB de páginas solicitadas; sem ampliar
buracos; sem tabelas dinâmicas; sem endereços inválidos/não mapeados;
sem sobreposição de páginas do tracker entre intervalos ou com a origem dos
ponteiros no pai. A seleção usa o tamanho real de página do tracker
(4 KiB), pois o download interno arredonda para essas páginas. Tabelas
com bytes distintos na mesma página não contam como independentes. Entradas históricas da lista de cópias são verificadas pela conclusão no
draw pipe, não apenas pela presença na lista. Havendo cópias CPU/HLE
realmente pendentes, mantém um download
individual. Uma tabela filha só tem o ponteiro lido depois de seu pai
passar pela sincronização original. Nenhum fence é dispensado.

Pode aumentar a latência de um pedido ao esperar também por uma irmã;
pode não encontrar grupos úteis na cena real. Por isso fica opcional.
Não há promessa de FPS a partir dos testes sintéticos.

## Medição

`TLOU SRT batches (5 s): calls=... tables=... multi-table=... combined-requests=...`
registra chamadas, intervalos pedidos, grupos com mais de uma tabela e
pedidos combinados (`tables - calls`). Não significa automaticamente o
mesmo número de submissões Vulkan eliminadas. O caminho direto tem seu
contador independente da 0.14. Com a experiência desligada, o caminho do
walker anterior é mantido e não gera este contador.

Compare na mesma área e resolução, com Medir passes da GPU desligado:

| Readback direto de buffers (0.14) | Agrupar leituras de tabelas (0.15) | Comparação |
| --- | --- | --- |
| Ligado | Desligado | Padrão da 0.14/0.15 |
| Ligado | Ligado | Efeito do agrupamento com memória mapeada |
| Desligado | Desligado | Cópia intermediária anterior |
| Desligado | Ligado | Efeito do agrupamento com staging |

## Validação

Teste C++ usa descritores guest inicialmente obsoletos e verifica que nenhum
ponteiro filho é lido antes de seu pai ser baixado. Compara os bytes finais
com o walker sequencial em 4000 árvores (pais/filhos, raízes e irmãs), além de
casos de aliases, sobreposições, nós dinâmicos, limites e fallback de cópias
pendentes. A simulação reduz 127121 chamadas para 90737; esses números medem
a simulação, não o jogo. O teste integra a suíte de runtime usada pelo CI.

Também foram inspecionados `DownloadMemory` (janela de falha de 512 KiB),
compilação de pipelines e apresentação. Os logs anteriores não justificam
alterar os dois últimos como solução para essa área: compilação pendente
zero e esperas de apresentação pequenas. Reduzir a janela de falha pode
multiplicar page faults, por isso não foi introduzido sem medida de demanda.
Os shaders compute caros continuam exigindo código capturado para uma
substituição HLE validada; não se adicionaram aliases por suposição.
