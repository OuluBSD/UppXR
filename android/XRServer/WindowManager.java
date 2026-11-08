package org.upp.XRServer;

import javax.microedition.khronos.opengles.GL10;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.List;

public class WindowManager {
    private List<Window> windows = new ArrayList<>();
    private List<String> drawCommands = new ArrayList<>();

    public void addWindow(Window w) {
        windows.add(w);
    }

    public void addDrawCommand(String cmd) {
        drawCommands.add(cmd);
        if (windows.size() > 0) {
            Window active = windows.get(windows.size() - 1);
            active.addCommand(cmd);
        }
    }

    public List<Window> getWindows() {
        return windows;
    }

    public void clearWindows() {
        windows.clear();
        drawCommands.clear();
    }

    public static class Window {
        private float x, y, z;
        private float width = 1.0f;
        private float height = 0.75f;
        private float alpha = 0.5f;
        private boolean transparent = true;
        private List<String> commands = new ArrayList<>();
        private String texturePath = "";
        private FloatBuffer vertexBuffer;
        private FloatBuffer colorBuffer;

        public Window(float x, float y, float z) {
            this.x = x; this.y = y; this.z = z;
            float[] vertices = {
                -width/2, -height/2, 0.0f,
                 width/2, -height/2, 0.0f,
                 width/2,  height/2, 0.0f,
                -width/2,  height/2, 0.0f
            };
            ByteBuffer bb = ByteBuffer.allocateDirect(vertices.length * 4);
            bb.order(ByteOrder.nativeOrder());
            vertexBuffer = bb.asFloatBuffer();
            vertexBuffer.put(vertices);
            vertexBuffer.position(0);

            float[] colors = {
                0.0f, 0.0f, 0.0f, alpha,
                0.0f, 0.0f, 0.0f, alpha,
                0.0f, 0.0f, 0.0f, alpha,
                0.0f, 0.0f, 0.0f, alpha
            };
            ByteBuffer cc = ByteBuffer.allocateDirect(colors.length * 4);
            cc.order(ByteOrder.nativeOrder());
            colorBuffer = cc.asFloatBuffer();
            colorBuffer.put(colors);
            colorBuffer.position(0);
        }

        public void setWidth(float w) { this.width = w; }
        public void setHeight(float h) { this.height = h; }
        public void setAlpha(float a) { this.alpha = a; }
        public void setTexturePath(String path) { this.texturePath = path; }

        public void addCommand(String cmd) {
            commands.add(cmd);
        }

        public void render(GL10 gl) {
            gl.glPushMatrix();
            gl.glTranslatef(x, y, z);
            gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
            gl.glEnableClientState(GL10.GL_COLOR_ARRAY);
            gl.glVertexPointer(3, GL10.GL_FLOAT, 0, vertexBuffer);
            gl.glColorPointer(4, GL10.GL_FLOAT, 0, colorBuffer);
            gl.glDrawArrays(GL10.GL_TRIANGLE_FAN, 0, 4);
            gl.glDisableClientState(GL10.GL_COLOR_ARRAY);
            gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
            renderCommands(gl);
            gl.glPopMatrix();
        }

        private void renderCommands(GL10 gl) {
            for (String cmd : commands) {
                String[] parts = cmd.split("\\s+");
                if (parts[0].equals("DRAW")) {
                    String type = parts[1];
                    switch (type) {
                        case "POINT":
                            if (parts.length >= 5) {
                                float px = Float.parseFloat(parts[2]);
                                float py = Float.parseFloat(parts[3]);
                                float pz = Float.parseFloat(parts[4]);
                                float[] v = {px, py, pz};
                                ByteBuffer bb = ByteBuffer.allocateDirect(v.length * 4);
                                bb.order(ByteOrder.nativeOrder());
                                FloatBuffer fb = bb.asFloatBuffer();
                                fb.put(v);
                                fb.position(0);
                                gl.glColor4f(1,1,1,1);
                                gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
                                gl.glVertexPointer(3, GL10.GL_FLOAT, 0, fb);
                                gl.glDrawArrays(GL10.GL_POINTS, 0, 1);
                                gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
                            }
                            break;
                        case "LINE":
                            if (parts.length >= 8) {
                                float x1 = Float.parseFloat(parts[2]);
                                float y1 = Float.parseFloat(parts[3]);
                                float z1 = Float.parseFloat(parts[4]);
                                float x2 = Float.parseFloat(parts[5]);
                                float y2 = Float.parseFloat(parts[6]);
                                float z2 = Float.parseFloat(parts[7]);
                                float[] v = {x1, y1, z1, x2, y2, z2};
                                ByteBuffer bb = ByteBuffer.allocateDirect(v.length * 4);
                                bb.order(ByteOrder.nativeOrder());
                                FloatBuffer fb = bb.asFloatBuffer();
                                fb.put(v);
                                fb.position(0);
                                gl.glColor4f(1,1,1,1);
                                gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
                                gl.glVertexPointer(3, GL10.GL_FLOAT, 0, fb);
                                gl.glDrawArrays(GL10.GL_LINES, 0, 2);
                                gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
                            }
                            break;
                        case "TRIANGLE":
                            if (parts.length >= 10) {
                                float x1 = Float.parseFloat(parts[2]);
                                float y1 = Float.parseFloat(parts[3]);
                                float z1 = Float.parseFloat(parts[4]);
                                float x2 = Float.parseFloat(parts[5]);
                                float y2 = Float.parseFloat(parts[6]);
                                float z2 = Float.parseFloat(parts[7]);
                                float x3 = Float.parseFloat(parts[8]);
                                float y3 = Float.parseFloat(parts[9]);
                                float z3 = Float.parseFloat(parts[10]);
                                float[] v = {x1, y1, z1, x2, y2, z2, x3, y3, z3};
                                ByteBuffer bb = ByteBuffer.allocateDirect(v.length * 4);
                                bb.order(ByteOrder.nativeOrder());
                                FloatBuffer fb = bb.asFloatBuffer();
                                fb.put(v);
                                fb.position(0);
                                gl.glColor4f(1,1,1,1);
                                gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
                                gl.glVertexPointer(3, GL10.GL_FLOAT, 0, fb);
                                gl.glDrawArrays(GL10.GL_TRIANGLES, 0, 3);
                                gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
                            }
                            break;
                        case "QUAD":
                            if (parts.length >= 12) {
                                float x1 = Float.parseFloat(parts[2]);
                                float y1 = Float.parseFloat(parts[3]);
                                float z1 = Float.parseFloat(parts[4]);
                                float x2 = Float.parseFloat(parts[5]);
                                float y2 = Float.parseFloat(parts[6]);
                                float z2 = Float.parseFloat(parts[7]);
                                float x3 = Float.parseFloat(parts[8]);
                                float y3 = Float.parseFloat(parts[9]);
                                float z3 = Float.parseFloat(parts[10]);
                                float x4 = Float.parseFloat(parts[11]);
                                float y4 = Float.parseFloat(parts[12]);
                                float z4 = Float.parseFloat(parts[13]);
                                float[] v = {x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4};
                                ByteBuffer bb = ByteBuffer.allocateDirect(v.length * 4);
                                bb.order(ByteOrder.nativeOrder());
                                FloatBuffer fb = bb.asFloatBuffer();
                                fb.put(v);
                                fb.position(0);
                                gl.glColor4f(1,1,1,1);
                                gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
                                gl.glVertexPointer(3, GL10.GL_FLOAT, 0, fb);
                                gl.glDrawArrays(GL10.GL_TRIANGLE_FAN, 0, 4);
                                gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
                            }
                            break;
                    }
                }
            }
        }
    }
}