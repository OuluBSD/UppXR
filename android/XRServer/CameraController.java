package org.upp.XRServer;

import javax.microedition.khronos.opengles.GL10;

public class CameraController {
    private float fov = 90.0f;
    private float aspect = 1.0f;
    private float nearPlane = 0.1f;
    private float farPlane = 100.0f;
    private float posX = 0.0f, posY = 0.0f, posZ = 0.0f;
    private float horzAngle = 0.0f, vertAngle = 0.0f;
    private float dx = 0.035f;
    private float dy = 0.0f;
    private float cameraX = 0.0f;

    // Gyro-based orientation (corrected for Android sensor frame)
    private float gyroPitch = 0.0f;
    private float gyroYaw = 0.0f;
    private float gyroRoll = 0.0f;
    private float[] rotationMatrix = new float[16];

    public void setFOV(float fov) { this.fov = fov; }
    public void setAspect(float aspect) { this.aspect = aspect; }
    public void setHorzAngle(float angle) { this.horzAngle = angle; }
    public void setVertAngle(float angle) { this.vertAngle = angle; }
    public void setDX(float dx) { this.dx = dx; }
    public void setDY(float dy) { this.dy = dy; }
    public void setCameraX(float x) { this.cameraX = x; }

    public void setPosition(float x, float y, float z) {
        posX = x; posY = y; posZ = z;
    }

    public void updateGyro(float pitch, float yaw, float roll) {
        gyroPitch = pitch;
        gyroYaw = yaw;
        gyroRoll = roll;
        computeRotationMatrix();
    }

