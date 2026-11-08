package org.upp.XRServer;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.net.Uri;
import android.provider.Settings;

import org.json.JSONObject;
import org.json.JSONException;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.CompletionService;
import java.util.concurrent.ExecutorCompletionService;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicReference;

public class XRServer extends Activity {
    private static final String TAG = "XRServer";
    private static final int MANAGER_PORT = 8888;
    private static final int REVERSE_CLIENT_PORT = 8889;
    private static final int PROTOCOL_VERSION = 1;

    private final AtomicReference<XrScene> scene = new AtomicReference<>(XrScene.defaultScene());
    private final XrPose pose = new XrPose();

    private GLSurfaceView glSurfaceView;
    private XrRenderer renderer;
    private SensorManager sensorManager;
    private Sensor rotationSensor;
    private ServerSocket serverSocket;
    private ExecutorService serverExecutor;
    private ExecutorService reverseExecutor;
    private volatile boolean running;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().clearFlags(android.view.WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);

        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        if (sensorManager != null) {
            rotationSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR);
        }

        glSurfaceView = new GLSurfaceView(this);
        glSurfaceView.setEGLContextClientVersion(2);
        renderer = new XrRenderer(scene, pose);
        glSurfaceView.setRenderer(renderer);
        glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        setContentView(glSurfaceView);

        running = true;
        startServer();
        startReverseClientDiscovery();
        showNetworkSettingsInfo();
    }

    @Override
    protected void onResume() {
        super.onResume();
        glSurfaceView.onResume();
        if (sensorManager != null && rotationSensor != null) {
            sensorManager.registerListener(rotationListener, rotationSensor, SensorManager.SENSOR_DELAY_GAME);
        }
    }

    @Override
    protected void onPause() {
        if (sensorManager != null) {
            sensorManager.unregisterListener(rotationListener);
        }
        glSurfaceView.onPause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        running = false;
        try {
            if (serverSocket != null) serverSocket.close();
        } catch (Exception ignored) {
        }
        if (serverExecutor != null) serverExecutor.shutdownNow();
        if (reverseExecutor != null) reverseExecutor.shutdownNow();
        super.onDestroy();
    }

    private void startServer() {
        serverExecutor = Executors.newCachedThreadPool();
        serverExecutor.submit(() -> {
            try (ServerSocket socket = new ServerSocket(MANAGER_PORT, 16, InetAddress.getByName("0.0.0.0"))) {
                serverSocket = socket;
                Log.d(TAG, "XR scene server listening on port " + MANAGER_PORT);
                while (running) {
                    Socket client = socket.accept();
                    serverExecutor.submit(() -> handleClient(client));
                }
            } catch (Exception e) {
                if (running) Log.e(TAG, "XR scene server failed", e);
            }
        });
    }

    private void showNetworkSettingsInfo() {
        SharedPreferences preferences = getPreferences(MODE_PRIVATE);
        if (preferences.getBoolean("network_settings_info_shown", false)) return;
        new AlertDialog.Builder(this)
            .setTitle("Allow XRServer network access")
            .setMessage("XRServer needs unrestricted Wi-Fi access to connect to XRClient. In app settings, allow restricted settings and enable background data/Data Saver access if those controls are present.")
            .setNegativeButton("Later", null)
            .setPositiveButton("Open app settings", (dialog, which) -> {
                preferences.edit().putBoolean("network_settings_info_shown", true).apply();
                Intent settings = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                settings.setData(Uri.parse("package:" + getPackageName()));
                startActivity(settings);
            })
            .show();
    }

    private void handleClient(Socket socket) {
        String remote = String.valueOf(socket.getRemoteSocketAddress());
        Log.d(TAG, "XR client connected: " + remote);
        try (Socket client = socket;
             BufferedReader in = new BufferedReader(new InputStreamReader(client.getInputStream(), StandardCharsets.UTF_8));
             BufferedWriter out = new BufferedWriter(new OutputStreamWriter(client.getOutputStream(), StandardCharsets.UTF_8))) {
            String line;
            while ((line = in.readLine()) != null) {
                writeResponse(out, handleMessage(line));
            }
        } catch (Exception e) {
            Log.w(TAG, "XR client disconnected: " + remote + ": " + e.getMessage());
        }
    }

    private void startReverseClientDiscovery() {
        reverseExecutor = Executors.newSingleThreadExecutor();
        reverseExecutor.submit(() -> {
            while (running) {
                Socket client = findReverseClient();
                if (client != null) {
                    Log.d(TAG, "XR reverse client connected: " + client.getRemoteSocketAddress());
                    rememberReverseClient(client.getInetAddress().getHostAddress());
                    handleClient(client);
                }
                sleepBeforeRetry();
            }
        });
    }

    private Socket findReverseClient() {
        String remembered = getPreferences(MODE_PRIVATE).getString("reverse_client_host", "");
        Socket client = connectReverseClient(remembered);
        if (client != null) return client;

        String localAddress = getWifiAddress();
        if (!localAddress.startsWith("192.168.")) {
            Log.w(TAG, "XR reverse discovery skipped: Wi-Fi address is '" + localAddress + "'.");
            return null;
        }
        int lastDot = localAddress.lastIndexOf('.');
        String subnet = localAddress.substring(0, lastDot + 1);
        Log.d(TAG, "XR reverse discovery scanning " + subnet + "0/24; remembered host=" + remembered);
        ExecutorService scanPool = Executors.newFixedThreadPool(32);
        CompletionService<Socket> completed = new ExecutorCompletionService<>(scanPool);
        int submitted = 0;
        for (int host = 1; running && host < 255; host++) {
            String candidate = subnet + host;
            if (candidate.equals(localAddress) || candidate.equals(remembered)) continue;
            completed.submit(() -> connectReverseClient(candidate));
            submitted++;
        }
        try {
            for (int i = 0; i < submitted; i++) {
                Future<Socket> result = completed.take();
                client = result.get();
                if (client != null) return client;
            }
        } catch (Exception e) {
            Log.w(TAG, "XR reverse discovery scan interrupted", e);
        } finally {
            scanPool.shutdownNow();
        }
        return null;
    }

    private Socket connectReverseClient(String host) {
        if (host == null || host.isEmpty()) return null;
        try {
            Socket socket = new Socket();
            socket.connect(new InetSocketAddress(host, REVERSE_CLIENT_PORT), 3000);
            return socket;
        } catch (Exception e) {
            if ("192.168.1.105".equals(host)) Log.d(TAG, "XR reverse connection to " + host + " failed: " + e);
            return null;
        }
    }

    private String getWifiAddress() {
        android.net.wifi.WifiManager wifi = (android.net.wifi.WifiManager) getApplicationContext().getSystemService(WIFI_SERVICE);
        if (wifi == null || wifi.getConnectionInfo() == null) return "";
        int address = wifi.getConnectionInfo().getIpAddress();
        return (address & 0xff) + "." + ((address >> 8) & 0xff) + "." + ((address >> 16) & 0xff) + "." + ((address >> 24) & 0xff);
    }

    private void rememberReverseClient(String host) {
        SharedPreferences preferences = getPreferences(MODE_PRIVATE);
        preferences.edit().putString("reverse_client_host", host).apply();
    }

    private void sleepBeforeRetry() {
        try {
            Thread.sleep(5000);
        } catch (InterruptedException ignored) {
            Thread.currentThread().interrupt();
        }
    }

    private JSONObject handleMessage(String line) {
        try {
            JSONObject message = new JSONObject(line);
            int sequence = message.optInt("sequence", -1);
            if (message.optInt("protocol", -1) != PROTOCOL_VERSION) {
                return error(sequence, "protocol_mismatch", "Expected protocol 1.");
            }

            String type = message.optString("type", "");
            if ("hello".equals(type)) {
                return ack(sequence).put("renderer", "gles2").put("sceneRevision", scene.get().revision);
            }
            if ("ping".equals(type)) {
                return ack(sequence).put("sceneRevision", scene.get().revision);
            }
            if ("snapshot".equals(type)) {
                XrScene next = XrScene.fromSnapshot(message, scene.get().revision + 1);
                scene.set(next);
                return ack(sequence).put("sceneRevision", next.revision).put("objectCount", next.objects.size());
            }
            if ("diagnostics".equals(type)) {
                return ack(sequence)
                    .put("sceneRevision", scene.get().revision)
                    .put("objectCount", scene.get().objects.size())
                    .put("renderer", "gles2")
                    .put("poseSensor", rotationSensor != null ? "rotation_vector" : "unavailable")
                    .put("leftEyeAscii", renderer.getLeftEyeAscii())
                    .put("rightEyeAscii", renderer.getRightEyeAscii());
            }
            if ("framebuffer_ascii".equals(type)) {
                renderer.requestFramebufferAscii();
                return ack(sequence).put("detail", "Capture requested; request diagnostics after one frame.");
            }
            return error(sequence, "unknown_type", "Unsupported message type '" + type + "'.");
        } catch (Exception e) {
            return error(-1, "invalid_message", e.getMessage() == null ? "Malformed JSON." : e.getMessage());
        }
    }

    private JSONObject ack(int sequence) throws JSONException {
        return new JSONObject().put("protocol", PROTOCOL_VERSION).put("type", "ack").put("sequence", sequence);
    }

    private JSONObject error(int sequence, String code, String detail) {
        try {
            return new JSONObject().put("protocol", PROTOCOL_VERSION).put("type", "error")
                .put("sequence", sequence).put("code", code).put("detail", detail);
        } catch (JSONException e) {
            throw new AssertionError("JSONObject rejected primitive error response", e);
        }
    }

    private void writeResponse(BufferedWriter out, JSONObject response) throws Exception {
        out.write(response.toString());
        out.write('\n');
        out.flush();
    }

    private final SensorEventListener rotationListener = new SensorEventListener() {
        @Override
        public void onSensorChanged(SensorEvent event) {
            if (event.sensor.getType() == Sensor.TYPE_ROTATION_VECTOR) {
                pose.setRotationVector(event.values, getDisplayRotation());
            }
        }

        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {
        }
    };

    @SuppressWarnings("deprecation")
    private int getDisplayRotation() {
        return getWindowManager().getDefaultDisplay().getRotation();
    }
}
