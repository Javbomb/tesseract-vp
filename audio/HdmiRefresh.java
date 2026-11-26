import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Looper;
import java.lang.reflect.Method;

/** Reconnects the HDMI audio output so Android reads the receiver's formats again. Exit 0 done, 2 no HDMI, 3 playing. */
@SuppressWarnings("deprecation")
public final class HdmiRefresh {
    public static void main(String[] args) {
        try {
            System.exit(refresh());
        } catch (Throwable e) {
            System.err.println("hdmi refresh failed: " + e);
            System.exit(1);
        }
    }

    private static int refresh() throws Exception {
        Looper.prepareMainLooper();
        Class<?> at = Class.forName("android.app.ActivityThread");
        Object thread = at.getMethod("systemMain").invoke(null);
        Context ctx = (Context) at.getMethod("getSystemContext").invoke(thread);
        AudioManager am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        if (am.isMusicActive()) return 3;
        int hdmi = Class.forName("android.media.AudioSystem").getField("DEVICE_OUT_HDMI").getInt(null);
        Method set = AudioManager.class.getMethod("setWiredDeviceConnectionState", int.class, int.class,
                String.class, String.class);
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            if (d.getType() != AudioDeviceInfo.TYPE_HDMI) continue;
            String addr = d.getAddress(), name = d.getProductName().toString();
            try {
                set.invoke(am, hdmi, 0, addr, name);
            } finally {
                set.invoke(am, hdmi, 1, addr, name);
            }
            return 0;
        }
        return 2;
    }
}
