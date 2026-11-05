# TLOU Android: resolução do Bachata Plus 0.2.7

SELF validado: `e5f0aa322f60bd90637cc10d83de08c77885b1921084021c630e04ed3d6febf7`,
CUSA00552 base 1.00, entrada `0x56d9f0`. Nenhum binário do jogo é incluído.

## Diferença encontrada na 0.9

O APK oficial Bachata S4 Plus 0.2.7 contém dois conjuntos distintos:

| XML | Região | Perfil de resolução |
|---|---|---|
| `TheLastOfUsRemastered_CUSA00552_Performance.xml` | CUSA00552 | Simplificado: 1 escrita em 720p, 12 em 540p |
| `TheLastOfUsRemastered.xml` | CUSA00556 | Completo: 32 escritas em cada resolução |

A implementação anterior partiu do XML simplificado. A 0.9 acrescentou
construtor e despacho por enum, mas deixou intactos o setter personalizado,
a inicialização global ao carregar configurações e o consumidor que fornece
as dimensões à criação da cena. Portanto seus testes de setters não provavam
a resolução realmente usada durante o jogo. Os logs 9/10 confirmam aplicação
dos patches (23/34 escritas com todos os seis efeitos desligados), não alvos 540p.

A 0.10 usa **as mesmas 32 escritas do perfil completo do Bachata 0.2.7**,
removendo `0x400000` dos endereços XML. Todos os 32 trechos originais de cada
perfil também coincidem com o SELF CUSA00552 fornecido. O conjunto substitui
inteiramente a implementação parcial, sem acumular saltos antigos.

| Caminho | Endereços principais | Comportamento do Bachata portado |
|---|---|---|
| Construtor | `0x9103bb`–`0x910432` | Tamanhos e downsample selecionados |
| Configuração por enum | `0x9104f2` | Encaminha ao setter completo |
| Setter personalizado | `0x910740` | Força tamanho da cena mesmo em chamadas indiretas |
| Inicialização global/configuração salva | `0x910b22`–`0x910ba0` | Impede restaurar 1080p nesse caminho |
| Consumidor da cena | `0x91adbc` | Fornece tamanho selecionado à alocação |
| Buffers de saída/AA | `0x92ef1a`, `0x92ef7d`, `0x92efc0`, `0x92f00d` | Mantém tamanhos nativos 1920×1080 e 960×540 |
| Composição e AA de saída | `0x91f6e8`, `0x91a37a`, `0x9d4023` | Mantém saída nativa e seus inversos |
| Viewport de vídeo | `0x8e441c`, `0x910580` | Vídeos usam viewport nativo separado da cena |

A fidelidade inclui os parâmetros de aspecto do próprio Bachata: o caminho
por enum passa 1.0 ao setter; chamadas diretas preservam o parâmetro recebido.
Não foram introduzidas alterações adicionais de aspecto por suposição.

## Origem verificável

APK: https://github.com/erickmendes22/Bachata-S4-Plus/releases/tag/0.2.7

- SHA256 do APK: `35c6fad39430e54aba561877953323e03b322a589a44442931317450e0b952d8`
- SHA256 do XML completo: `b6e6fe68d8009128368a96d4debd78cb8998674e71d8edd3c5876a6b8b28a854`
- As duas cópias de patches no runtime (normal/user) são iguais.
- Os bytes completos também coincidem com o relatório histórico R22; a fonte
  principal desta revisão é o XML extraído do APK 0.2.7, não apenas o relatório.
- O teste de paridade compara endereço, comprimento e bytes de todas as 32
  escritas de cada perfil com hashes extraídos desse XML.

## Validação e limites

`python tools/validate_tlou_performance.py /caminho/eboot.bin` verifica todas
as assinaturas e executa: construtor, 20 combinações de modo/frequência por
resolução, setter personalizado, inicialização global, consumidor da cena,
as quatro chamadas de criação de buffers nativos e os caminhos de vídeo/AA.
Usa Unicorn 2.1.4 para fluxo/argumentos e execução nativa x86-64 com AVX para
a aritmética dos setters. Um teste mínimo mostrou VMULSS retornando zero no
Unicorn usado aqui; não se interpreta esse resultado como defeito do jogo.
A execução nativa isola os setters e substitui apenas a notificação externa
por RET; não inicia o loop do jogo. Requer host x86-64 com AVX.

O preparador continua publicando `patches.bin` atomicamente, com SHA256 do
SELF e comparação dos bytes originais. Original remove todas as escritas de
resolução, preservando somente os fixes de estabilidade. Alterações não
exigem nova extração/ligação do jogo e nunca modificam o SELF do usuário.

A 0.10 registra até quatro linhas `TLOU target audit` no renderizador: tamanho
solicitado, alvos reais de cor/profundidade, área de render, viewport, MRT e
shader. Isso distingue o alvo consumido de `Output resolution: 1920x1080`, que
é apenas configuração de saída. Não há captura de quadros, readback extra ou
espera de GPU nesse registro. Alvos nativos de saída continuam esperados;
ver apenas um alvo 1080p não significa que a cena 3D use 1080p.

Os testes verificam a implementação e a paridade com o Bachata, não substituem
validação visual Android nem medem ganho de FPS no aparelho.

## Desempenho

Readback 1/2 permanece independente da resolução. Os seis efeitos e os fixes
de jobs/timeout continuam iguais. Os experimentos de CPU/pipelines permanecem
opcionais, desligados por padrão, sem novas opções ou promessas de ganho.
Nos logs 9/10 os quatro experimentos estavam ligados e `PREP_WORKERS=0`;
esses testes não isolam o efeito individual. Esta revisão corrige somente o
conjunto de resolução e sua verificação no renderizador.
