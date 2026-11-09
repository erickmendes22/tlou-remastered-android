# TLOU Android 0.13: lotes de uploads DMA

## O que os dados permitem concluir

O log 17 confirma afinidade efetiva nos núcleos 6/7, mas a área pesada mantém
18.3–19.6 flips/s, sem compilações ou desenhos pendentes. O usuário confirmou
ausência de ganho visual/FPS. A afinidade volta a ficar desligada por padrão;
uma preferência explicitamente salva continua respeitada. O contador de CPU
do thread inicial não representa todos os workers do jogo.

`wait=0` era apenas espera de compilação. O resumo agora chama esse campo de
`compile-wait`. Não havia tempos GPU por passe nem volume/custo dos uploads
DMA no pacote 17; portanto ele não comprova a causa principal dos 20 FPS.

## Alteração no caminho de memória

`SynchronizeDmaBuffers` percorre os buffers residentes para shaders com
endereçamento indireto. O caminho anterior reservava staging e gravava cópias
separadamente para cada trecho residente com bytes CPU modificados.

Com `TLOU_BATCH_DMA_UPLOADS=1`, os mesmos trechos alterados são agrupados por
arena em lotes de até 256 regiões e 4 MiB. Uma transferência individual já
maior que 4 MiB continua inteira. O agrupamento mantém a ordem, termina antes
de trocar de arena, preserva alinhamento de quatro bytes na origem e não
copia lacunas entre regiões. O rastreamento de barreiras usa as regiões
exatas da cópia agrupada: o intervalo entre alocações distantes não é marcado
como escrito. A seleção de bytes modificados, o tratamento
de páginas lazy, a resolução de readbacks, a gravação das barreiras e as
esperas das cópias continuam nos caminhos existentes. Não se limita a
sincronização a ponteiros presumidos nem se ignora conteúdo residente.

A opção **Vídeo e desempenho → Agrupar uploads de memória GPU** vem ligada
e permite voltar ao caminho de reservas separadas após reabrir o jogo.
O renderer desktop continua usando o caminho anterior quando a variável
não está definida. Os patches de resolução/estabilidade não foram alterados.

## Medição e limite de interpretação

O resumo de cinco segundos publica bytes/lotes de upload e deltas dos
contadores existentes de espera por ticks GPU, gravação, cópias, quadro livre,
trava de submit e apresentação AHB. Acquire/apresentação Android recebem
cronômetros por quadro. Os tempos de espera são tempos de parede de pontos
instrumentados, podem se sobrepor e não representam ocupação total da GPU.

O resumo DMA, antes restrito ao perfil completo, informa chamadas, bytes,
lotes de staging e tempo de parede na sincronização. Isso permite conferir
se esse caminho é significativo na cena do aparelho. Menos lotes não significa
menos bytes transferidos ou um ganho de FPS garantido.

**Análise de desempenho → Medir passes da GPU** expõe o profiler de timestamps
já existente, junto ao perfil de esperas do renderer. Fica desligado por
padrão. Para uma captura, ligue, reabra o jogo, reproduza a área pesada e
exporte os logs. Desligue e reabra antes de comparar FPS: os marcadores,
consultas e relatórios podem alterar o custo do quadro. Intervalos entre
submissões aparecem no perfil e podem conter GPU ociosa. O profiler verifica
suporte a timestamps/host query reset e considera os bits válidos do contador.

## Validação

O teste nativo do agrupador compara 30 mil uploads com o resultado do
caminho imediato, incluindo lacunas, trocas/revisitas de arena, múltiplos
quadros, payloads variáveis e offsets mutados pelo consumidor. Também
verifica os limites de 4 MiB/256 regiões e a preservação da ordem/alinhamento.
Um caso sintético de 1024 trechos de 64 bytes esparsos gera quatro lotes em
vez de 1024 reservas/comandos de upload, com as mesmas regiões de destino.
Isso mede redução de operações nesse caso, não FPS do jogo no Android.

Os testes de runtime, preparação/patches e FPS e a compilação Java fazem
parte da validação; o workflow compila o alvo ARM64 real e verifica o APK.
Ainda falta execução no aparelho para medir ganho e conferir estabilidade,
NPCs, iluminação e mudanças de cena. A causa dominante do limite de 20 FPS
permanece aberta até obter os custos da cena, sem atribuí-la à afinidade.
