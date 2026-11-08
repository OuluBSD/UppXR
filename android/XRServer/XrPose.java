package org.upp.XRServer;

import android.hardware.SensorManager;
import android.opengl.Matrix;
import android.view.Surface;

public class XrPose {
    private final float[] rotation = new float[16];
    private final float[] remappedRotation = new float[16];
    private final float[] leftCorrection = new float[16];
    private final float[] rightCorrection = new float[16];
    private final float[] correctedRotation = new float[16];
    private final float[] axisCorrection = new float[16];

    public XrPose() {
        Matrix.setIdentityM(rotation, 0);
    }

    public synchronized void setRotationVector(float[] vector, int displayRotation) {
        SensorManager.getRotationMatrixFromVector(rotation, vector);
        if (displayRotation == Surface.ROTATION_90) {
            SensorManager.remapCoordinateSystem(rotation, SensorManager.AXIS_Y, SensorManager.AXIS_MINUS_X, remappedRotation);
        } else if (displayRotation == Surface.ROTATION_270) {
            SensorManager.remapCoordinateSystem(rotation, SensorManager.AXIS_MINUS_Y, SensorManager.AXIS_X, remappedRotation);
        } else if (displayRotation == Surface.ROTATION_180) {
            SensorManager.remapCoordinateSystem(rotation, SensorManager.AXIS_MINUS_X, SensorManager.AXIS_MINUS_Y, remappedRotation);
        } else {
            System.arraycopy(rotation, 0, remappedRotation, 0, rotation.length);
        }
        System.arraycopy(remappedRotation, 0, rotation, 0, rotation.length);

        // GcVR's AR helmet mapping: phone orientation is tilted 45 degrees
        // downwards and reaches the eyes through a mirrored optical path.
        Matrix.setRotateM(leftCorrection, 0, 135.0f, 1, 0, 0);
        Matrix.setRotateM(rightCorrection, 0, 180.0f, 0, 0, 1);
        Matrix.multiplyMM(correctedRotation, 0, leftCorrection, 0, rotation, 0);
        Matrix.multiplyMM(rotation, 0, correctedRotation, 0, rightCorrection, 0);

        // The GVR pose basis labels Android yaw and roll oppositely. Conjugate
        // the completed optical pose so head pitch, yaw, and roll remain pitch,
        // yaw, and roll in the rendered camera basis without changing neutral.
        Matrix.setRotateM(axisCorrection, 0, 180.0f, 0, 1, 1);
        Matrix.multiplyMM(correctedRotation, 0, axisCorrection, 0, rotation, 0);
        Matrix.multiplyMM(rotation, 0, correctedRotation, 0, axisCorrection, 0);

        // Preserve the now-correct pitch while reversing the two remaining
        // GVR axes reported by the helmet: yaw and roll.
        Matrix.setRotateM(axisCorrection, 0, 180.0f, 1, 0, 0);
        Matrix.multiplyMM(correctedRotation, 0, axisCorrection, 0, rotation, 0);
        Matrix.multiplyMM(rotation, 0, correctedRotation, 0, axisCorrection, 0);
    }

    public synchronized void copyRotation(float[] destination) {
        System.arraycopy(rotation, 0, destination, 0, rotation.length);
    }
}
