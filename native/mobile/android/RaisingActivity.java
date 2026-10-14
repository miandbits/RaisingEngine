package org.raisingengine.player;

import android.os.Bundle;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/** Materialize immutable APK assets for the shared filesystem-based renderer. */
public final class RaisingActivity extends SDLActivity {
    private File gameDirectory;

    @Override protected String[] getLibraries() { return new String[] { "SDL3", "main" }; }

    @Override protected String[] getArguments() {
        return new String[] { "--bundle", gameDirectory.getAbsolutePath() };
    }

    @Override protected void onCreate(Bundle state) {
        try {
            String buildId;
            try (InputStream input = getAssets().open("raising-build-id.txt")) {
                byte[] bytes = new byte[65];
                int count = 0, next;
                while (count < bytes.length && (next = input.read(bytes, count, bytes.length - count)) != -1) count += next;
                buildId = new String(bytes, 0, count, StandardCharsets.US_ASCII).trim();
            }
            if (!buildId.matches("[a-f0-9]{64}")) throw new IOException("Invalid game build ID");
            gameDirectory = new File(getFilesDir(), "game-" + buildId);
            File ready = new File(gameDirectory, ".ready");
            if (!ready.isFile()) {
                copyAssetDirectory("game", gameDirectory);
                if (!new File(gameDirectory, "game.json").isFile()) throw new IOException("Missing game.json");
                try (FileOutputStream marker = new FileOutputStream(ready)) {
                    marker.write(buildId.getBytes(StandardCharsets.US_ASCII));
                    marker.getFD().sync();
                }
            }
        } catch (IOException error) {
            throw new IllegalStateException("Unable to prepare Raising game assets", error);
        }
        super.onCreate(state);
    }

    private void copyAssetDirectory(String assetPath, File destination) throws IOException {
        if (!destination.isDirectory() && !destination.mkdirs()) throw new IOException("Cannot create " + destination);
        String[] names = getAssets().list(assetPath);
        if (names == null) throw new IOException("Cannot list " + assetPath);
        byte[] buffer = new byte[65536];
        for (String name : names) {
            String childPath = assetPath + "/" + name;
            File child = new File(destination, name);
            String[] children = getAssets().list(childPath);
            if (children != null && children.length > 0) {
                copyAssetDirectory(childPath, child);
            } else {
                try (InputStream input = getAssets().open(childPath); FileOutputStream output = new FileOutputStream(child)) {
                    int count;
                    while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
                }
            }
        }
    }
}
