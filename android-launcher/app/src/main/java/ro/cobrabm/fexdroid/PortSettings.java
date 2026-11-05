package ro.cobrabm.fexdroid;

import android.content.Context;
import android.content.SharedPreferences;
import org.json.JSONObject;
import java.io.File;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.util.Map;

final class PortSettings {
    static final String[] EFFECTS={"ssao","motion_blur","depth_of_field","bloom","sunlight_shadows","subsurface"};
    static SharedPreferences prefs(Context context) { return context.getSharedPreferences("tlou",Context.MODE_PRIVATE); }
    static File patchOptions(Context context) throws Exception {
        SharedPreferences p=prefs(context);
        JSONObject options=new JSONObject(); options.put("resolution",p.getString("resolution","original"));
        for(String key:EFFECTS) options.put(key,p.getBoolean(key,false));
        File file=new File(context.getFilesDir(),"patch-options.json");
        Files.write(file.toPath(),options.toString().getBytes(StandardCharsets.UTF_8));
        return file;
    }
    static void environment(Context context,Map<String,String> env) {
        SharedPreferences p=prefs(context);
        String resolution=p.getString("resolution","original");
        env.put("TLOU_EXPECT_SCENE_SIZE",resolution.equals("540")?"960x540":
            resolution.equals("720")?"1280x720":resolution.equals("360")?"640x360":"1920x1080");
        env.put("TLOU_MEMORY_WALK_ONCE",p.getBoolean("memory_walk_once",false)?"1":"0");
        env.put("TLOU_RING_BINDING_CURSOR",p.getBoolean("ring_binding_cursor",false)?"1":"0");
        env.put("TLOU_DRAW_COUNTER_TRIM",p.getBoolean("draw_counter_trim",false)?"1":"0");
        env.put("GOW3_TLOU_COMPUTE_MEMO",p.getBoolean("compute_perm_memo",false)?"1":"0");
        env.put("GOW3_TLOU_HOST_COPY_BATCH",p.getBoolean("host_copy_batch",false)?"1":"0");
        env.put("TLOU_MAPPED_FAULT_READBACKS",p.getBoolean("mapped_fault_readbacks",false)?"1":"0");
        env.put("TLOU_SPLIT_CS64",p.getBoolean("split_cs64",false)?"1":"0");
        env.put("TLOU_INLINE_SINGLE_COPY",p.getBoolean("inline_single_copy",false)?"1":"0");
        env.put("TLOU_STACK_MAPPED_READBACK",p.getBoolean("stack_mapped_readback",false)?"1":"0");
        env.put("GOW3_FETCH_NO_BLOCK",p.getBoolean("fetch_no_block",false)?"1":"0");
        env.put("GOW3_LAND_ON_GUEST",p.getBoolean("land_on_guest",false)?"1":"0");
        // Temporal SRT readbacks: same already-required producer tick, CPU-dirty
        // and GPU ownership checked again after the wait. Default ON.
        env.put("TLOU_SRT_HOT_READBACKS",p.getBoolean("srt_hot_readbacks",false)?"1":"0");
        env.put("TLOU_SRT_DEP_BATCH","0"); // measured zero groups, retired
        // The normal value is 1024; reducing the threshold lets small truly
        // immutable buffers reach the same tracked, write-guarded arena.
        if(p.getBoolean("static_min_512",false)) env.put("GOW3_STATIC_MIN_BYTES","512");
        else env.remove("GOW3_STATIC_MIN_BYTES");
        env.put("TLOU_MAPPED_READBACKS",p.getBoolean("mapped_readbacks",true)?"1":"0");
        // Profiling is separate from optimization and off during FPS comparisons.
        // Remove disabled keys: legacy renderer sites test presence, not their value.
        for(String key:new String[]{"GOW3_GPU_PROFILE","GOW3_FRAME_STATS","GOW3_PERF_DIAG"}) {
            if(p.getBoolean("gpu_profile",false)) env.put(key,"1"); else env.remove(key);
        }
        env.put("GOW3_VSYNC",p.getBoolean("vsync",true)?"1":"0");
        env.put("GOW3_FPS_LIMIT",p.getString("fps_limit","60"));
        env.put("GOW3_FRAMES_AHEAD",p.getString("frames_ahead","1"));
        // TLOU: precise readbacks caused graphical corruption and poorer FPS.
        // Always force relaxed regardless of an old saved "readback=2" preference.
        env.put("GOW3_READBACKS","1");
        if(p.contains("readback") || p.contains("sotc_precise_narrow")) {
            p.edit().remove("readback").remove("sotc_precise_narrow").apply();
        }
        env.put("GOW3_ASYNC_SHADERS",p.getBoolean("async_shaders",true)?"1":"0");
        // Independent opt-in knobs already implemented in the TLOU renderer.
        // All new experiments remain OFF by default.
        env.put("TLOU_DESCRIPTOR_BULK","0"); // v0.23 A/B showed no FPS gain; retired

        env.put("GOW3_PREP_POOL",p.getBoolean("prep_pool",false)?"1":"0");
        env.put("GOW3_CPU_TRIMS",p.getBoolean("cpu_trims",false)?"1":"0");
        env.put("GOW3_PREFETCH_PIPELINES",p.getBoolean("prefetch_pipelines",false)?"1":"0");
        env.put("GOW3_PARALLEL_PIPELINES",p.getBoolean("parallel_pipelines",false)?"1":"0");
        String workers=p.getString("prep_workers","auto");
        if(workers.equals("0")||workers.equals("2")||workers.equals("4")||workers.equals("6"))
            env.put("GOW3_PREP_WORKERS",workers);
        else env.remove("GOW3_PREP_WORKERS"); // original hardware-based selection
        // Keep stale/deferred image reads off; changing their semantics needs device validation.
        env.put("GOW3_DEFERRED_READBACK","0"); env.put("GOW3_STALE_READBACK","0");
    }
    static String experimentReport(Context context,Map<String,String> env) {
        StringBuilder result=new StringBuilder("TLOU A/B: resolution=")
            .append(prefs(context).getString("resolution","original"));
        for(String key:new String[]{"TLOU_MEMORY_WALK_ONCE","TLOU_RING_BINDING_CURSOR","TLOU_DRAW_COUNTER_TRIM","GOW3_TLOU_COMPUTE_MEMO","GOW3_TLOU_HOST_COPY_BATCH","TLOU_SRT_HOT_READBACKS","TLOU_SRT_DEP_BATCH","GOW3_FETCH_NO_BLOCK","GOW3_LAND_ON_GUEST","GOW3_STATIC_MIN_BYTES","TLOU_STACK_MAPPED_READBACK","TLOU_INLINE_SINGLE_COPY","TLOU_SPLIT_CS64","TLOU_MAPPED_FAULT_READBACKS","TLOU_MAPPED_READBACKS","GOW3_GPU_PROFILE","GOW3_READBACKS","GOW3_FRAMES_AHEAD",
                "GOW3_ASYNC_SHADERS","TLOU_DESCRIPTOR_BULK","GOW3_PREP_POOL","GOW3_CPU_TRIMS",
                "GOW3_PREFETCH_PIPELINES","GOW3_PARALLEL_PIPELINES",
                "GOW3_PREP_WORKERS"})
            result.append(" ").append(key).append("=")
                .append(env.containsKey(key)?env.get(key):"auto");
        return result.toString();
    }
    static String summary(Context context) {
        SharedPreferences p=prefs(context); String r=p.getString("resolution","original");
        String limit=p.getString("fps_limit","60");
        return (r.equals("original")?"Resolução original":r+"p")+"  ·  "+(limit.equals("0")?"Sem limite":limit+" FPS")+"  ·  "+
            (p.getBoolean("vsync",true)?"VSync ligado":"VSync desligado");
    }
}
