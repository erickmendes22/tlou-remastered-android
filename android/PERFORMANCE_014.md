# TLOU Android 0.14: readback direto de buffers pequenos

## Evidência: logs 19 e 20

Ambos usam agrupamento de uploads ligado e o usuário relata ausência de ganho.
O log 19 não tem perfil GPU; o 20 tem. Não são um A/B do agrupamento com
profiling igual. Não aparece relatório de sincronização DMA em nenhum deles;
os dados não sustentam aquele caminho como gargalo dominante desta cena.

No log 19, após carregar a área pesada, há cerca de 18–19 FPS com zero
compilações/desenhos pendentes. Esperas por ticks GPU somam aproximadamente
2.1 s por janela de cinco segundos. Espera por apresentação, aquisição de
buffers e trava de submit é pequena nas mesmas janelas.

O perfil do log 20 localiza o caminho: `DownloadRanges` (linha 1896 da 0.13)
executa 9378–9380 esperas por janela, com aproximadamente 2.59 s esperando o
semáforo e 0.46 s em submissões. `SyncSrtTables` registra 9301–9313 tabelas
baixadas antes de percorrer os descritores. Na última janela, os 67.87 ms/quadro
do perfil incluem 40.03 ms atribuídos ao intervalo entre submissões, cerca de
129 por quadro. Essa classificação inclui intervalos/barreiras e não mede
ocupação pura de hardware. Os shaders compute 22e6e19e e 61d67259 também têm
custo relevante; o seu código não foi fornecido nesses logs, então não se
introduziu um HLE por suposição baseada apenas no hash.

O perfil custa trabalho e seu FPS de 14–15 não deve ser comparado diretamente
ao log 19 como medida de regressão. Ainda assim, os dois apontam um custo de
espera por GPU, e o perfil identifica milhares de downloads síncronos pequenos.

## Mudança

O `DownloadRanges` antigo sempre gravava uma cópia GPU para staging,
submetia/esperava a fila atual e lia o staging. No dispositivo dos logs, as
arenas são `DeviceLocal | HostVisible | HostCoherent` e cada submissão já
termina com barreira de escritas GPU para `HOST_READ`.

`TLOU_MAPPED_READBACKS=1` permite ler diretamente esses dados quando o total
planejado cabe em 64 KiB. Antes da leitura, usa o maior tick registrado nas
páginas/janelas relevantes. Uma escrita ainda não submetida é submetida e
esperada por `Scheduler::Wait`; se o tick já era submetido, não se força uma
nova submissão do trabalho atual apenas para copiar de volta os mesmos bytes.
Histórico ausente exige o tick atual; histórico futuro/inválido cai no staging.

O caminho exige mapeamento completo, memória coerente e barreira HOST_READ
ativa. Resolve readbacks assíncronos anteriores como antes, monta um snapshot
CPU após a espera e chama o mesmo `WriteBackGpuData`: páginas lazy e bytes
mais recentes do guest continuam sujeitos à política de mesclagem existente.
Os flags de modificação são limpos nos mesmos pontos. Não se escreve o
mapeamento cru no guest nem se pula a conclusão das escritas GPU.

Leituras grandes, lacunas, mapeamentos ausentes/incompatíveis e falhas de
validação mantêm o staging antigo. O caminho desktop permanece igual sem a
variável. A referência Vulkan para leitura no host mantém barreira e espera;
memória coerente dispensa a invalidação manual:
https://docs.vulkan.org/guide/latest/synchronization_examples.html

## Opções e validação

**Vídeo e desempenho → Readback direto de buffers** vem ligado. Desligar e
reabrir restaura o staging para essa rotina. Agrupamento DMA passa a desligado
por padrão, respeitando escolhas já salvas. Os patches do eboot, resolução,
readback 1/2 e política de fences não foram alterados.

Uma linha `TLOU mapped readbacks (5 s)` informa leituras diretas, bytes,
quantas evitaram nova submissão e quantas caíram no staging. O perfil GPU
continua opcional e deve ficar desligado para comparar FPS.

O teste nativo usa memória PROT_NONE até a callback de espera, de modo que
uma leitura antecipada falha. Verifica partes em alocações distintas,
histórico ausente, ticks futuros, lacunas, limites/overflow e 3000 planos
aleatórios comparados byte a byte ao staging de referência. Também testa
que só se espera o tick necessário, sem exigir submissões posteriores que
escrevem em outras regiões. A suíte de runtime, preparação/patches, FPS e
compilação Java passa; o workflow verifica a compilação ARM64 e o APK.

Os testes locais verificam seleção/ordem/dados, não substituem a execução
Vulkan no Adreno. O novo ganho de FPS e a estabilidade visual precisam ser
medidos no aparelho. Esta mudança ataca o caminho caro identificado no log;
não elimina leituras GPU→CPU logicamente exigidas pelo percorrimento SRT.
