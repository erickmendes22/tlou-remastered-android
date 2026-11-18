package ro.cobrabm.fexdroid;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Transactional AdrenoTools ZIP storage, separate from replaceable runtime files. */
public final class DriverArchive {
    private static final long MAX_ARCHIVE=256L*1024*1024, MAX_TOTAL=512L*1024*1024;
    public enum Reason { INVALID_PACKAGE, WRONG_ABI, NEWER_ANDROID }
    public static final class Rejected extends IOException {
        public final Reason reason;
        Rejected(Reason reason) { super(reason.name()); this.reason=reason; }
    }
    public static final class Metadata {
        public final String name, library;
        public final int minApi;
        public Metadata(String name,String library,int minApi) {
            this.name=name; this.library=library; this.minApi=minApi;
        }
    }
    public interface MetadataReader { Metadata read(File file) throws Exception; }
    public static final class Installed {
        public final String id,name;
        public final File library;
        public final int minApi;
        Installed(String id,String name,File library,int minApi) {
            this.id=id; this.name=name; this.library=library; this.minApi=minApi;
        }
    }
    private static Rejected invalid() { return new Rejected(Reason.INVALID_PACKAGE); }
    private static File child(File root,String name) throws IOException {
        if (name.isEmpty() || name.startsWith("/") || name.contains("\\") || name.contains(":")) throw invalid();
        for (String component:name.split("/")) if (component.equals("..") || component.equals(".")) throw invalid();
        File target=new File(root,name).getCanonicalFile();
        if (!target.toPath().startsWith(root.getCanonicalFile().toPath()) || target.equals(root.getCanonicalFile())) throw invalid();
        return target;
    }
    private static void clean(File root) throws IOException {
        if (Files.isSymbolicLink(root.toPath())) { Files.delete(root.toPath()); return; }
        File[] files=root.listFiles();
        if (files!=null) for (File f:files) clean(f);
        Files.deleteIfExists(root.toPath());
    }
    private static void mkdir(File dir) throws IOException {
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create driver directory");
    }
    /** Reject other architectures and glibc builds before they can be selected. */
    public static void validateLibrary(File library) throws IOException {
        try (RandomAccessFile f=new RandomAccessFile(library,"r")) {
            byte[] head=new byte[64]; f.readFully(head);
            ByteBuffer h=ByteBuffer.wrap(head).order(ByteOrder.LITTLE_ENDIAN);
            if (h.getInt(0)!=0x464c457f || head[4]!=2 || head[5]!=1 || h.getShort(16)!=3 || h.getShort(18)!=183)
                throw new Rejected(Reason.WRONG_ABI);
            long phoff=h.getLong(32); int size=Short.toUnsignedInt(h.getShort(54)), count=Short.toUnsignedInt(h.getShort(56));
            if (count==0 || count>512 || size<56 || phoff<64 || phoff>f.length()-(long)size*count) throw invalid();
            List<long[]> loads=new ArrayList<>(); long dynamic=-1,dynamicSize=0;
            for (int i=0;i<count;i++) {
                byte[] raw=new byte[56]; f.seek(phoff+(long)i*size); f.readFully(raw);
                ByteBuffer p=ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN);
                long offset=p.getLong(8),bytes=p.getLong(32);
                if (offset<0 || bytes<0 || offset>f.length()-bytes) throw invalid();
                if (p.getInt(0)==1) loads.add(new long[]{p.getLong(16),offset,bytes});
                if (p.getInt(0)==2) { dynamic=offset; dynamicSize=bytes; }
            }
            if (dynamic<0 || dynamicSize>1024*1024) throw invalid();
            long strings=-1,stringSize=0; List<Long> needed=new ArrayList<>();
            for (long offset=0;offset+16<=dynamicSize;offset+=16) {
                byte[] raw=new byte[16]; f.seek(dynamic+offset); f.readFully(raw);
                ByteBuffer d=ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN);
                long tag=d.getLong(),value=d.getLong(); if (tag==0) break;
                if (tag==1) needed.add(value);
                if (tag==5) strings=value;
                if (tag==10) stringSize=value;
            }
            long strOffset=-1;
            for (long[] load:loads) if (strings>=load[0] && strings-load[0]<load[2]) strOffset=load[1]+strings-load[0];
            if (strOffset<0 || stringSize<=0 || stringSize>MAX_TOTAL || strOffset>f.length()-stringSize) throw invalid();
            for (long offset:needed) {
                if (offset<0 || offset>=stringSize) throw invalid();
                f.seek(strOffset+offset); StringBuilder name=new StringBuilder(); boolean terminated=false;
                for (int i=0;i<512 && offset+i<stringSize;i++) {
                    int c=f.readUnsignedByte(); if (c==0) { terminated=true; break; } name.append((char)c);
                }
                if (!terminated) throw invalid();
                String n=name.toString();
                if (n.contains("/") || n.matches(".*\\.so\\.[0-9].*") || n.startsWith("ld-linux"))
                    throw new Rejected(Reason.WRONG_ABI);
            }
        } catch (EOFException e) { throw invalid(); }
    }
    public static Installed install(File root,InputStream input,int api,MetadataReader reader) throws Exception {
        root=root.getCanonicalFile();
        mkdir(root);
        File archive=File.createTempFile(".driver-", ".zip",root);
        File stage=new File(root,".stage-"+UUID.randomUUID()); mkdir(stage);
        try {
            MessageDigest digest=MessageDigest.getInstance("SHA-256");
            try (OutputStream out=new FileOutputStream(archive)) {
                byte[] buffer=new byte[65536]; long total=0; int n;
                while ((n=input.read(buffer))!=-1) {
                    total+=n; if (total>MAX_ARCHIVE) throw invalid();
                    digest.update(buffer,0,n); out.write(buffer,0,n);
                }
            }
            StringBuilder hex=new StringBuilder(); for (byte b:digest.digest()) hex.append(String.format(Locale.ROOT,"%02x",b&255));
            String id=hex.toString(); File metadata=null;
            try (ZipFile zip=new ZipFile(archive)) {
                Enumeration<? extends ZipEntry> entries=zip.entries(); Set<String> names=new HashSet<>();
                int count=0; long total=0;
                while (entries.hasMoreElements()) {
                    ZipEntry e=entries.nextElement(); if (++count>256) throw invalid();
                    File target=child(stage,e.getName());
                    if (!names.add(target.getPath()) || e.getName().split("/").length>8) throw invalid();
                    if (e.isDirectory()) { mkdir(target); continue; }
                    mkdir(target.getParentFile());
                    try (InputStream in=zip.getInputStream(e); OutputStream out=new FileOutputStream(target)) {
                        byte[] buffer=new byte[65536]; int n;
                        while ((n=in.read(buffer))!=-1) { total+=n; if (total>MAX_TOTAL) throw invalid(); out.write(buffer,0,n); }
                    }
                    if (target.getName().equals("meta.json")) { if (metadata!=null) throw invalid(); metadata=target; }
                }
            }
            if (metadata==null || metadata.length()>65536) throw invalid();
            Metadata m=reader.read(metadata);
            if (m.name==null || m.name.trim().isEmpty() || m.name.length()>200 || m.library==null || !m.library.endsWith(".so") || m.minApi<21) throw invalid();
            if (m.minApi>api) throw new Rejected(Reason.NEWER_ANDROID);
            File library=child(metadata.getParentFile(),m.library); validateLibrary(library);
            String relative=stage.toPath().relativize(library.toPath()).toString();
            Properties info=new Properties(); info.setProperty("name",m.name); info.setProperty("library",relative); info.setProperty("minApi",Integer.toString(m.minApi));
            try (OutputStream out=new FileOutputStream(new File(stage,".driver-info.properties"))) { info.store(out,"Imported Android Vulkan driver"); }
            File installed=new File(root,id);
            if (!installed.exists() && !stage.renameTo(installed)) throw new IOException("Cannot install driver");
            Installed result=read(root,id,api); validateLibrary(result.library); return result;
        } finally { clean(stage); Files.deleteIfExists(archive.toPath()); }
    }
    public static Installed read(File root,String id,int api) throws IOException {
        if (!id.matches("[0-9a-f]{64}")) throw invalid();
        File dir=child(root,id); Properties info=new Properties();
        try (InputStream in=new FileInputStream(new File(dir,".driver-info.properties"))) { info.load(in); }
        int minApi;
        try { minApi=Integer.parseInt(info.getProperty("minApi","0")); } catch (NumberFormatException e) { throw invalid(); }
        if (minApi<21) throw invalid();
        if (minApi>api) throw new Rejected(Reason.NEWER_ANDROID);
        File library=child(dir,info.getProperty("library",""));
        if (!library.isFile()) throw invalid();
        return new Installed(id,info.getProperty("name",id.substring(0,8)),library,minApi);
    }
}
