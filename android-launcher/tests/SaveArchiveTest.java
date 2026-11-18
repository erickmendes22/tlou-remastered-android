package ro.cobrabm.fexdroid;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

public final class SaveArchiveTest {
    private static void require(boolean value){if(!value)throw new AssertionError();}
    private static byte[] zip(String... names)throws IOException {
        ByteArrayOutputStream out=new ByteArrayOutputStream();
        try(ZipOutputStream z=new ZipOutputStream(out)) {for(String name:names){z.putNextEntry(new ZipEntry(name));z.write(new byte[]{1,2,3});z.closeEntry();}}
        return out.toByteArray();
    }
    private static void rejected(File user,byte[] bytes)throws IOException {
        boolean failed=false;try{SaveArchive.importZip(user,new ByteArrayInputStream(bytes));}catch(IOException e){failed=true;}
        require(failed);require(Files.readString(user.toPath().resolve("savedata/1/CUSA00552/slot/data.bin")).equals("new progress"));
    }
    public static void main(String[] args)throws Exception {
        Path root=Files.createTempDirectory("tlou-save-test-");File source=root.resolve("source").toFile(),target=root.resolve("target").toFile();
        Map<String,byte[]> data=new LinkedHashMap<>();
        data.put("savedata/1/CUSA00552/slot/data.bin","new progress".getBytes(java.nio.charset.StandardCharsets.UTF_8));
        data.put("savedata/1/CUSA00552/slot.sce_sys/param.bin",new byte[]{0,4,-1,7});
        data.put("savedata/1/CUSA00552.memory/memory.dat",new byte[]{9,0,0,8});
        for(Map.Entry<String,byte[]> e:data.entrySet()){Path p=source.toPath().resolve(e.getKey());Files.createDirectories(p.getParent());Files.write(p,e.getValue());}
        Path empty=source.toPath().resolve("savedata/1/CUSA00552/empty");Files.createDirectories(empty);
        ByteArrayOutputStream out=new ByteArrayOutputStream();SaveArchive.exportZip(source,out);byte[] backupZip=out.toByteArray();
        Path old=target.toPath().resolve("savedata/1/CUSA00552/old-slot/data.bin");Files.createDirectories(old.getParent());Files.writeString(old,"old progress");
        File backup=SaveArchive.importZip(target,new ByteArrayInputStream(backupZip));
        require(backup!=null);require(Files.readString(backup.toPath().resolve("1/CUSA00552/old-slot/data.bin")).equals("old progress"));
        for(Map.Entry<String,byte[]> e:data.entrySet())require(Arrays.equals(e.getValue(),Files.readAllBytes(target.toPath().resolve(e.getKey()))));
        require(Files.isDirectory(target.toPath().resolve("savedata/1/CUSA00552/empty")));require(!Files.exists(old));
        rejected(target,zip("savedata/../../escape"));rejected(target,zip("/savedata/escape"));
        rejected(target,zip("savedata/1/../escape"));rejected(target,zip("savedata\\escape"));
        rejected(target,zip("savedata/1//same","savedata/1/same"));rejected(target,new byte[]{1,2,3});
        rejected(target,Arrays.copyOf(backupZip,backupZip.length/2));
        // A running game holds this same lock for the entire child-process lifetime.
        try(SaveArchive.Guard game=SaveArchive.lock(target)) {
            rejected(target,backupZip);
            boolean failed=false;try{SaveArchive.exportZip(target,new ByteArrayOutputStream());}catch(IOException e){failed=true;}require(failed);
        }
        // A failed import and released game lock leave normal export/import usable.
        out=new ByteArrayOutputStream();SaveArchive.exportZip(target,out);
        SaveArchive.importZip(target,new ByteArrayInputStream(out.toByteArray()));
        System.out.println("Save ZIP: round-trip, metadata/memory, empty directories, backup, malformed paths, truncation and running-game lock PASS");
        try(java.util.stream.Stream<Path> walk=Files.walk(root)){for(Path p:walk.sorted(Comparator.reverseOrder()).toArray(Path[]::new))Files.delete(p);}
    }
}
