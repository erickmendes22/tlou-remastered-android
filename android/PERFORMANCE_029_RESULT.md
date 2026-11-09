# TLOU Android v0.29 — dispositivo, comparação e auditoria upstream

Data: 2026-11-04. Fonte: arquivo de execução fornecido pelo usuário `TLOU-Android-Logs (22)(2).zip`; sem recompilação de shaders nas janelas estáveis finais.

## O que mudou e o que os números realmente demonstram

Android Adreno 830, Turnip Gen8 V37 patched, patch 720p, Readback 1.

Execução anterior (`launch-previous.log`): `TLOU_INLINE_SINGLE_COPY=1`, `TLOU_COMPUTE_WAIT_FAST=1`, `TLOU_SPLIT_CS64=1`; **sem** `TLOU_SRT_HOT_READBACKS`.

Nova execução v0.29 (`launch.log`): `TLOU_SRT_HOT_READBACKS=1`, `TLOU_INLINE_SINGLE_COPY=0`, sem `TLOU_COMPUTE_WAIT_FAST`, `TLOU_SPLIT_CS64=1`.

| Métrica, últimas quatro janelas finais sem compilações | Execução anterior | v0.29 |
| --- | ---: | ---: |
| FPS médios | 36,10 | 35,75 |
| CPU command, % de um núcleo | 75,75 | 76,55 |
| CPU draw, % de um núcleo | 65,50 | 64,60 |
| Espera acumulada `gpu-ticks` ms/5s | 1073,88 | 1060,45 |
| Lotes de upload, por 5s | 24595,00 | 24843,50 |
| Compilações nas quatro janelas | 0 | 0 |

As execuções **não** são A/B exclusivamente da v0.29, pois as opções de cópia inline e espera de compute mudaram entre os launches. A diferença de ~0,35 FPS não prova regressão do prefetch; tampouco existe ganho demonstrado.

Mecanismo realmente ativado: nas quatro janelas com atividade de gameplay (a primeira ainda na transição), as linhas `TLOU SRT temporal readbacks` mostram `candidate-bytes=648416/821840/842144/888656` e `published-bytes` iguais, com `late-skipped-bytes=0`. Os readbacks mapeados diretos caem para ~12-13,5 mil/5s, contra ~17-18 mil/5s anteriores. Não houve fallback para staging nas janelas, mas a espera de GPU permaneceu perto de 1,06s/5s e o comando ainda consome ~76% de núcleo. Portanto, o prefetch executa o que foi programado, **sem liberar o caminho crítico do frame**.

Não gerar APK alegando ganho. Deixar a experiência OFF por padrão no código-fonte, preservando a possibilidade de reativação de quem já tiver a preferência salva (ou quiser investigação controlada). Não modificar o caminho de Readback 1; Readback 2 excluído por bugs e FPS. Não recomparar apenas ON/OFF se a linha de base já está no ZIP.

## Upstream PC (ravencaio/tlou-remastered-pc)

O branch Android deriva de `32d24df` de novembro 1. O upstream PC estava 17 commits à frente, até `212fb04` de 04/11, release v0.1.6.

- 0.1.2: cópia de stencil de profundidade (`82c46b8b`) e compilação de shaders em background. Corrige gráficos; `GOW3_ASYNC_SHADERS=1` já está ativo no dispositivo.
- 0.1.3: shader da chuva `2288df23`, correção visual.
- 0.1.4: saneamento de leitura de `fault_buffer` (limite de elementos, limpeza inicial), correção de integridade e estabilidade; nenhuma aceleração sustentada reportada.
- 0.1.5: sombras PS4 Pro aumentam qualidade/custo, sem justificativa para ativar no Snapdragon; correções do mouse não se aplicam a FPS.
- 0.1.6: outras IDs CUSA, sem otimização material de CPU/GPU.

**Não portar** as mudanças anteriores como se fossem otimizações de FPS. Avaliar correções gráficas separadamente, com proteção de versão e teste visual. O metadata `device-ready.json` do ZIP diz CUSA00552/01.00, enquanto as release notes do PC indicam update 1.11: não presumir que offsets/assinaturas do jogo são idênticos.

## Onde a investigação tem fundamento para continuar

Os contadores de `gpu-ticks` são TEMPOS DE ESPERA INSTRUMENTADOS (podem se sobrepor), não milissegundos totais de execução da GPU. Os ~24 mil uploads/5s tampouco significam ~24 mil cópias redundantes. Já houve v0.24 (zero operações agrupadas), v0.26 (NOP sem FPS), v0.27 (relaxed FP sem ganho comprovado), v0.28 (wait/nested-IB sem ganho). Nada disso deve ser simplesmente repetido.

1. O perfil de timestamps mais antigo localizou `22e6e19e` e `61d67259` em ~14,5ms/frame combinados; esta execução apenas contém dumps binários, sem novos timestamps. Verificar custo ATUAL e diferenças de ocupação/instruções Adreno antes de substituir o shader. Split64 e relaxed-FP não provaram ganho em execuções anteriores.
2. O command processor (75–77% de núcleo), draw (63–66%) e recorder (48–52%) são custos reais. A otimização FAST_BIND/DRAW_REPLAY de ShadowOfTheColossus-PC tem melhora medida naquela engine, mas TLOU ainda **não tem taxa de reutilização de estado ou breakdown de BindResources por fase**. Não transplantar sem essas evidências e sem equivalência de gerações, memória e descritores.
3. Para qualquer candidato de próxima compilação, exigir redução real do custo que ele promete eliminar (tempo GPU dos hashes ou tempo ocupado de draw/command) e impacto sustentado em FPS e 1% lows, sem regressão gráfica, física, saves e crashes. Uma build de teste sem esse fundamento não atende ao pedido.

Fontes:
- https://github.com/ravencaio/tlou-remastered-pc/releases/tag/v0.1.6
- https://github.com/ravencaio/tlou-remastered-pc/compare/32d24df...main
- https://github.com/Lucas-blip-png/ShadowOfTheColossus-PC/blob/main/docs/sotc/DRAW-REPLAY.md
- `android/PERFORMANCE_028_RESULT.md`, `android/PERFORMANCE_029.md`
