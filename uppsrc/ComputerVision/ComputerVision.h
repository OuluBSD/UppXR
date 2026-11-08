#ifndef _ComputerVision_ComputerVision_h_
#define _ComputerVision_ComputerVision_h_

#include <Draw/Draw.h>
#include <Geometry/Geometry.h>

NAMESPACE_UPP

#include "Cascade.h"
#include "Backend.h"
#include "Struct.h"
#include "Math.h"
#include "ImageProcessing.h"
#include "ORB.h"
#include "BrightnessBinaryFeature.h"
#include "FAST.h"
#include "HAAR.h"
#include "Matrix.h"
#include "LinearAlgebra.h"
#include "MotionEstimator.h"
#include "OpticalFlowLK.h"
#include "Transform.h"
#include "YAPE.h"
#include "YAPE06.h"
#include "VideoStabilizer.h"
#include "GaussMotionFilter.h"
#include "KeypointMotionEstimator.h"
#include "OnepassStabilizer.h"
#include "OrbSystem.h"

extern const char* fast_score_map_shader;
extern const char* fast_nms_shader;

END_UPP_NAMESPACE

#endif
