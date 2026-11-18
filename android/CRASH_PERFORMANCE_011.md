# TLOU Android 0.11: crash e transições pesadas

## Evidência dos logs 13/14

A resolução agora funciona: o Vulkan registra cena 1280×720, profundidade
1280×720 e MRT=2; saída/composição permanecem 1920×1080. Os 32 patches de
cada resolução e seus hashes de paridade com o Bachata permanecem intactos.

O `runtime-previous.log` do pacote 13 termina em sinal 11, offset `0xf6beb0`,
endereço `0x20`. A instrução é `mov esi,[rbx]`; o prólogo copiou RCX para RBX e
somou 0x20. O chamador em `0x92c942` carrega a fonte de transformações do campo
`+0x80` do componente, e a função percorre essa fonte quando a contagem de
saída é diferente de zero. A ausência da fonte explica o acesso NULL+0x20.
Isso não identifica por que o componente chegou sem essa fonte.

As sessões atuais dos pacotes 13/14 terminam em código 143 (SIGTERM), não em
um novo registro de SIGSEGV. Não há evidência de OOM ou de falha de Vulkan
nessas terminações. O único crash explícito é o preservado no log anterior.

| Evidência | Log 13 | Log 14 |
|---|---|---|
| Avisos idênticos do cabeçalho opcional de shaders | 1316 | 913 |
| Desenhos aguardando pipeline, acumulados nos resumos | 9085 | 9204 |
| Pipelines gráficos no fim | 2585 | 3618 |
| Programas no fim | 1826 | 2521 |
| Warm-up em segundo plano | 777/814 em 3.95 s | 1770/1835 em 7.02 s |

Todos os experimentos estavam ligados, mas `PREP_WORKERS=0`. Isso não mede
separadamente o benefício de cada opção. Os logs não têm tempos de quadro;
portanto os números acima mostram trabalho nas transições, não quantificam
o gargalo do FPS estável. As leituras de arquivos geralmente levam poucos ms
por janela de cinco segundos. Não se alterou readback, sincronização GPU,
resolução ou qualidade gráfica com base em suposição.

## Mudanças

- Proteção no início da função `0xf6be70` para fonte nula. Usa 13 bytes de
  padding imediatamente após o RET da função anterior e preserva o prólogo
  original no caminho válido. Nenhuma remoção global de assert ou retomada
  arbitrária de SIGSEGV. O campo sem fonte deixa de ser atualizado nessa
  chamada: não se inventam matrizes nem se corrige a origem do dado ausente.
- Diagnóstico fatal Android usa registradores e leitura limitada de até 16
  frames guest, sem chamar o unwinder nativo através do JIT. O relatório antigo
  terminava antes dos frames/exit; evitar esse caminho potencialmente bloqueante
  permite encerrar uma falha real e conservar os dados mínimos. Leituras de
  frames negadas pelo sistema apenas encerram a caminhada.
- A fila de gravação do cache passa a ter um único mutex para fila e condição.
  O contador anterior era incrementado sob um mutex e decrementado sem ele,
  permitindo corrida, perda de atualizações e espera ocupada. Tarefas aceitas
  são drenadas ao fechar, e envios posteriores ao fechamento são rejeitados.
- Os vetores de SPIR-V/metadados agora são movidos para a tarefa de I/O. O
  capture `[=]` anterior copiava o vetor no thread que solicita a gravação.
- O aviso de cabeçalho opcional mantém os quatro primeiros exemplos e totais
  a cada 1024 ocorrências. Falhas de tradução e outros erros continuam iguais;
  evita serializar todos os compiladores no mutex do logger a cada shader.
- Resumo leve a cada cinco segundos registra flips/s, pior intervalo, tempo de
  compilação no thread principal, tempo agregado dos workers, espera e desenhos
  pendentes. Reutiliza contadores existentes, sem perfil por draw, captura de
  quadros ou consultas de timestamp/readback GPU adicionais. Tempo de workers
  é soma entre threads e pode exceder o intervalo de parede.

## Validação

`tools/validate_tlou_crash.py <SELF>` reproduz o acesso a 0x20 no original,
verifica retorno sem modificar a saída para fonte ausente e compara 48 casos
válidos em execução x86-64 nativa: saídas idênticas byte a byte. O SELF usado
é identificado por SHA256, e todas as assinaturas do guard são verificadas.

Os testes de runtime incluem 16 mil tarefas de produtores concorrentes,
transferência de payload de 4 MiB sem cópia, erro de I/O, fechamento com fila,
reabertura e cancelamento RAII. O teste de relatório fatal usa processo filho
com prazo de dois segundos e frame inválido para verificar saída limitada.
Os testes de preparação/patches e a validação das resoluções continuam passando.

Não se mediu ganho de FPS no Android. As mudanças de desempenho reduzem custos
identificados de compilação/gravação/log nas transições; não estabelecem um
resultado para áreas já aquecidas. A origem do componente sem fonte permanece
aberta e poderá ser melhor localizada pelos registradores/callers se outra
falha ocorrer. Validação visual e de estabilidade no aparelho ainda é necessária.
