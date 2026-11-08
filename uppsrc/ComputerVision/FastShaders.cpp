#include "ComputerVision.h"

NAMESPACE_UPP

const char* fast_score_map_shader = R"SH4D3R(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0, r8) uniform readonly image2D inputImage;
layout(binding = 1, r32f) uniform writeonly image2D scoreMap;

uniform float threshold;

const ivec2 offsets[16] = ivec2[](
    ivec2(0, -3), ivec2(1, -3), ivec2(2, -2), ivec2(3, -1),
    ivec2(3, 0), ivec2(3, 1), ivec2(2, 2), ivec2(1, 3),
    ivec2(0, 3), ivec2(-1, 3), ivec2(-2, 2), ivec2(-3, 1),
    ivec2(-3, 0), ivec2(-3, -1), ivec2(-2, -2), ivec2(-1, -3)
);

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(inputImage);
    if (pos.x < 3 || pos.y < 3 || pos.x >= size.x - 3 || pos.y >= size.y - 3) {
        imageStore(scoreMap, pos, vec4(0.0));
        return;
    }

    float p = imageLoad(inputImage, pos).r;
    float v[16];
    for(int i = 0; i < 16; i++)
        v[i] = imageLoad(inputImage, pos + offsets[i]).r;

    // Fast check for 9-pixel arc
    bool possible = false;
    float t = threshold / 255.0;
    
    // Brighter arc
    int count = 0;
    int max_count = 0;
    for(int i = 0; i < 25; i++) { // wrap around
        if(v[i % 16] > p + t) {
            count++;
            if(count > max_count) max_count = count;
        } else count = 0;
    }
    if(max_count >= 9) possible = true;
    
    // Darker arc
    if(!possible) {
        count = 0;
        max_count = 0;
        for(int i = 0; i < 25; i++) {
            if(v[i % 16] < p - t) {
                count++;
                if(count > max_count) max_count = count;
            } else count = 0;
        }
        if(max_count >= 9) possible = true;
    }

    float score = 0.0;
    if(possible) {
        // Heuristic score: max difference in the best arc
        // (Simplified for now, real FAST score is more complex)
        score = max_count / 16.0; 
    }
    
    imageStore(scoreMap, pos, vec4(score, 0.0, 0.0, 0.0));
}
)SH4D3R";

const char* fast_nms_shader = R"SH4D3R(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0, r32f) uniform readonly image2D scoreMap;
layout(binding = 1, r32f) uniform writeonly image2D nmsMap;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(scoreMap);
    if (pos.x < 1 || pos.y < 1 || pos.x >= size.x - 1 || pos.y >= size.y - 1) {
        imageStore(nmsMap, pos, vec4(0.0));
        return;
    }

    float s = imageLoad(scoreMap, pos).r;
    if(s <= 0.0) {
        imageStore(nmsMap, pos, vec4(0.0));
        return;
    }

    bool is_max = true;
    for(int dy = -1; dy <= 1; dy++) {
        for(int dx = -1; dx <= 1; dx++) {
            if(dx == 0 && dy == 0) continue;
            if(imageLoad(scoreMap, pos + ivec2(dx, dy)).r >= s) {
                is_max = false;
                break;
            }
        }
        if(!is_max) break;
    }

    imageStore(nmsMap, pos, vec4(is_max ? s : 0.0, 0.0, 0.0, 0.0));
}
)SH4D3R";

END_UPP_NAMESPACE
