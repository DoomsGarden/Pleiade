package com.doomsgarden.pleiade.viewer;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Insets;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.view.WindowInsets;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.json.JSONObject;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.regex.Pattern;

public final class ViewerActivity extends Activity {
    private static final String HOST = "appassets.androidplatform.net";
    private static final String ASSET_PREFIX = "/viewer/";
    private static final String HOME = "https://" + HOST + ASSET_PREFIX + "index.html";
    private static final int IMPORT_DOCUMENT = 10;
    private static final int SAVE_DOCUMENT = 11;
    private static final int MAX_EXPORT_BYTES = 8 * 1024 * 1024;
    private static final int MAX_IMPORT_BYTES = 1_000_000;
    private static final Pattern ASSET_PATH = Pattern.compile("[A-Za-z0-9_-]+(?:/[A-Za-z0-9_-]+)*\\.[a-z0-9]+");
    private static final Pattern EXPORT_NAME = Pattern.compile("Pleiade-[A-Za-z0-9-]+\\.(json|png)");
    private final ExecutorService fileOperations = Executors.newSingleThreadExecutor();
    private WebView viewer;
    private byte[] exportData;

    @Override public void onCreate(Bundle savedState) {
        super.onCreate(savedState);
        FrameLayout content = new FrameLayout(this);
        content.setBackgroundColor(Color.rgb(17, 29, 39));
        viewer = new WebView(this);
        viewer.setBackgroundColor(Color.rgb(17, 29, 39));
        content.addView(viewer, new FrameLayout.LayoutParams(-1, -1));
        setContentView(content);
        if (Build.VERSION.SDK_INT >= 35) {
            content.setOnApplyWindowInsetsListener((view, insets) -> {
                Insets safe = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                Insets keyboard = insets.getInsets(WindowInsets.Type.ime());
                view.setPadding(safe.left, safe.top, safe.right, Math.max(safe.bottom, keyboard.bottom));
                return insets;
            });
            content.requestApplyInsets();
        }

        WebSettings settings = viewer.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        viewer.setWebViewClient(new LocalViewerClient());
        viewer.addJavascriptInterface(new FileBridge(), "PleiadeAndroid");
        viewer.loadUrl(HOME);
    }

    private static boolean localAsset(Uri url) {
        return "https".equals(url.getScheme()) && HOST.equals(url.getEncodedAuthority())
            && url.getPath() != null && url.getPath().startsWith(ASSET_PREFIX);
    }

    private final class LocalViewerClient extends WebViewClient {
        @Override public WebResourceResponse shouldInterceptRequest(WebView view, WebResourceRequest request) {
            Uri url = request.getUrl();
            if (!localAsset(url) || !"GET".equals(request.getMethod())) {
                return missing();
            }
            String path = url.getPath().substring(ASSET_PREFIX.length());
            if (!ASSET_PATH.matcher(path).matches()) {
                return missing();
            }
            try {
                String mime = mimeType(path);
                String charset = mime.equals("application/octet-stream") ? null : "UTF-8";
                Map<String, String> headers = path.endsWith(".html")
                    ? Collections.singletonMap("Content-Security-Policy",
                        "default-src 'self'; base-uri 'none'; form-action 'none'; object-src 'none'; "
                        + "script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:;")
                    : Collections.emptyMap();
                return new WebResourceResponse(mime, charset, 200, "OK", headers,
                                               getAssets().open("viewer/" + path));
            } catch (IOException error) {
                return missing();
            }
        }

