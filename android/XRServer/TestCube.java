package org.upp.XRServer;

import javax.microedition.khronos.opengles.GL10;

public class TestCube {
    private float rotationX = 0.0f;
    private float rotationY = 0.0f;
    private float rotationZ = 0.0f;
    private float rotationSpeed = 0.5f;

    // Cube vertices (8 corners)
    private final float[] vertices = {
        -0.5f, -0.5f, -0.5f,  // 0
         0.5f, -0.5f, -0.5f,  // 1
         0.5f,  0.5f, -0.5f,  // 2
        -0.5f,  0.5f, -0.5f,  // 3
        -0.5f, -0.5f,  0.5f,  // 4
         0.5f, -0.5f,  0.5f,  // 5
         0.5f,  0.5f,  0.5f,  // 6
        -0.5f,  0.5f,  0.5f   // 7
    };

    // Face indices (6 faces, 4 vertices each = 24 vertices total)
    private final byte[] faceIndices = {
        0, 1, 2, 3,  // back face (-Z)
        4, 7, 6, 5,  // front face (+Z)
        0, 4, 5, 1,  // bottom face (-Y)
        3, 2, 6, 7,  // top face (+Y)
        0, 3, 7, 4,  // left face (-X)
        1, 5, 6, 2   // right face (+X)
    };

    // Colors for each face (R, G, B, A)
    private final float[][] faceColors = {
        {1.0f, 0.0f, 0.0f, 1.0f},  // back - RED
        {0.0f, 1.0f, 0.0f, 1.0f},  // front - GREEN
        {0.0f, 0.0f, 1.0f, 1.0f},  // bottom - BLUE
        {1.0f, 1.0f, 0.0f, 1.0f},  // top - YELLOW
        {1.0f, 0.0f, 1.0f, 1.0f},  // left - MAGENTA
        {0.0f, 1.0f, 1.0f, 1.0f}   // right - CYAN
    };

    // Normals for each face
    private final float[][] faceNormals = {
        {0.0f, 0.0f, -1.0f},  // back
        {0.0f, 0.0f, 1.0f},   // front
        {0.0f, -1.0f, 0.0f},  // bottom
        {0.0f, 1.0f, 0.0f},   // top
        {-1.0f, 0.0f, 0.0f},  // left
        {1.0f, 0.0f, 0.0f}    // right
    };

    public TestCube() {
    }

    public void update(float deltaTime) {
        rotationX += rotationSpeed * deltaTime * 30.0f;
        rotationY += rotationSpeed * deltaTime * 20.0f;
        rotationZ += rotationSpeed * deltaTime * 15.0f;
        
        if (rotationX >= 360.0f) rotationX -= 360.0f;
        if (rotationY >= 360.0f) rotationY -= 360.0f;
        if (rotationZ >= 360.0f) rotationZ -= 360.0f;
    }

    public void setRotationSpeed(float speed) {
        rotationSpeed = speed;
    }

    public void render(GL10 gl) {
        gl.glPushMatrix();
        gl.glRotatef(rotationX, 1.0f, 0.0f, 0.0f);
        gl.glRotatef(rotationY, 0.0f, 1.0f, 0.0f);
        gl.glRotatef(rotationZ, 0.0f, 0.0f, 1.0f);

        gl.glEnableClientState(GL10.GL_VERTEX_ARRAY);
        gl.glVertexPointer(3, GL10.GL_FLOAT, 0, floatBuffer(vertices));

        for (int face = 0; face < 6; face++) {
            gl.glColor4f(
                faceColors[face][0],
                faceColors[face][1],
                faceColors[face][2],
                faceColors[face][3]
            );
            gl.glNormal3f(
                faceNormals[face][0],
                faceNormals[face][1],
                faceNormals[face][2]
            );

            // Draw face as quad
            int startIdx = face * 4;
            byte[] indices = new byte[4];
            System.arraycopy(faceIndices, startIdx, indices, 0, 4);
            gl.glDrawElements(GL10.GL_TRIANGLE_FAN, 4, GL10.GL_UNSIGNED_BYTE, byteBuffer(indices));
        }

        gl.glDisableClientState(GL10.GL_VERTEX_ARRAY);
        gl.glPopMatrix();
    }

    private java.nio.FloatBuffer floatBuffer(float[] arr) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.allocateDirect(arr.length * 4);
        bb.order(java.nio.ByteOrder.nativeOrder());
        java.nio.FloatBuffer fb = bb.asFloatBuffer();
        fb.put(arr);
        fb.position(0);
        return fb;
    }

    private java.nio.ByteBuffer byteBuffer(byte[] arr) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.allocateDirect(arr.length);
        bb.order(java.nio.ByteOrder.nativeOrder());
        bb.put(arr);
        bb.position(0);
        return bb;
    }
}