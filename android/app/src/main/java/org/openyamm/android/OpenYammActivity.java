package org.openyamm.android;

import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

public class OpenYammActivity extends SDLActivity {
    private static final int IMMERSIVE_SYSTEM_UI_FLAGS =
        View.SYSTEM_UI_FLAG_FULLSCREEN |
        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
        View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
        View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
        View.INVISIBLE;

    private final Runnable reapplyImmersiveMode = new Runnable() {
        @Override
        public void run() {
            applyImmersiveMode();
        }
    };

    private final ViewGroup.OnHierarchyChangeListener textInputHierarchyListener =
        new ViewGroup.OnHierarchyChangeListener() {
        @Override
        public void onChildViewAdded(View parent, View child) {
            disableAutoHandwriting(child);
        }

        @Override
        public void onChildViewRemoved(View parent, View child) {
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        applyImmersiveMode();
        installTextInputViewOptions();
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveMode();
        installTextInputViewOptions();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) {
            applyImmersiveMode();
        }
    }

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        super.onSystemUiVisibilityChange(visibility);

        if ((visibility & View.SYSTEM_UI_FLAG_HIDE_NAVIGATION) == 0 ||
            (visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
            Handler handler = getWindow().getDecorView().getHandler();

            if (handler != null) {
                handler.removeCallbacks(reapplyImmersiveMode);
                handler.postDelayed(reapplyImmersiveMode, 500);
            }
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "main"
        };
    }

    @Override
    protected String[] getArguments() {
        return new String[] {};
    }

    private void applyImmersiveMode() {
        Window window = getWindow();

        if (window == null) {
            return;
        }

        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);

        if (Build.VERSION.SDK_INT >= 28) {
            window.getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        }

        View decorView = window.getDecorView();
        decorView.setSystemUiVisibility(IMMERSIVE_SYSTEM_UI_FLAGS);
        disableAutoHandwriting(decorView);

        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = window.getInsetsController();

            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }
    }

    private void installTextInputViewOptions() {
        if (Build.VERSION.SDK_INT < 33) {
            return;
        }

        if (mLayout != null) {
            mLayout.setOnHierarchyChangeListener(textInputHierarchyListener);
            disableAutoHandwriting(mLayout);
        }

        if (mTextEdit != null) {
            disableAutoHandwriting(mTextEdit);
        }
    }

    private void disableAutoHandwriting(View view) {
        if (Build.VERSION.SDK_INT < 33 || view == null) {
            return;
        }

        view.setAutoHandwritingEnabled(false);

        if (!(view instanceof ViewGroup)) {
            return;
        }

        ViewGroup group = (ViewGroup)view;

        for (int index = 0; index < group.getChildCount(); ++index) {
            disableAutoHandwriting(group.getChildAt(index));
        }
    }
}
