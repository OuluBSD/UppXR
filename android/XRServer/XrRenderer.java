package org.upp.XRServer;

import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.concurrent.atomic.AtomicReference;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class XrRenderer implements GLSurfaceView.Renderer {
    private static final String TAG = "XRServer";
    private static final String VERTEX_SHADER =
        "uniform mat4 uMvp;\n" +
        "attribute vec3 aPosition;\n" +
        "attribute vec4 aColor;\n" +
        "varying vec4 vColor;\n" +
        "void main() { vColor = aColor; gl_Position = uMvp * vec4(aPosition, 1.0); }\n";
    private static final String FRAGMENT_SHADER =
        "precision mediump float;\n" +
        "varying vec4 vColor;\n" +
        "void main() { gl_FragColor = vColor; }\n";

    private final AtomicReference<XrScene> scene;
    private final XrPose pose;
    private final float[] projection = new float[16];
    private final float[] view = new float[16];
    private final float[] model = new float[16];
    private final float[] mvp = new float[16];
    private final float[] rotation = new float[16];
    private final FloatBuffer cubeVertices = floatBuffer(cubeVertices());
    private final FloatBuffer cubeColors = floatBuffer(cubeColors());

    private int program;
    private int positionLocation;
    private int colorLocation;
    private int mvpLocation;
    private int width;
    private int height;
    private long startNanos;
    private volatile boolean framebufferAsciiRequested;
    private volatile String leftEyeAscii = "No framebuffer capture requested.";
    private volatile String rightEyeAscii = "No framebuffer capture requested.";

    public XrRenderer(AtomicReference<XrScene> scene, XrPose pose) {
        this.scene = scene;
        this.pose = pose;
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        program = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
        positionLocation = GLES20.glGetAttribLocation(program, "aPosition");
        colorLocation = GLES20.glGetAttribLocation(program, "aColor");
        mvpLocation = GLES20.glGetUniformLocation(program, "uMvp");
        GLES20.glEnable(GLES20.GL_DEPTH_TEST);
        GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
        GLES20.glClearColor(0.015f, 0.02f, 0.04f, 1.0f);
        startNanos = System.nanoTime();
        checkGl("onSurfaceCreated");
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        this.width = width;
        this.height = height;
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        if (width <= 1 || height <= 1) return;
        XrScene frameScene = scene.get();
        float seconds = (System.nanoTime() - startNanos) / 1_000_000_000.0f;
        int eyeWidth = width / 2;
        for (int eye = 0; eye < 2; eye++) {
            int eyeX = eye * eyeWidth;
            GLES20.glViewport(eyeX, 0, eyeWidth, height);
            GLES20.glScissor(eyeX, 0, eyeWidth, height);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);
            drawEye(frameScene, eye == 0 ? -0.032f : 0.032f, (float) eyeWidth / height, seconds);
        }
        if (framebufferAsciiRequested) {
            framebufferAsciiRequested = false;
            leftEyeAscii = framebufferAscii(0, eyeWidth);
            rightEyeAscii = framebufferAscii(eyeWidth, eyeWidth);
        }
        checkGl("onDrawFrame");
    }

    public void requestFramebufferAscii() {
        framebufferAsciiRequested = true;
    }

    public String getLeftEyeAscii() {
        return leftEyeAscii;
    }

    public String getRightEyeAscii() {
        return rightEyeAscii;
    }

    private String framebufferAscii(int x0, int eyeWidth) {
        final String ramp = " .:-=+*#%@";
        final int columns = 32;
        final int rows = 18;
        ByteBuffer pixel = ByteBuffer.allocateDirect(4).order(ByteOrder.nativeOrder());
        StringBuilder result = new StringBuilder("\n");
        for (int row = rows - 1; row >= 0; row--) {
            int y = row * (height - 1) / (rows - 1);
            for (int column = 0; column < columns; column++) {
                int x = x0 + column * (eyeWidth - 1) / (columns - 1);
                pixel.position(0);
                GLES20.glReadPixels(x, y, 1, 1, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pixel);
                int brightness = ((pixel.get(0) & 0xff) + (pixel.get(1) & 0xff) + (pixel.get(2) & 0xff)) / 3;
                result.append(ramp.charAt(brightness * (ramp.length() - 1) / 255));
            }
            result.append('\n');
        }
        return result.toString();
    }

    private void drawEye(XrScene frameScene, float eyeOffset, float aspect, float seconds) {
        Matrix.perspectiveM(projection, 0, 45, aspect, 0.05f, 100.0f);
        Matrix.setLookAtM(view, 0, eyeOffset, 0, 0, eyeOffset, 0, -1, 0, 1, 0);
        pose.copyRotation(rotation);
        Matrix.multiplyMM(view, 0, rotation, 0, view, 0);
        // The phone faces the helmet mirror, opposite to the virtual world's
        // forward axis. Reorient the neutral optical heading towards the scene.
        Matrix.rotateM(view, 0, 180.0f, 0, 1, 0);

        GLES20.glUseProgram(program);
        for (XrScene.Object3d object : frameScene.objects) {
            Matrix.setIdentityM(model, 0);
            Matrix.translateM(model, 0, object.position[0], object.position[1], object.position[2]);
            if (object.rotationSpeed != 0) Matrix.rotateM(model, 0, seconds * object.rotationSpeed * 57.29578f, 0, 1, 0);
            Matrix.scaleM(model, 0, object.scale[0], object.scale[1], object.scale[2]);
            Matrix.multiplyMM(mvp, 0, view, 0, model, 0);
            Matrix.multiplyMM(mvp, 0, projection, 0, mvp, 0);
            GLES20.glUniformMatrix4fv(mvpLocation, 1, false, mvp, 0);
            if ("cube".equals(object.type)) drawCube(object.color);
            else drawGrid(object.color);
        }
    }

    private void drawCube(float[] color) {
        cubeVertices.position(0);
        cubeColors.position(0);
        GLES20.glVertexAttribPointer(positionLocation, 3, GLES20.GL_FLOAT, false, 0, cubeVertices);
        GLES20.glEnableVertexAttribArray(positionLocation);
        GLES20.glVertexAttribPointer(colorLocation, 4, GLES20.GL_FLOAT, false, 0, cubeColors);
        GLES20.glEnableVertexAttribArray(colorLocation);
        GLES20.glDrawArrays(GLES20.GL_TRIANGLES, 0, 36);
        GLES20.glDisableVertexAttribArray(colorLocation);
        GLES20.glDisableVertexAttribArray(positionLocation);
    }

    private void drawGrid(float[] color) {
        float[] vertices = gridVertices();
        float[] colors = new float[(vertices.length / 3) * 4];
        for (int i = 0; i < colors.length; i += 4) System.arraycopy(color, 0, colors, i, 4);
        FloatBuffer positions = floatBuffer(vertices);
        FloatBuffer tint = floatBuffer(colors);
        GLES20.glVertexAttribPointer(positionLocation, 3, GLES20.GL_FLOAT, false, 0, positions);
        GLES20.glEnableVertexAttribArray(positionLocation);
        GLES20.glVertexAttribPointer(colorLocation, 4, GLES20.GL_FLOAT, false, 0, tint);
        GLES20.glEnableVertexAttribArray(colorLocation);
        GLES20.glDrawArrays(GLES20.GL_LINES, 0, vertices.length / 3);
        GLES20.glDisableVertexAttribArray(colorLocation);
        GLES20.glDisableVertexAttribArray(positionLocation);
    }

    private int createProgram(String vertexSource, String fragmentSource) {
        int vertex = compileShader(GLES20.GL_VERTEX_SHADER, vertexSource);
        int fragment = compileShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
        int result = GLES20.glCreateProgram();
        GLES20.glAttachShader(result, vertex);
        GLES20.glAttachShader(result, fragment);
        GLES20.glLinkProgram(result);
        int[] linked = new int[1];
        GLES20.glGetProgramiv(result, GLES20.GL_LINK_STATUS, linked, 0);
        if (linked[0] == 0) throw new IllegalStateException("GLES2 program link failed: " + GLES20.glGetProgramInfoLog(result));
        return result;
    }

    private int compileShader(int type, String source) {
        int shader = GLES20.glCreateShader(type);
        GLES20.glShaderSource(shader, source);
        GLES20.glCompileShader(shader);
        int[] compiled = new int[1];
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0);
        if (compiled[0] == 0) throw new IllegalStateException("GLES2 shader compilation failed: " + GLES20.glGetShaderInfoLog(shader));
        return shader;
    }

    private void checkGl(String stage) {
        int error = GLES20.glGetError();
        if (error != GLES20.GL_NO_ERROR) Log.e(TAG, stage + " GLES2 error 0x" + Integer.toHexString(error));
    }

    private static FloatBuffer floatBuffer(float[] values) {
        ByteBuffer bytes = ByteBuffer.allocateDirect(values.length * 4).order(ByteOrder.nativeOrder());
        FloatBuffer result = bytes.asFloatBuffer();
        result.put(values).position(0);
        return result;
    }

    private static float[] cubeVertices() {
        return new float[] {
            -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
            -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
            -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,  0.5f,-0.5f,-0.5f,
             0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f,-0.5f, 0.5f
        };
    }

    private static float[] cubeColors() {
        float[] colors = new float[36 * 4];
        float[][] faces = {{1,0.2f,0.2f,1},{0.2f,1,0.3f,1},{0.3f,0.4f,1,1},{1,0.9f,0.2f,1},{1,0.2f,1,1},{0.2f,1,1,1}};
        for (int face = 0; face < 6; face++) for (int vertex = 0; vertex < 6; vertex++) System.arraycopy(faces[face], 0, colors, (face * 6 + vertex) * 4, 4);
        return colors;
    }

    private static float[] gridVertices() {
        float[] vertices = new float[3 * 21 * 2 * 3];
        int index = 0;
        for (int axis = 0; axis < 3; axis++) for (int i = -10; i <= 10; i++) {
            for (int end = -10; end <= 10; end += 20) {
                for (int component = 0; component < 3; component++) {
                    vertices[index++] = component == axis ? i : component == (axis + 1) % 3 ? end : 0;
                }
            }
        }
        return vertices;
    }
}
