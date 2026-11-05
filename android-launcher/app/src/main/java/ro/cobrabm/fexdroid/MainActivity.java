package ro.cobrabm.fexdroid;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.*;
import org.json.JSONObject;
import java.io.*;
import java.nio.file.Files;
import java.util.*;
import java.util.concurrent.Executors;

/** TLOU launcher: PKG or extracted game -> automatic on-device preparation. */
public final class MainActivity extends Activity {
    private static final int PICK_GAME=501, PICK_DRIVER=502, PICK_PKG=503, PICK_DEST=504, SAVE_LOGS=505, IMPORT_SAVES=506, EXPORT_SAVES=507;
    private static final int BACKGROUND=0xff101613, PANEL=0xff1b2520, TEXT=0xffe7e9df, MUTED=0xffa6b0a4, ACCENT=0xffc2cf96;
    private SharedPreferences prefs;
    private TextView gameInfo, driverInfo, status, permission;
    private ProgressBar progress;
    private final List<Button> buttons=new ArrayList<>();
    private final java.util.concurrent.ExecutorService worker=Executors.newSingleThreadExecutor();
    private boolean busy;
    private Runnable afterPermission;
    private Uri pendingPkg;
    private InstallTools tools;

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        prefs=getSharedPreferences("tlou",MODE_PRIVATE);
        // Keep paths chosen in the first experimental APK.
        if (!prefs.contains("game_path")) prefs.edit().putString("game_path",getPreferences(0).getString("prepared_path","")).apply();
        if (saved!=null && saved.getString("pending_pkg")!=null) pendingPkg=Uri.parse(saved.getString("pending_pkg"));
        tools=new InstallTools(this);
        getWindow().setStatusBarColor(BACKGROUND);
        getWindow().setNavigationBarColor(BACKGROUND);
        buildHome();
    }
    private void buildHome() {
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.setBackgroundColor(BACKGROUND);
        LinearLayout root=column();root.setPadding(dp(22),dp(18),dp(22),dp(24));scroll.addView(root);
        LinearLayout header=new LinearLayout(this);header.setGravity(Gravity.CENTER_VERTICAL);
        TextView brand=text("DAEDALUS  /  ANDROID",12,ACCENT);brand.setLetterSpacing(.14f);
        header.addView(brand,new LinearLayout.LayoutParams(0,-2,1));
        Button menu=new Button(this);menu.setText("☰");menu.setTextSize(23);menu.setContentDescription("Menu do emulador");
        menu.setOnClickListener(v->{if(!busy)openMenu("home");});header.addView(menu,new LinearLayout.LayoutParams(dp(48),dp(48)));buttons.add(menu);root.addView(header);
        TextView title=text("THE LAST OF US",34,TEXT);title.setTypeface(Typeface.create("sans-serif-condensed",Typeface.BOLD));root.addView(title);
        TextView subtitle=text("R E M A S T E R E D",12,ACCENT);subtitle.setPadding(0,0,0,dp(18));root.addView(subtitle);
        LinearLayout game=card(root,"SEU JOGO");gameInfo=text("",14,TEXT);gameInfo.setMaxLines(3);game.addView(gameInfo);
        add(game,"JOGAR",true,()->withAccess(()->prepareSelected(true)));
        TextView detail=text("PKG ou pasta extraída. Preparação automática no celular.",12,MUTED);game.addView(detail);
        add(game,"Escolher pasta do jogo",false,()->withAccess(()->pickFolder(PICK_GAME)));
        add(game,"Instalar por PKG",false,()->withAccess(()->pickFile(PICK_PKG)));
        TextView manual=text("Informar caminho da pasta",12,ACCENT);manual.setOnClickListener(v->{if(!busy)manualPath();});game.addView(manual);
        LinearLayout config=card(root,"CONFIGURAÇÃO");
        driverInfo=text("",13,MUTED);config.addView(driverInfo);
        add(config,"Vídeo e desempenho",false,()->openMenu("video"));
        add(config,"Patches do jogo",false,()->openMenu("patches"));
        add(config,"Saves · importar / exportar",false,()->openMenu("saves"));
        progress=new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);progress.setIndeterminate(true);progress.setVisibility(View.GONE);root.addView(progress);
        status=text("Pronto para continuar. Abra o menu lateral para drivers, HUD e controles.",13,MUTED);root.addView(status);
        permission=text("Permitir acesso aos arquivos",13,ACCENT);permission.setPadding(0,dp(12),0,dp(12));permission.setOnClickListener(v->withAccess(this::refresh));root.addView(permission);
        root.addView(text("CUSA00552  ·  Daedalus v0.31 experimental",11,MUTED));setContentView(scroll);refresh();
    }
    private void exportLogs() {
        Intent save=new Intent(Intent.ACTION_CREATE_DOCUMENT);save.setType("application/zip");save.addCategory(Intent.CATEGORY_OPENABLE);
        save.putExtra(Intent.EXTRA_TITLE,"TLOU-Android-Logs.zip");startActivityForResult(save,SAVE_LOGS);
    }
    private File saveUser(){return new File(getFilesDir(),"tlou/user");}
    private void exportSaves() {
        Intent save=new Intent(Intent.ACTION_CREATE_DOCUMENT);save.setType("application/zip");save.addCategory(Intent.CATEGORY_OPENABLE);
        save.putExtra(Intent.EXTRA_TITLE,"TLOU-Saves-"+new java.text.SimpleDateFormat("yyyyMMdd-HHmmss",java.util.Locale.ROOT).format(new Date())+".zip");
        startActivityForResult(save,EXPORT_SAVES);
    }
    private void openMenu(String section) {
        new PortMenu(this,false,new PortMenu.Actions(){
            public void logs(){showLog();} public void exportLogs(){MainActivity.this.exportLogs();}
            public void chooseGame(){withAccess(()->pickFolder(PICK_GAME));}
            public void installPkg(){withAccess(()->pickFile(PICK_PKG));}
            public void importDriver(){pickFile(PICK_DRIVER);}public void selectDriver(){MainActivity.this.selectDriver();}
            public void editControls(){}public void resetControls(){}public void exitGame(){}
            public void importSaves(){pickFile(IMPORT_SAVES);}
            public void exportSaves(){MainActivity.this.exportSaves();}
            public void changed(){refresh();}
        }).open(section);
    }
    private int dp(int value) { return Math.round(value*getResources().getDisplayMetrics().density); }
    private LinearLayout column() { LinearLayout l=new LinearLayout(this); l.setOrientation(LinearLayout.VERTICAL); return l; }
    private TextView text(String message,int size,int color) {
        TextView t=new TextView(this); t.setText(message); t.setTextSize(size); t.setTextColor(color); t.setPadding(0,dp(5),0,dp(7)); return t;
    }
    private GradientDrawable background(int color) { GradientDrawable d=new GradientDrawable(); d.setColor(color); d.setCornerRadius(dp(12)); return d; }
    private LinearLayout card(LinearLayout root,String title) {
        LinearLayout c=column(); c.setPadding(dp(16),dp(12),dp(16),dp(12)); c.setBackground(background(PANEL));
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2); lp.bottomMargin=dp(14); root.addView(c,lp);
        TextView heading=text(title,11,ACCENT); heading.setLetterSpacing(0.12f); c.addView(heading); return c;
    }
    private void add(LinearLayout parent,String label,boolean primary,Runnable action) {
        Button b=new Button(this); b.setText(label); b.setTextSize(14); b.setAllCaps(false); b.setMinHeight(dp(48));
        b.setTextColor(primary?BACKGROUND:TEXT); b.setBackgroundTintList(android.content.res.ColorStateList.valueOf(primary?ACCENT:0xff303c33));
        b.setOnClickListener(v->{if(!busy)action.run();}); LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,dp(50)); p.topMargin=dp(8); parent.addView(b,p); buttons.add(b);
    }
    private void alert(String title,String message) { if(!isFinishing()&&!isDestroyed())new AlertDialog.Builder(this).setTitle(title).setMessage(message).setPositiveButton("OK",null).show(); }
    private void update(String message) { runOnUiThread(()->{if(!isDestroyed())status.setText(message);}); }
    private boolean access() { return Build.VERSION.SDK_INT<30 || Environment.isExternalStorageManager(); }
    private void withAccess(Runnable action) {
        if(access()){action.run();return;}
        afterPermission=action;
        try { startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,Uri.parse("package:"+getPackageName()))); }
        catch(Exception e){startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));}
    }
    private void pickFile(int code) { Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT); i.setType("*/*"); i.addCategory(Intent.CATEGORY_OPENABLE); i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION); startActivityForResult(i,code); }
    private void pickFolder(int code) { Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE); i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION); startActivityForResult(i,code); }
    private void refresh() {
        if(gameInfo==null)return;
        String folder=prefs.getString("game_path","");
        gameInfo.setText(folder.isEmpty()?"Nenhum jogo selecionado":folder);
        driverInfo.setText(PortSettings.summary(this)+"\n"+prefs.getString("driver_name","Driver do sistema"));
        permission.setVisibility(access()?View.GONE:View.VISIBLE);
    }
    private void task(String message, Work work) {
        if(busy)return; busy=true; for(Button b:buttons)b.setEnabled(false); progress.setVisibility(View.VISIBLE);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); update(message);
        worker.execute(()->{try{work.run();}catch(Exception e){RunLog.failure(this,"Operação do launcher",e);update("A operação não foi concluída."); runOnUiThread(()->alert("Não foi possível concluir",e.getMessage()==null?e.toString():e.getMessage()));}
            finally{runOnUiThread(()->{busy=false;if(isDestroyed())return;for(Button b:buttons)b.setEnabled(true);progress.setVisibility(View.GONE);getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);refresh();});}});
    }
    private interface Work { void run() throws Exception; }
    private File selected() { return new File(prefs.getString("game_path","")); }
    private void chooseGame(File folder) {
        if(!InstallTools.prepared(folder)&&!new File(folder,"eboot.bin").isFile()) {alert("Pasta do jogo","Escolha a pasta que contém eboot.bin e sce_sys, ou uma pasta já preparada.");return;}
        prefs.edit().putString("game_path",folder.getAbsolutePath()).apply(); refresh(); prepareSelected(false);
    }
    private void prepareSelected(boolean launch) {
        if(busy)return;
        if(launch){RunLog.begin(this);RunLog.write(this,"Jogo selecionado: "+prefs.getString("game_path","")+" • driver="+prefs.getString("driver_name","Sistema"));}
        if(prefs.getString("game_path","").isEmpty()){alert("Selecione o jogo","Instale um PKG ou escolha a pasta do jogo.");return;}
        task("Verificando e preparando jogo…",()->{
            File folder=selected(); File prepared;
            if(InstallTools.prepared(folder)){prepared=folder;tools.applyPatches(prepared,this::update);}
            else prepared=tools.prepare(folder,this::update);
            prefs.edit().putString("prepared_path",prepared.getAbsolutePath()).apply();
            update("Pronto para jogar.");
            if(launch)runOnUiThread(()->{if(!isDestroyed()){try { RunLog.write(this,"Preparação verificada. Abrindo GameActivity: "+prepared); Intent i=new Intent(this,GameActivity.class);i.putExtra("prepared_path",prepared.getAbsolutePath());startActivity(i); } catch(Throwable error) { RunLog.failure(this,"Falha ao abrir GameActivity",error);alert("Inicialização",error.toString()); }}});
        });
    }
    private void manualPath() {
        EditText input=new EditText(this); input.setSingleLine(true); input.setText(prefs.getString("game_path",""));input.setHint("/storage/emulated/0/Jogos/CUSA00552");
        new AlertDialog.Builder(this).setTitle("Pasta do jogo").setView(input).setNegativeButton("Cancelar",null).setPositiveButton("Selecionar",(d,w)->withAccess(()->chooseGame(new File(input.getText().toString().trim())))).show();
    }
    private void installPkg(File pkg,File parent) {
        task("Instalando PKG…",()->{
            File extracted=tools.extract(pkg,parent,this::update);
            // Save extracted folder even if preparation fails; never force another extraction.
            prefs.edit().putString("game_path",extracted.getAbsolutePath()).apply();
            File prepared=tools.prepare(extracted,this::update);
            prefs.edit().putString("prepared_path",prepared.getAbsolutePath()).apply(); update("Jogo instalado e preparado. Toque em JOGAR.");
        });
    }
    private String displayName(Uri uri) {
        try(Cursor c=getContentResolver().query(uri,new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)) {if(c!=null&&c.moveToFirst())return c.getString(0);}catch(Exception ignored){}
        return "Driver importado";
    }
    private void importDriver(Uri uri) {
        String name=displayName(uri);
        task("Importando driver…",()->{
            File drivers=new File(getFilesDir(),"drivers");
            try(BufferedInputStream in=new BufferedInputStream(Objects.requireNonNull(getContentResolver().openInputStream(uri)))) {
                in.mark(4); int a=in.read(),b=in.read();in.reset();
                if(a=='P'&&b=='K') {
                    DriverArchive.Installed driver=DriverArchive.install(drivers,in,Build.VERSION.SDK_INT,file->{
                        JSONObject j=new JSONObject(new String(Files.readAllBytes(file.toPath()),java.nio.charset.StandardCharsets.UTF_8));
                        return new DriverArchive.Metadata(j.getString("name"),j.getString("libraryName"),j.getInt("minApi"));
                    });
                    prefs.edit().putString("driver_path",driver.library.getAbsolutePath()).putString("driver_name",driver.name).apply();
                } else {
                    File raw=new File(drivers,"raw");if(!raw.isDirectory()&&!raw.mkdirs())throw new IOException("Falha ao criar pasta de drivers");
                    File staged=File.createTempFile("driver-",".so",raw);
                    try {
                        try(OutputStream out=new FileOutputStream(staged)){byte[] bytes=new byte[65536];long total=0;int n;while((n=in.read(bytes))!=-1){total+=n;if(total>256L*1024*1024)throw new IOException("Driver grande demais");out.write(bytes,0,n);}}
                        DriverArchive.validateLibrary(staged);
                        Files.write(new File(staged.getPath()+".name").toPath(),name.getBytes(java.nio.charset.StandardCharsets.UTF_8));
                        prefs.edit().putString("driver_path",staged.getAbsolutePath()).putString("driver_name",name).apply();
                    }catch(Exception e){Files.deleteIfExists(staged.toPath());throw e;}
                }
            }catch(DriverArchive.Rejected e){throw new IOException(e.reason==DriverArchive.Reason.NEWER_ANDROID?"Este driver exige uma versão mais recente do Android":"Pacote inválido. Use um driver Vulkan Android ARM64 com meta.json e biblioteca .so.",e);}
            update("Driver importado e selecionado.");
        });
    }
    private void selectDriver() {
        List<String> names=new ArrayList<>(),paths=new ArrayList<>();names.add("Driver do sistema");paths.add("");
        File root=new File(getFilesDir(),"drivers");File[] dirs=root.listFiles();
        if(dirs!=null)for(File d:dirs)try{
            if(d.getName().equals("raw")){
                File[] raw=d.listFiles((dir,name)->name.endsWith(".so"));if(raw!=null)for(File f:raw){File n=new File(f.getPath()+".name");names.add(n.isFile()?new String(Files.readAllBytes(n.toPath()),java.nio.charset.StandardCharsets.UTF_8):f.getName());paths.add(f.getAbsolutePath());}
            }else if(!d.getName().startsWith(".")){DriverArchive.Installed imported=DriverArchive.read(root,d.getName(),Build.VERSION.SDK_INT);names.add(imported.name);paths.add(imported.library.getAbsolutePath());}
        }catch(Exception ignored){}
        String current=prefs.getString("driver_path","");int checked=Math.max(0,paths.indexOf(current));
        new AlertDialog.Builder(this).setTitle("Driver Vulkan").setSingleChoiceItems(names.toArray(new String[0]),checked,(dialog,index)->{
            prefs.edit().putString("driver_path",paths.get(index)).putString("driver_name",names.get(index)).apply();refresh();dialog.dismiss();
        }).setNegativeButton("Cancelar",null).show();
    }
    private File local(Uri uri,boolean tree) throws Exception {
        String raw=null;
        if("com.android.externalstorage.documents".equals(uri.getAuthority())){
            String id=tree?DocumentsContract.getTreeDocumentId(uri):DocumentsContract.getDocumentId(uri);int colon=id.indexOf(':');if(colon<1)throw new IOException("Volume inválido");
            String volume=id.substring(0,colon);if(!volume.matches("[A-Za-z0-9_-]+"))throw new IOException("Volume inválido");
            File root=volume.equalsIgnoreCase("primary")?Environment.getExternalStorageDirectory():new File("/storage",volume);
            File result=new File(root,id.substring(colon+1)).getCanonicalFile();if(!result.toPath().startsWith(root.getCanonicalFile().toPath()))throw new IOException("Pasta inválida");return result;
        }
        if(!tree){
            try{String id=DocumentsContract.getDocumentId(uri);if(id.startsWith("raw:"))raw=id.substring(4);}catch(Exception ignored){}
            if(raw==null)try(Cursor c=getContentResolver().query(uri,new String[]{"_data"},null,null,null)){if(c!=null&&c.moveToFirst())raw=c.getString(0);}catch(Exception ignored){}
        }
        if(raw!=null){File f=new File(raw).getCanonicalFile();if(f.toPath().startsWith(new File("/storage").toPath()))return f;}
        throw new IOException("Escolha um arquivo ou pasta local pelo armazenamento interno/USB. Para pastas, você também pode informar o caminho.");
    }
    private void showLog() {
        ScrollView scroll=new ScrollView(this);TextView text=new TextView(this);text.setText(RunLog.summary(this));text.setTextSize(12);text.setTextIsSelectable(true);text.setPadding(dp(16),dp(12),dp(16),dp(12));scroll.addView(text);
        new AlertDialog.Builder(this).setTitle("TLOU — logs").setView(scroll).setPositiveButton("Fechar",null).show();
    }
    @Override protected void onResume(){super.onResume();refresh();if(afterPermission!=null){Runnable action=afterPermission;afterPermission=null;if(access())action.run();}}
    @Override protected void onSaveInstanceState(Bundle state){super.onSaveInstanceState(state);if(pendingPkg!=null)state.putString("pending_pkg",pendingPkg.toString());}
    @Override protected void onActivityResult(int code,int result,Intent data){
        super.onActivityResult(code,result,data);if(result!=RESULT_OK||data==null||data.getData()==null){if(code==PICK_DEST)pendingPkg=null;return;}Uri uri=data.getData();
        try{getContentResolver().takePersistableUriPermission(uri,data.getFlags()&(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION));}catch(Exception ignored){}
        try{
            if(code==SAVE_LOGS){task("Exportando logs…",()->{java.io.OutputStream out=getContentResolver().openOutputStream(uri,"w");if(out==null)throw new IOException("Não foi possível abrir o destino");RunLog.export(this,out);update("Logs ZIP salvos.");});}
            else if(code==EXPORT_SAVES){task("Exportando saves…",()->{
                try(OutputStream out=getContentResolver().openOutputStream(uri,"w")){
                    if(out==null)throw new IOException("Não foi possível abrir o destino");
                    SaveArchive.exportZip(saveUser(),out);
                }update("Saves exportados com sucesso.");
            });}
            else if(code==IMPORT_SAVES){task("Importando saves…",()->{
                try(InputStream in=getContentResolver().openInputStream(uri)){
                    if(in==null)throw new IOException("Não foi possível abrir o ZIP");
                    File backup=SaveArchive.importZip(saveUser(),in);
                    RunLog.write(this,"Saves importados; backup anterior: "+(backup==null?"sem saves anteriores":backup.getAbsolutePath()));
                    update(backup==null?"Saves importados com sucesso.":"Saves importados. Os anteriores foram preservados em backup.");
                }
            });}
            else if(code==PICK_GAME)chooseGame(local(uri,true));
            else if(code==PICK_DRIVER)importDriver(uri);
            else if(code==PICK_PKG){pendingPkg=uri;new AlertDialog.Builder(this).setTitle("Destino do jogo").setMessage("Escolha onde criar a pasta CUSA00552. O PKG será extraído diretamente para ela; depois o app prepara o jogo automaticamente.").setPositiveButton("Escolher destino",(d,w)->pickFolder(PICK_DEST)).setNegativeButton("Cancelar",(d,w)->pendingPkg=null).show();}
            else if(code==PICK_DEST&&pendingPkg!=null){Uri pkg=pendingPkg;pendingPkg=null;installPkg(local(pkg,false),local(uri,true));}
        }catch(Exception e){alert("Seleção",e.getMessage());}
    }
    @Override protected void onDestroy(){worker.shutdown();super.onDestroy();}
}
