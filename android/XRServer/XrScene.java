package org.upp.XRServer;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public final class XrScene {
    public static final class Object3d {
        public final String id;
        public final String type;
        public final float[] position;
        public final float[] scale;
        public final float[] color;
        public final float rotationSpeed;

        Object3d(String id, String type, float[] position, float[] scale, float[] color, float rotationSpeed) {
            this.id = id;
            this.type = type;
            this.position = position;
            this.scale = scale;
            this.color = color;
            this.rotationSpeed = rotationSpeed;
        }
    }

    public final long revision;
    public final List<Object3d> objects;

    private XrScene(long revision, List<Object3d> objects) {
        this.revision = revision;
        this.objects = Collections.unmodifiableList(objects);
    }

    public static XrScene defaultScene() {
        List<Object3d> objects = new ArrayList<>();
        objects.add(new Object3d("default-cube", "cube", new float[] {0, 0, -3}, new float[] {1, 1, 1},
            new float[] {0.2f, 0.8f, 1.0f, 1.0f}, 0.7f));
        return new XrScene(0, objects);
    }

    public static XrScene fromSnapshot(JSONObject message, long revision) throws Exception {
        JSONArray input = message.getJSONArray("objects");
        if (input.length() > 2048) throw new Exception("Snapshot has more than 2048 objects.");

        List<Object3d> objects = new ArrayList<>();
        Set<String> ids = new HashSet<>();
        for (int i = 0; i < input.length(); i++) {
            JSONObject item = input.getJSONObject(i);
            String id = item.getString("id");
            String type = item.getString("type");
            if (id.isEmpty() || id.length() > 128 || !ids.add(id)) throw new Exception("Object id must be unique and at most 128 characters.");
            if (!"cube".equals(type) && !"grid".equals(type)) throw new Exception("Unsupported object type '" + type + "'.");
            objects.add(new Object3d(id, type, vector(item, "position", 0, 0, -3), vector(item, "scale", 1, 1, 1),
                vector(item, "color", 1, 1, 1, 1), (float) item.optDouble("rotationSpeed", 0)));
        }
        return new XrScene(revision, objects);
    }

    private static float[] vector(JSONObject object, String key, float... defaults) throws Exception {
        JSONArray input = object.optJSONArray(key);
        if (input == null) return defaults;
        if (input.length() != defaults.length) throw new Exception("'" + key + "' must contain " + defaults.length + " values.");
        float[] output = new float[defaults.length];
        for (int i = 0; i < output.length; i++) {
            output[i] = (float) input.getDouble(i);
            if (!Float.isFinite(output[i])) throw new Exception("'" + key + "' contains a non-finite value.");
        }
        return output;
    }
}
