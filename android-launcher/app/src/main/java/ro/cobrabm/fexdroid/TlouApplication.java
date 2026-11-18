package ro.cobrabm.fexdroid;

import android.app.Application;

public final class TlouApplication extends Application {
    @Override public void onCreate() {
        super.onCreate();
        Thread.UncaughtExceptionHandler previous=Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread,error)->{
            RunLog.failure(this,"Exceção não tratada em "+thread.getName(),error);
            if(previous!=null)previous.uncaughtException(thread,error);
            else { android.os.Process.killProcess(android.os.Process.myPid()); System.exit(1); }
        });
    }
}