    private void computeRotationMatrix() {
        // Android sensor frame: X=pitch (forward), Y=yaw (left), Z=roll (up)
        // Apply correction from GcArMatrix: Quaternion.Euler(135, 0, 0) * quat * Quaternion.Euler(0, 0, 180)
        // This is equivalent to rotating 135° around X, then 180° around Z
        // In OpenGL: multiply by correction matrix on the left
        
        float cosPitch = (float) Math.cos(gyroPitch);
        float sinPitch = (float) Math.sin(gyroPitch);
        float cosYaw = (float) Math.cos(gyroYaw);
        float sinYaw = (float) Math.sin(gyroYaw);
        float cosRoll = (float) Math.cos(gyroRoll);
        float sinRoll = (float) Math.sin(gyroRoll);

        // Base rotation from gyro (ZYX order for Euler)
        float[][] R = new float[3][3];
        R[0][0] = cosYaw * cosRoll;
        R[0][1] = cosPitch * sinRoll + sinPitch * sinYaw * cosRoll;
        R[0][2] = sinPitch * sinRoll - cosPitch * sinYaw * cosRoll;
        R[1][0] = -cosYaw * sinRoll;
        R[1][1] = cosPitch * cosRoll - sinPitch * sinYaw * sinRoll;
        R[1][2] = sinPitch * cosRoll + cosPitch * sinYaw * sinRoll;
        R[2][0] = sinYaw;
        R[2][1] = -sinPitch * cosYaw;
        R[2][2] = cosPitch * cosYaw;

        // Correction: 135° around X, then 180° around Z
        // Rx(135°) = [1, 0, 0; 0, cos135, -sin135; 0, sin135, cos135]
        // cos135 = -√2/2 ≈ -0.7071, sin135 = √2/2 ≈ 0.7071
        // Rz(180°) = [-1, 0, 0; 0, -1, 0; 0, 0, 1]
        // Combined: Rz * Rx * R
        
        float sqrt2_2 = 0.70710678f;
        float[][] correction = {
            {-1.0f, 0.0f, 0.0f},
            {0.0f, -sqrt2_2, sqrt2_2},
            {0.0f, -sqrt2_2, -sqrt2_2}
        };

        // Multiply: correction * R
        float[][] finalR = new float[3][3];
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                finalR[i][j] = correction[i][0] * R[0][j] + correction[i][1] * R[1][j] + correction[i][2] * R[2][j];
            }
        }

        // Convert to 4x4 matrix
        rotationMatrix[0] = finalR[0][0]; rotationMatrix[4] = finalR[0][1]; rotationMatrix[8] = finalR[0][2]; rotationMatrix[12] = 0;
        rotationMatrix[1] = finalR[1][0]; rotationMatrix[5] = finalR[1][1]; rotationMatrix[9] = finalR[1][2]; rotationMatrix[13] = 0;
        rotationMatrix[2] = finalR[2][0]; rotationMatrix[6] = finalR[2][1]; rotationMatrix[10] = finalR[2][2]; rotationMatrix[14] = 0;
        rotationMatrix[3] = 0; rotationMatrix[7] = 0; rotationMatrix[11] = 0; rotationMatrix[15] = 1;
    }

    public void applyProjection(GL10 gl, float aspect, boolean isLeft) {
        gl.glMatrixMode(GL10.GL_PROJECTION);
        gl.glLoadIdentity();
        float halfFov = fov / 2.0f;
        float f = (float) (1.0 / Math.tan(Math.toRadians(halfFov)));
        float w = f / aspect;
        float h = f;
        float eyeOffset = isLeft ? -dx / 2.0f : dx / 2.0f;

        gl.glFrustumf(
            w * 0.5f + eyeOffset * 0.1f,
            -w * 0.5f + eyeOffset * 0.1f,
            -h * 0.5f, h * 0.5f,
            nearPlane, farPlane
        );
    }

    public void applyView(GL10 gl, float eyeOffset, boolean isLeft) {
        gl.glMatrixMode(GL10.GL_MODELVIEW);
        gl.glLoadIdentity();

        float eyeX = posX + cameraX + (isLeft ? -dx / 2.0f : dx / 2.0f);
        float lookAtX = eyeX + (float) Math.sin(Math.toRadians(horzAngle)) * 10.0f;
        float lookAtY = posY + (float) Math.sin(Math.toRadians(vertAngle)) * 10.0f;
        float lookAtZ = posZ + (float) Math.cos(Math.toRadians(horzAngle)) * 10.0f;

        float upX = 0.0f;
        float upY = (float) Math.cos(Math.toRadians(vertAngle));
        float upZ = 0.0f;

        GLU.gluLookAt(gl, eyeX, posY, posZ, lookAtX, lookAtY, lookAtZ, upX, upY, upZ);
        
        // Apply gyro rotation correction
        if (rotationMatrix[0] != 1.0f || rotationMatrix[5] != 1.0f || rotationMatrix[10] != 1.0f) {
            gl.glMultMatrixf(rotationMatrix, 0);
        }
    }

    private static class GLU {
        static void gluLookAt(GL10 gl, float eyeX, float eyeY, float eyeZ,
                               float centerX, float centerY, float centerZ,
                               float upX, float upY, float upZ) {
            float fx = centerX - eyeX;
            float fy = centerY - eyeY;
            float fz = centerZ - eyeZ;
            float flen = (float) Math.sqrt(fx * fx + fy * fy + fz * fz);
            if (flen > 0.00001f) { fx /= flen; fy /= flen; fz /= flen; }

            float sx = fy * upZ - fz * upY;
            float sy = fz * upX - fx * upZ;
            float sz = fx * upY - fy * upX;
            float slen = (float) Math.sqrt(sx * sx + sy * sy + sz * sz);
            if (slen > 0.00001f) { sx /= slen; sy /= slen; sz /= slen; }

            float ux = sy * fz - sz * fy;
            float uy = sz * fx - sx * fz;
            float uz = sx * fy - sy * fx;

            float[] m = new float[16];
            m[0] = sx; m[4] = ux; m[8] = -fx; m[12] = -(sx * eyeX + ux * eyeY + (-fx) * eyeZ);
            m[1] = sy; m[5] = uy; m[9] = -fy; m[13] = -(sy * eyeX + uy * eyeY + (-fy) * eyeZ);
            m[2] = sz; m[6] = uz; m[10] = -fz; m[14] = -(sz * eyeX + uz * eyeY + (-fz) * eyeZ);
            m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;

            gl.glLoadMatrixf(m, 0);
        }
    }
}
