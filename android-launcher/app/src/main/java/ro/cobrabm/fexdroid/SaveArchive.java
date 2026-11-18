package ro.cobrabm.fexdroid;

import java.io.*;
import java.nio.channels.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Lossless backup of runtime_savedata.c's user/savedata tree, including .sce_sys and .memory. */
final class SaveArchive {
    private static final long MAX_BYTES=2L*1024*1024*1024;
    private static final int MAX_ENTRIES=20000;
    static final class Guard implements AutoCloseable {
        final RandomAccessFile file; final FileLock lock;
        Guard(RandomAccessFile file,FileLock lock){this.file=file;this.lock=lock;}
        public void close() throws IOException {try{lock.release();}finally{file.close();}}
    }
    static Guard lock(File user) throws IOException {
        Files.createDirectories(user.toPath());
        RandomAccessFile file=new RandomAccessFile(new File(user,".savedata.lock"),"rw");
        try {
            FileLock lock;
            try{lock=file.getChannel().tryLock();}catch(OverlappingFileLockException e){lock=null;}
            if(lock==null)throw new IOException("Feche o jogo antes de importar ou exportar saves.");
            return new Guard(file,lock);
        }catch(IOException|RuntimeException e){file.close();throw e;}
    }
    static void exportZip(File user,OutputStream output) throws IOException {
        try(Guard guard=lock(user);ZipOutputStream zip=new ZipOutputStream(new BufferedOutputStream(output))) {
            Path root=new File(user,"savedata").toPath();
            if(!Files.isDirectory(root,LinkOption.NOFOLLOW_LINKS))throw new IOException("Nenhum save encontrado. Salve no jogo antes de exportar.");
            long total=0;int count=0;
            try(java.util.stream.Stream<Path> walk=Files.walk(root)) {
                Iterator<Path> entries=walk.sorted().iterator();
                while(entries.hasNext()) {
                    Path path=entries.next();
                    if(Files.isSymbolicLink(path))throw new IOException("Save contém link simbólico não suportado.");
                    if(++count>MAX_ENTRIES)throw new IOException("Quantidade de arquivos de save excedida.");
                    boolean directory=Files.isDirectory(path,LinkOption.NOFOLLOW_LINKS);
                    if(!directory&&!Files.isRegularFile(path,LinkOption.NOFOLLOW_LINKS))throw new IOException("Arquivo de save inválido.");
                    String suffix=root.relativize(path).toString().replace(File.separatorChar,'/');
                    ZipEntry entry=new ZipEntry("savedata/"+suffix+(directory&&!suffix.isEmpty()?"/":""));
                    entry.setTime(Files.getLastModifiedTime(path).toMillis());zip.putNextEntry(entry);
                    if(!directory)try(InputStream in=Files.newInputStream(path)) {
                        byte[] bytes=new byte[65536];int n;
                        while((n=in.read(bytes))!=-1){total+=n;if(total>MAX_BYTES)throw new IOException("Saves excedem 2 GiB.");zip.write(bytes,0,n);}
                    }
                    zip.closeEntry();
                }
            }
        }
    }
    static File importZip(File user,InputStream input) throws IOException {
        try(Guard guard=lock(user)) {
            Path stage=Files.createTempDirectory(user.toPath(),".save-import-");
            File backup=null;
            try {
                Set<String> seen=new HashSet<>();long total=0;int files=0,entries=0;
                try(ZipInputStream zip=new ZipInputStream(new BufferedInputStream(input))) {
                    ZipEntry entry;byte[] bytes=new byte[65536];
                    while((entry=zip.getNextEntry())!=null) {
                        if(++entries>MAX_ENTRIES)throw new IOException("Quantidade de arquivos do ZIP excedida.");
                        String name=entry.getName();
                        if(name.indexOf('\\')>=0||name.indexOf('\0')>=0||!name.startsWith("savedata/"))
                            throw new IOException("Use um ZIP de saves exportado pelo TLOU (pasta savedata).");
                        for(String part:name.split("/",-1))if(part.equals("..")||part.equals("."))throw new IOException("Caminho inválido no ZIP.");
                        Path target=stage.resolve(name).normalize();
                        if(!target.startsWith(stage)||!seen.add(target.toString()))throw new IOException("Caminho duplicado ou inválido no ZIP.");
                        if(entry.isDirectory()){Files.createDirectories(target);continue;}
                        if(target.equals(stage.resolve("savedata")))throw new IOException("Raiz de saves inválida.");
                        Files.createDirectories(target.getParent());
                        try(OutputStream out=Files.newOutputStream(target,StandardOpenOption.CREATE_NEW)) {
                            int n;while((n=zip.read(bytes))!=-1){total+=n;if(total>MAX_BYTES)throw new IOException("Saves excedem 2 GiB.");out.write(bytes,0,n);}
                        }
                        if(entry.getTime()>=0)Files.setLastModifiedTime(target,java.nio.file.attribute.FileTime.fromMillis(entry.getTime()));
                        ++files;
                    }
                }
                if(files==0)throw new IOException("O ZIP não contém arquivos de save.");
                Path current=user.toPath().resolve("savedata"),incoming=stage.resolve("savedata");
                if(Files.exists(current,LinkOption.NOFOLLOW_LINKS)) {
                    backup=new File(user,"savedata-backup-"+System.currentTimeMillis()+"-"+UUID.randomUUID());
                    Files.move(current,backup.toPath(),StandardCopyOption.ATOMIC_MOVE);
                }
                try{Files.move(incoming,current,StandardCopyOption.ATOMIC_MOVE);}
                catch(IOException e){if(backup!=null)try{Files.move(backup.toPath(),current,StandardCopyOption.ATOMIC_MOVE);}catch(IOException rollback){e.addSuppressed(rollback);}throw e;}
                return backup;
            } finally {removeTree(stage);}
        }
    }
    private static void removeTree(Path root) throws IOException {
        if(!Files.exists(root,LinkOption.NOFOLLOW_LINKS))return;
        try(java.util.stream.Stream<Path> walk=Files.walk(root)) {
            Iterator<Path> paths=walk.sorted(Comparator.reverseOrder()).iterator();
            while(paths.hasNext())Files.delete(paths.next());
        }
    }
}
