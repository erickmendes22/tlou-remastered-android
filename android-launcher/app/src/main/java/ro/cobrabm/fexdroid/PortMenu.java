package ro.cobrabm.fexdroid;

import android.app.Activity;
import android.app.Dialog;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.*;

/** Shared side drawer for launcher and gameplay. HUD/controls apply immediately. */
final class PortMenu {
    interface Actions {
        void logs(); void exportLogs(); void chooseGame(); void installPkg();
        void importDriver(); void selectDriver(); void editControls(); void resetControls(); void exitGame();
        default void importSaves() {}
        default void exportSaves() {}
        void changed();
    }
    private final Activity activity;
    private final Actions actions;
    private final boolean inGame;
    private final SharedPreferences prefs;
    private Dialog dialog;
    private LinearLayout content;
    private String section;
    PortMenu(Activity activity,boolean inGame,Actions actions) {
        this.activity=activity;this.inGame=inGame;this.actions=actions;prefs=PortSettings.prefs(activity);
    }
    private int dp(int v) { return Math.round(v*activity.getResources().getDisplayMetrics().density); }
    private TextView text(String value,int size,int color) {
        TextView v=new TextView(activity);v.setText(value);v.setTextSize(size);v.setTextColor(color);
        v.setPadding(0,dp(8),0,dp(8));return v;
    }
    void open(String initial) {
        section=initial;dialog=new Dialog(activity);dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout root=new LinearLayout(activity);root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20),dp(16),dp(20),dp(16));root.setBackgroundColor(0xff131b18);
        LinearLayout header=new LinearLayout(activity);header.setGravity(Gravity.CENTER_VERTICAL);
        TextView title=text("DAEDALUS  /  TLOU",16,0xffdce3ce);title.setTypeface(null,Typeface.BOLD);
        header.addView(title,new LinearLayout.LayoutParams(0,-2,1));
        Button close=new Button(activity);close.setText("×");close.setTextSize(24);close.setAllCaps(false);
        close.setOnClickListener(v->dialog.dismiss());header.addView(close,new LinearLayout.LayoutParams(dp(44),dp(44)));root.addView(header);
        ScrollView scroll=new ScrollView(activity);scroll.setFillViewport(true);
        content=new LinearLayout(activity);content.setOrientation(LinearLayout.VERTICAL);scroll.addView(content);
        root.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        dialog.setContentView(root);Window w=dialog.getWindow();
        if(w!=null){w.setBackgroundDrawableResource(android.R.color.transparent);w.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);WindowManager.LayoutParams lp=w.getAttributes();lp.dimAmount=.55f;lp.gravity=Gravity.RIGHT;lp.width=Math.min(dp(380),(int)(activity.getResources().getDisplayMetrics().widthPixels*.92));lp.height=-1;w.setAttributes(lp);}
        dialog.setOnDismissListener(d->actions.changed());render();dialog.show();
        if(w!=null) w.setLayout(Math.min(dp(380),(int)(activity.getResources().getDisplayMetrics().widthPixels*.92)),-1);
    }
    private void heading(String s) { content.addView(text(s,13,0xffbdce9e)); }
    private void note(String s) { content.addView(text(s,12,0xff9da99f)); }
    private void row(String label,Runnable run) {
        Button b=new Button(activity);b.setText(label);b.setAllCaps(false);b.setTextSize(14);b.setGravity(Gravity.LEFT|Gravity.CENTER_VERTICAL);
        b.setTextColor(0xffe8ece2);b.setBackgroundTintList(android.content.res.ColorStateList.valueOf(0xff24312a));
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,dp(47));lp.bottomMargin=dp(6);content.addView(b,lp);b.setOnClickListener(v->run.run());
    }
    private void action(String label,Runnable run) { row(label,()->{dialog.dismiss();run.run();}); }
    private void toggle(String key,String label,boolean value) {
        Switch s=new Switch(activity);s.setText(label);s.setTextSize(14);s.setTextColor(0xffe8ece2);s.setPadding(0,dp(12),0,dp(12));
        s.setChecked(prefs.getBoolean(key,value));s.setOnCheckedChangeListener((v,checked)->{prefs.edit().putBoolean(key,checked).apply();actions.changed();});content.addView(s);
    }
    private void choice(String key,String label,String[] names,String[] values,String fallback) {
        heading(label);Spinner spinner=new Spinner(activity,Spinner.MODE_DROPDOWN);
        ArrayAdapter<String> adapter=new ArrayAdapter<>(activity,android.R.layout.simple_spinner_dropdown_item,names);spinner.setAdapter(adapter);
        String current=prefs.getString(key,fallback);int selected=0;for(int i=0;i<values.length;i++)if(values[i].equals(current))selected=i;
        spinner.setSelection(selected);spinner.setTag(selected);
        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){
            public void onNothingSelected(AdapterView<?> p){}
            public void onItemSelected(AdapterView<?> p,View v,int index,long id){if(((Integer)spinner.getTag())!=index){spinner.setTag(index);prefs.edit().putString(key,values[index]).apply();actions.changed();}}
        });content.addView(spinner,new LinearLayout.LayoutParams(-1,dp(46)));
    }
    private void slider(String key,String label,int min,int max,int fallback) {
        TextView title=text(label+": "+prefs.getInt(key,fallback),13,0xffe8ece2);content.addView(title);
        SeekBar bar=new SeekBar(activity);bar.setMax(max-min);bar.setProgress(prefs.getInt(key,fallback)-min);
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onStartTrackingTouch(SeekBar b){} public void onStopTrackingTouch(SeekBar b){actions.changed();}
            public void onProgressChanged(SeekBar b,int value,boolean user){if(user){prefs.edit().putInt(key,value+min).apply();title.setText(label+": "+(value+min));actions.changed();}}
        });content.addView(bar,new LinearLayout.LayoutParams(-1,dp(38)));
    }
    private void go(String s) { section=s;render(); }
    private void render() {
        content.removeAllViews();
        if(!section.equals("home"))row("‹  Menu",()->go("home"));
        if(section.equals("home")) {
            heading(inGame?"DURANTE O JOGO":"CONFIGURAÇÕES");
            row("Vídeo e desempenho",()->go("video"));row("Patches do jogo",()->go("patches"));
            row("Controles e HUD",()->go("hud"));
            if(!inGame){row("Jogo e drivers",()->go("library"));row("Saves · importar / exportar",()->go("saves"));}
            row("Análise de desempenho",()->go("profiling"));
            action("Ver logs",actions::logs);if(!inGame)action("Exportar logs ZIP",actions::exportLogs);
            if(inGame)action("Voltar ao launcher",actions::exitGame);
            note("TLOU Remastered  ·  Daedalus v0.31 experimental");
        } else if(section.equals("video")) {
            heading("VÍDEO E DESEMPENHO");note("Aplicados ao iniciar o jogo. Compare as opções no mesmo cenário.");
            choice("resolution","Resolução interna (patch de eboot)",new String[]{"Original (referência)","1280 × 720 · experimental","960 × 540 · experimental","640 × 360 · experimental"},new String[]{"original","720","540","360"},"original");
            note("540p/360p podem causar bugs e reduzir FPS: o patch altera dimensões internas, mas não garante que todos os buffers e passes sejam reduzidos. Volte para Original se houver regressão.");
            note("Readback: 1 · Relaxado (fixo). O Readback 2 foi removido porque causava bugs gráficos e perda de FPS.");
            toggle("mapped_readbacks","Readback direto de buffers",true);
            note("Lê a memória compartilhada após esperar os dados da GPU. Desligue para comparar a cópia intermediária anterior; requer reabrir o jogo.");
            row("Novos testes de desempenho · v0.31",()->go("perf31"));
            row("Experimentos anteriores",()->go("experiments"));
            choice("fps_limit","Limite de FPS",new String[]{"30 FPS","60 FPS","Sem limite de apresentação"},new String[]{"30","60","0"},"60");
            toggle("vsync","VSync",true);toggle("async_shaders","Compilação assíncrona de shaders",true);
            choice("frames_ahead","Fila de renderização",new String[]{"0 · Menor latência","1 · Padrão","2 · Maior fila / latência"},new String[]{"0","1","2"},"1");
            toggle("fullscreen","Preencher tela (esticar)",false);
            note("Preencher tela é aplicado na hora. O limite de apresentação não desbloqueia a lógica de FPS do jogo.");
        } else if(section.equals("saves")) {
            heading("SAVES DO JOGO");
            note("Feche o jogo antes de transferir saves. O ZIP inclui progresso, perfil e metadados. Importar substitui os saves ativos e mantém uma cópia de segurança dos anteriores.");
            action("Exportar saves ZIP",actions::exportSaves);
            action("Importar saves ZIP",actions::importSaves);
            note("Para importar, selecione um ZIP exportado pelo TLOU com a pasta savedata.");
        } else if(section.equals("perf31")) {
            heading("TESTES DE DESEMPENHO · v0.31");
            note("Opções independentes, desligadas por padrão. Reinicie o jogo após mudar. Compare primeiro uma por vez na mesma cena, com Medir passes da GPU desligado.");
            toggle("memory_walk_once","Memória: evitar buscas repetidas",false);
            note("Percorre os mesmos mapeamentos sob a trava original. Mantém a cópia e o tratamento de lacunas.");
            toggle("ring_binding_cursor","Bindings: percorrer constantes em ordem",false);
            note("Reduz consultas dentro do mesmo draw. Não reutiliza dados ou descritores de outros draws.");
            toggle("draw_counter_trim","Draw: reduzir contadores internos",false);
            note("Evita medições por pacote quando o relatório de perfil está desligado. Com perfil ligado, mantém os contadores.");
            toggle("compute_perm_memo","Compute: reutilizar especialização compatível",false);
            note("Reaproveita a especialização somente com descritores e estado idênticos. Leituras SRT e sincronização continuam ativas.");
            toggle("host_copy_batch","Cópias host: agrupar comandos consecutivos",false);
            note("Agrupa somente cópias adjacentes na fila. Pode piorar sequências com poucas cópias; mantenha apenas se ajudar no seu teste.");
            row("Desativar os cinco testes v0.31",()->{
                prefs.edit().putBoolean("memory_walk_once",false).putBoolean("ring_binding_cursor",false)
                    .putBoolean("draw_counter_trim",false).putBoolean("compute_perm_memo",false)
                    .putBoolean("host_copy_batch",false).apply();actions.changed();render();
            });
        } else if(section.equals("experiments")) {
            heading("OTIMIZAÇÕES EXPERIMENTAIS");
            note("SRT temporal desligado por padrão: o teste de 04/11 reduziu o número de leituras, mas não trouxe ganho demonstrável de FPS nem reduziu materialmente as esperas de GPU. Reinicie para aplicar.");
            toggle("srt_hot_readbacks","SRT: juntar leituras recentes na mesma espera GPU",false);
            note("Aproveita um readback obrigatório para trazer pequenas tabelas recentes com produtor já registrado. Exclui páginas alteradas pela CPU ou com readbacks pendentes; desative se observar qualquer artefato ou travamento.");
            toggle("fetch_no_block","GoW III: espera de escrita fora da thread GPU",false);
            note("Movimenta para a thread CPU uma espera por escrita GPU já submetida; exige fila de cópia compatível. Até 3 tentativas, depois usa o caminho original.");
            toggle("land_on_guest","GoW III: concluir readbacks na thread solicitante",false);
            note("Quando uma página está protegida por um readback pendente, espera somente trabalho já submetido na thread solicitante; depois revalida ou usa o caminho original.");
            toggle("static_min_512","Reutilizar buffers estáticos pequenos (mínimo 512 B)",false);
            note("Do GoW III: promove à arena buffers de leitura que não mudaram por 3 submissões, com rastreamento de escritas. O mínimo normal é 1024 B.");
            toggle("stack_mapped_readback","Buffer na pilha para readbacks pequenos (até 512 B)",false);
            note("Evita alocação temporária para as leituras mapeadas pequenas; não muda esperas, dados nem mesclagem.");
            toggle("inline_single_copy","Registro leve de cópias GPU pequenas (até 64 KiB)",false);
            note("Evita uma reserva e uma cópia do descritor no gravador Vulkan. Mantém transferências e barreiras; desligado por padrão.");
            toggle("split_cs64","Compute de vértices: grupos de 64 (2 shaders)",false);
            note("Experimento isolado, desligado por padrão: divide o grupo virtual de 1024 em 16 de 64, apenas nos dois shaders mais caros. Dispatch indireto permanece original. Reinicie e compare com o perfil GPU desligado.");
            toggle("mapped_fault_readbacks","Readback direto para acessos da CPU",false);
            note("Evita cópias intermediárias quando a CPU precisa ler dados da GPU. Compare separadamente; requer reabrir o jogo.");
            toggle("prep_pool","Reutilizar buffers de preparação de draws",false);
            toggle("cpu_trims","Reduzir trabalho de CPU por draw",false);
            toggle("prefetch_pipelines","Pré-compilar pipelines antecipadamente",false);
            toggle("parallel_pipelines","Compilar pipelines paralelamente (risco de glitches)",false);
            choice("prep_workers","Workers de preparação de draws",
                new String[]{"Automático (original)","0 · Sem workers","2 workers","4 workers","6 workers"},
                new String[]{"auto","0","2","4","6"},"auto");
            note("Prefetch e compilação paralela podem mudar travamentos e correção visual. O modo automático de workers é o original.");
            row("Desativar todos os experimentos",()->{
                prefs.edit().putBoolean("memory_walk_once",false).putBoolean("ring_binding_cursor",false).putBoolean("draw_counter_trim",false).putBoolean("compute_perm_memo",false).putBoolean("host_copy_batch",false).putBoolean("fetch_no_block",false).putBoolean("land_on_guest",false).putBoolean("static_min_512",false).putBoolean("stack_mapped_readback",false).putBoolean("inline_single_copy",false).putBoolean("split_cs64",false).putBoolean("srt_hot_readbacks",false).remove("compute_wait_fast").remove("relaxed_skinning_fp").putBoolean("mapped_fault_readbacks",false).putBoolean("descriptor_bulk",false).putBoolean("prep_pool",false).putBoolean("cpu_trims",false)
                    .putBoolean("prefetch_pipelines",false).putBoolean("parallel_pipelines",false)
                    .putString("prep_workers","auto").apply();
                actions.changed();render();
            });
        } else if(section.equals("profiling")) {
            heading("ANÁLISE DE DESEMPENHO");
            toggle("gpu_profile","Medir passes da GPU",false);
            note("Reabra o jogo após mudar. Registra tempos por passe, shader e transferência no ZIP de logs. A medição acrescenta trabalho: deixe desligada ao comparar FPS.");
            action("Ver logs",actions::logs);
            if(!inGame)action("Exportar logs ZIP",actions::exportLogs);
        } else if(section.equals("patches")) {
            heading("PATCHES · CUSA00552 1.00");note("Adaptados do Bachata S4+. O eboot e as instruções são validados ao iniciar. Alterações exigem reabrir o jogo.");
            toggle("ssao","Desativar SSAO",false);toggle("motion_blur","Desativar motion blur",false);
            toggle("depth_of_field","Desativar profundidade de campo",false);toggle("bloom","Desativar bloom",false);
            toggle("sunlight_shadows","Desativar sombras solares",false);toggle("subsurface","Desativar subsurface",false);
            note("Sombras e subsurface alteram a iluminação e os materiais. Original mantém a aparência do jogo.");
            row("Restaurar patches e resolução original",()->{SharedPreferences.Editor e=prefs.edit().putString("resolution","original");for(String key:PortSettings.EFFECTS)e.putBoolean(key,false);e.apply();actions.changed();render();});
        } else if(section.equals("hud")) {
            heading("CONTROLES E HUD");toggle("hud_visible","Mostrar FPS",true);
            choice("hud_mode","Estilo do HUD",new String[]{"Mínimo · FPS","Compacto · FPS + ms"},new String[]{"minimal","compact"},"compact");
            choice("hud_corner","Posição",new String[]{"Superior esquerdo","Superior direito","Inferior esquerdo","Inferior direito","Personalizada (arrastar)"},new String[]{"0","1","2","3","-1"},"0");
            slider("hud_size","Tamanho do HUD",10,26,14);slider("hud_opacity","Fundo do HUD (%)",0,100,65);
            note("Arraste o HUD durante o jogo para reposicioná-lo.");
            toggle("touch_visible","Controles na tela",true);slider("touch_opacity","Opacidade dos controles (%)",15,100,55);
            if(inGame){action("Editar posição dos botões",actions::editControls);action("Restaurar posição dos botões",actions::resetControls);}
        } else if(section.equals("library")) {
            heading("JOGO E DRIVERS");action("Escolher pasta do jogo",actions::chooseGame);action("Instalar jogo por PKG",actions::installPkg);
            action("Selecionar driver Vulkan",actions::selectDriver);action("Importar driver ZIP ou .so",actions::importDriver);
        }
    }
}