        @Override public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
            Uri url = request.getUrl();
            if (localAsset(url) && (ASSET_PREFIX + "index.html").equals(url.getPath())) {
                return false;
            }
            if (request.isForMainFrame() && request.hasGesture() && "https".equals(url.getScheme())
                && "polyhaven.com".equals(url.getHost())) {
                try {
                    startActivity(new Intent(Intent.ACTION_VIEW, url));
                } catch (ActivityNotFoundException error) {
                    showMessage("No se encontró un navegador para abrir el enlace.");
                }
            }
            return true;
        }
    }

    private static WebResourceResponse missing() {
        return new WebResourceResponse("text/plain", "UTF-8", 404, "Not Found",
            Collections.emptyMap(), new ByteArrayInputStream("Archivo no incluido".getBytes(StandardCharsets.UTF_8)));
    }

    private static String mimeType(String path) {
        if (path.endsWith(".html")) return "text/html";
        if (path.endsWith(".js")) return "text/javascript";
        if (path.endsWith(".css")) return "text/css";
        if (path.endsWith(".json")) return "application/json";
        return "application/octet-stream";
    }

    public final class FileBridge {
        @JavascriptInterface public void openWorld() {
            runOnUiThread(() -> pickWorldFile());
        }

        @JavascriptInterface public void saveFile(String name, String mime, String encoded) {
            if (name == null || encoded == null || !EXPORT_NAME.matcher(name).matches()
                || !(("image/png".equals(mime) && name.endsWith(".png"))
                     || ("application/json".equals(mime) && name.endsWith(".json")))
                || encoded.length() > ((MAX_EXPORT_BYTES + 2) / 3) * 4) {
                runOnUiThread(() -> showMessage("El archivo no se pudo preparar."));
                return;
            }
            byte[] data;
            try {
                data = Base64.decode(encoded, Base64.NO_WRAP);
            } catch (IllegalArgumentException error) {
                runOnUiThread(() -> showMessage("El archivo no se pudo preparar."));
                return;
            }
            if (data.length <= MAX_EXPORT_BYTES) {
                runOnUiThread(() -> pickExportLocation(name, mime, data));
            } else {
                runOnUiThread(() -> showMessage("El archivo no se pudo preparar."));
            }
        }
    }

    private void pickWorldFile() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType("*/*")
            .putExtra(Intent.EXTRA_MIME_TYPES,
                      new String[]{"application/json", "text/plain", "application/octet-stream"});
        try {
            startActivityForResult(pick, IMPORT_DOCUMENT);
        } catch (ActivityNotFoundException error) {
            showMessage("No se encontró un selector de archivos.");
        }
    }

    private void pickExportLocation(String name, String mime, byte[] data) {
        if (exportData != null || isFinishing() || isDestroyed()) return;
        exportData = data;
        Intent create = new Intent(Intent.ACTION_CREATE_DOCUMENT)
            .addCategory(Intent.CATEGORY_OPENABLE)
            .setType(mime)
            .putExtra(Intent.EXTRA_TITLE, name);
        try {
            startActivityForResult(create, SAVE_DOCUMENT);
        } catch (ActivityNotFoundException error) {
            exportData = null;
            showMessage("No se encontró un selector para guardar el archivo.");
        }
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        Uri uri = result == RESULT_OK && data != null ? data.getData() : null;
        if (request == IMPORT_DOCUMENT && uri != null) {
            fileOperations.execute(() -> readWorld(uri));
        } else if (request == SAVE_DOCUMENT) {
            byte[] bytes = exportData;
            exportData = null;
            if (uri != null && bytes != null) {
                fileOperations.execute(() -> writeFile(uri, bytes));
            }
        }
    }

    private void writeFile(Uri uri, byte[] bytes) {
        try (OutputStream output = getContentResolver().openOutputStream(uri)) {
            if (output == null) throw new IOException("Unavailable document");
            output.write(bytes);
        } catch (IOException | SecurityException error) {
            runOnUiThread(() -> showMessage("No se pudo guardar el archivo."));
            return;
        }
        runOnUiThread(() -> showMessage("Archivo guardado."));
    }

    private void readWorld(Uri uri) {
        try (InputStream input = getContentResolver().openInputStream(uri)) {
            if (input == null) throw new IOException("Unavailable document");
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) != -1) {
                if (bytes.size() + count > MAX_IMPORT_BYTES) {
                    runOnUiThread(() -> showMessage("Archivo demasiado grande."));
                    return;
                }
                bytes.write(buffer, 0, count);
            }
            String json = new String(bytes.toByteArray(), StandardCharsets.UTF_8);
            String argument = JSONObject.quote(json);
            runOnUiThread(() -> {
                if (!isDestroyed()) {
                    viewer.evaluateJavascript("window.pleiade.importWorldText(" + argument + ")", null);
                }
            });
        } catch (IOException | SecurityException error) {
            runOnUiThread(() -> showMessage("No se pudo leer el archivo."));
        }
    }

    private void showMessage(String message) {
        if (!isDestroyed()) Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    @Override protected void onPause() {
        viewer.onPause();
        viewer.pauseTimers();
        super.onPause();
    }

    @Override protected void onResume() {
        super.onResume();
        viewer.resumeTimers();
        viewer.onResume();
    }

    @Override protected void onDestroy() {
        exportData = null;
        fileOperations.shutdown();
        viewer.removeJavascriptInterface("PleiadeAndroid");
        ((FrameLayout) viewer.getParent()).removeView(viewer);
        viewer.destroy();
        super.onDestroy();
    }
}
