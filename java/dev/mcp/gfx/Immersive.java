package dev.mcp.gfx;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/** 和 HND 移植版一样用反射——因为 NDK 编译环境里没有 android.jar */
public final class Immersive {
    public static void go(Object act) {
        try {
            Object win = act.getClass().getMethod("getWindow").invoke(act);
            Object view = win.getClass().getMethod("getDecorView").invoke(win);
            Class<?> vc = Class.forName("android.view.View");
            String[] names = {
                "SYSTEM_UI_FLAG_LAYOUT_STABLE",
                "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION",
                "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN",
                "SYSTEM_UI_FLAG_HIDE_NAVIGATION",
                "SYSTEM_UI_FLAG_FULLSCREEN",
                "SYSTEM_UI_FLAG_IMMERSIVE_STICKY"
            };
            int flags = 0;
            for (String n : names) {
                try { flags |= vc.getField(n).getInt(null); } catch (Throwable ignore) {}
            }
            Method set = vc.getMethod("setSystemUiVisibility", int.class);
            set.invoke(view, flags);
        } catch (Throwable t) { /* 失败就算了, 不能崩游戏 */ }
    }
}
