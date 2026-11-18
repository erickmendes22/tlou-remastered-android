# v0.31 experimental — testes opcionais e transferência de saves

Build solicitada pelo usuário para testar os candidatos já medidos. Não é release
final nem promessa de ganho acumulado. Base v0.30, mesmo applicationId e assinatura
de desenvolvimento do CI para permitir atualização sem apagar os dados.

Vídeo e desempenho → Novos testes de desempenho v0.31:

| Opção | Variável | Base de custo ARM64 anterior |
| --- | --- | --- |
| Memória: evitar buscas repetidas | TLOU_MEMORY_WALK_ONCE | 33,47 ns por sequência clamp/copy no cenário rotativo; ~0,236 ms/frame se 7.044 tentativas tiverem esse padrão |
| Bindings: percorrer constantes em ordem | TLOU_RING_BINDING_CURSOR | ~0,070 ms/frame para 8 slots/4 ring bindings por estágio e 2 estágios gráficos + compute; distribuição real desconhecida |
| Draw: reduzir contadores internos | TLOU_DRAW_COUNTER_TRIM | 63,62 ns/pacote; ~0,237 ms/frame no cenário de um pacote por draw/dispatch; mantém medição quando perfil ativo |
| Compute: reutilizar especialização compatível | GOW3_TLOU_COMPUTE_MEMO | 87,48 ns/hit; ~0,139 ms/frame se todos os 1.589 dispatches forem compatíveis |
| Cópias host: agrupar comandos consecutivos | GOW3_TLOU_HOST_COPY_BATCH | 2 KiB/grupo16: ~0,029–0,030 ms produtor e ~0,173–0,229 ms wall do harness por frame histórico; não somar |

Todas desligadas por padrão; reiniciar o jogo para aplicar. Os cinco estados
aparecem no relatório TLOU A/B. Desativar os testes restaura os algoritmos
anteriores. Uma opção não altera as preferências das outras. Os resultados
isolados, especialmente caminhos com cache compartilhado e threads paralelas,
não são aditivos nem previsão de FPS. Os custos anteriores não medem o overhead
exato dos novos seletores de runtime desta integração.

SRT, esperas de produtor, mesclagem de escritas CPU, barreiras, modos readback,
residência e ordem Vulkan mantêm os contratos existentes. A opção de memo continua
executando o walker SRT e só reutiliza especialização com chave exata e caso seguro;
há invalidação ao atualizar programa/módulos. Cópias só se juntam no chunk ainda não
publicado, sem comandos intermediários; conclusão só é publicada após seus bytes.

Saves → Exportar / Importar ZIP está no launcher e menu lateral. O formato contém
`savedata/` relativo a `files/tlou/user`, incluindo progresso, `.sce_sys` e `.memory`.
Importação prepara e valida todos os arquivos antes de substituir a pasta ativa;
os saves anteriores são preservados numa pasta `savedata-backup-*` no diretório
do usuário do runtime. Falha na troca tenta restaurar a pasta original. Arquivos
inválidos/duplicados e caminhos externos são rejeitados. Jogo e transferências
compartilham um lock exclusivo; não é possível trocar saves enquanto o filho FEX
ainda estiver em execução.

Validação: 20 testes Python Android; contratos nativos existentes; 50 mil casos
de memória em ambos os modos; 100 mil listas de bindings; ordem/bytes/conclusão
de cópias entre threads; ZIP ida/volta com metadados e perfil, backup, caminhos
malformados, truncamento e exclusão durante jogo. CI também valida ARM64/x86 e
sanitizadores, compila e liga todo o runtime Bionic, monta/assina e verifica o APK.
Testes no dispositivo e reformulação da interface ficam para depois do feedback
desta versão, como solicitado.
