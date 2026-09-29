package org.glfrontier;

import android.content.pm.ActivityInfo;
import android.os.Bundle;

import org.libsdl.app.SDLActivity;

/*
 * SDL's own activity; SDL is linked statically into libGLFrontier.so, which
 * also holds SDL_main (the game's main()), so that is the only library.
 */
public class GLFrontierActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "GLFrontier" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

    /* Always landscape (either way up): SDL would otherwise pick the
       orientation from the window size when the window is made */
    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }
}
