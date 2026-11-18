# TLOU Android 0.12: distribuição das threads críticas

## Evidência e limite da conclusão

O pacote de logs 16 mantém aproximadamente 19.9–20.7 flips/s nas áreas já
carregadas, com zero compilações, zero espera por pipeline e zero desenhos
pendentes nas mesmas janelas. Os quatro experimentos ligados e desligados
produzem resultado semelhante. O ajuste de cache da 0.11 resolve custos das
transições, mas não explica esse FPS estável. A execução atual termina em
143; não contém um novo crash explícito.

O usuário também comparou 360p com 720p no mesmo ponto e informou FPS
praticamente igual. Na captura, os seis núcleos menores mostram atividade,
enquanto os dois de maior frequência aparecem ociosos. Uso agregado baixo
não prova ausência de gargalo em uma thread nem identifica sozinho uma
espera de sincronização. Não há medição de ganho da 0.12 no aparelho.

## Mudança executável

`TLOU_FAST_RENDER_CORES=1`, padrão do launcher, posiciona o thread principal
guest, o processador de comandos, o gravador de draws e o gravador Vulkan
no grupo mais rápido conhecido dentro da máscara permitida no lançamento.
A ordem é capacidade de CPU, depois frequência máxima se a capacidade não
distinguir os grupos. Metadados incompletos ou homogêneos conservam todos
os núcleos. Quando existe apenas um núcleo na classe mais rápida, inclui-se
a classe seguinte para não concentrar todo esse trabalho em um só núcleo.

Workers guest, auxiliares e I/O recuperam a máscara ampla de lançamento em
vez de herdar a máscara reduzida do criador. O dimensionamento dos workers
também usa a máscara original: selecionar dois núcleos para renderização
não reduz automaticamente o pool de compilação. Não se alteram nice,
classe do scheduler, clocks, cpusets do sistema ou limites térmicos.

Uma falha de afinidade deixa o kernel decidir; os registros mostram a
máscara efetiva e o erro. A política não habilita CPUs negadas pelo Android.
Se os núcleos rápidos estiverem fora da máscara inicial, não serão usados
por esta mudança. Quatro threads em dois núcleos também podem competir;
o resultado precisa ser comparado na mesma cena.

O menu **Vídeo e desempenho → Threads principais nos núcleos rápidos**
permite desligar a política e restaurar o comportamento de agendamento do
sistema na próxima abertura. A resolução, readback, patches e os quatro
experimentos anteriores permanecem independentes dessa opção.

## Verificação de execução

`TLOU CPU placement` registra a máscara inicial, o critério de seleção e a
máscara efetiva dos quatro threads críticos. O resumo já existente de cinco
segundos passa a ler quatro relógios de CPU por thread:

```
TLOU CPU critical: main=…% command=…% draw=…% recorder=…%
```

100% significa um núcleo ocupado durante a janela; -1 indica relógio
indisponível. Isso mede execução em CPU, não tempo parado em mutex/futex,
driver ou fence. Não há consultas GPU, temporização por draw ou perfil
completo habilitado. Os relógios também funcionam com a política desligada
para permitir comparação. Afinidade aceita não comprova aumento de FPS.

## Validação

`android/bionic/test_runtime_contracts.sh` verifica topologias 6+2 e de
três classes, CPU IDs não contíguos, máscara restrita, metadados ausentes,
serialização/parsing, restauração de afinidade num pthread real e opção OFF.
O teste garante que ampliar o worker não modifica a máscara do pai.
O header também compila como C++20 com warnings tratados como erros.
Os testes de preparação/patches, runtime e medidor de FPS e a compilação Java
continuam passando. A compilação Android completa e verificação do APK são
feitas no workflow nativo; desempenho e estabilidade em hardware dependem
de uma nova execução no aparelho.
