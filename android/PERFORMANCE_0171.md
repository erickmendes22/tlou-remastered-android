# TLOU v0.17.1 — comparação de 64/1024 threads (logs 30 e 31)

Os dois testes foram feitos em 720p, Readback 1, GPU profiler desligado,
com mapped readbacks, mapped fault readbacks, prep pool, cpu trims, prefetch
e parallel pipelines ativados. A única alteração entre as duas últimas
execuções foi `TLOU_SPLIT_CS64`.

| Medida | 64 ligado (log 30) | 64 desligado (log 31) |
| --- | ---: | ---: |
| FPS estável, últimas 3 janelas | 35,43 | 35,20 |
| Thread GPU command, CPU de 1 núcleo | 75,3% | 76,0% |
| Uploads por 5 s | 141,0 MiB | 139,9 MiB |
| Lotes de upload por 5 s | 24.187 | 24.217 |
| Espera GPU ticks por 5 s | 1.038 ms | 1.096 ms |
| Readbacks mapeados diretos por 5 s | 18.451 | 18.413 |

Estas amostras são curtas e não são repetições suficientes para comprovar
ganho. A diferença de ~0,23 FPS está dentro da variação esperada. Os
dois shaders `22e6e19e` e `61d67259` carregaram a versão de 64
somente na execução ligada. Não há DeviceLost registrado.

**Problema confirmado no log 31:** ao alternar OFF após ON, o cache registra
`its binary is not the one its description was written with (hash differs)`
para ambos os shaders. O arquivo de SPIR-V tinha sufixo por variante, mas os
metadados e chaves dos pipelines usavam o mesmo `HashCombine`.

**Correção:** a chave de permutação do shader, a chave do pipeline compute
e a chave usada pelo registro/validação de metadata agora são diferenciadas
pela variante, exclusivamente para os dois hashes-alvo e apenas ao
ativar o experimento. A variante padrão preserva a chave antiga.
O cache já produzido pela v0.17 pode exigir uma recompilação final ao
voltar para OFF, pois o meta anterior foi sobrescrito; após isso cada modo
mantém metadados independentes.

**Gargalo restante:** aproximadamente 24 mil lotes e 140 MiB de
uploads a cada 5 s, ~1,0 a 1,1 s de espera em GPU ticks por 5 s.
O upload e as esperas podem sobrepor-se e não são medição direta de
ocupação de GPU. Os readbacks mapeados da v0.14 continuam úteis:
~18.400 leituras / janela evitam ~18.200 novas submissões.
O caminho de fault tem ~30 leituras / janela sem evitar praticamente
nenhuma submissão; não há dados OFF/ON para atribuir custo ou ganho a ele.

Nenhuma nova alteração especulativa de sincronização foi incluída.
O experimento de grupos de 64 continua opcional e desligado por padrão.
