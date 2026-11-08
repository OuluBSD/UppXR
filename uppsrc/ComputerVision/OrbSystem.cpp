#include "ComputerVision.h"

NAMESPACE_UPP


void OrbSystem::SetInput(Image i) {
	sz = i.GetSize();
	const RGBA* it = i.Begin();
	int len = sz.cx * sz.cy * 4;
	input.SetSize(sz.cx, sz.cy, 4);
	memcpy(input.data.Begin(), it, len);
}

void OrbSystem::TrainPattern() {
	ByteMat img_u8;
	if (input.channels > 1)
		Grayscale(input, img_u8);
	else
		img_u8 = input;

	if (img_u8.cols <= 0 || img_u8.rows <= 0)
		return;
	auto& lev0_img = tmp0;
	auto& lev_img = tmp1;
	auto& pattern_preview = tmp2;
    double sc = 1.0;
    int max_pattern_size = 2048;
    int max_per_level = 500;
    double sc_inc = pow(2.0, 1.0/4.0); // 1.189
    lev0_img.SetSize(img_u8.cols, img_u8.rows, 1);
    lev_img.SetSize(img_u8.cols, img_u8.rows, 1);
    int new_width=0, new_height=0;
    int corners_num=0;

    int sc0 = 1;
    if (img_u8.cols > max_pattern_size || img_u8.rows > max_pattern_size)
        sc0 = min(max_pattern_size/img_u8.cols, max_pattern_size/img_u8.rows);
    
    new_width = (img_u8.cols*sc0);
    new_height = (img_u8.rows*sc0);
    pattern_sz = Size(new_width, new_height);

    Resample(img_u8, lev0_img, new_width, new_height);

    // prepare preview
    pattern_preview.SetSize(new_width>>1, new_height>>1, 1);
    DownsamplePyramid(lev0_img, pattern_preview);
	
	pattern_corners.SetCount(num_train_levels);
	pattern_descriptors.SetCount(num_train_levels);
    for(int lev=0; lev < num_train_levels; ++lev) {
        Vector<Keypoint>& lev_corners = pattern_corners[lev];

        // preallocate corners array
        int i = (new_width*new_height); // better safe than sorry
        lev_corners.SetCount(i);
        while(--i >= 0) {
            lev_corners[i].Set(0,0,0,0,-1);
        }

        pattern_descriptors[lev].SetCount(max_per_level);
    }

    // do the first level
    {
	    Vector<Keypoint>& lev_corners = pattern_corners[0];
	    Vector<BinDescriptor>& lev_descr = pattern_descriptors[0];
	
	    GaussianBlur(lev0_img, lev_img, blur_size); // this is more robust
	    corners_num = DetectKeypoints(lev_img, lev_corners, max_per_level);
	    o.Describe(lev_img, lev_corners, lev_descr);
	
	    //LOG("train " << lev_img.cols << "x" << lev_img.rows << " points: " << corners_num);
	
	    sc /= sc_inc;
    }
    
    // lets do multiple scale levels
    // we can use Canvas context draw method for faster SetSize
    // but its nice to demonstrate that you can do everything with jsfeat
    for(int lev = 1; lev < num_train_levels; ++lev) {
        Vector<Keypoint>& lev_corners = pattern_corners[lev];
        Vector<BinDescriptor>& lev_descr = pattern_descriptors[lev];

        int new_width = (int)(lev0_img.cols*sc);
        int new_height = (int)(lev0_img.rows*sc);

        Resample(lev0_img, lev_img, new_width, new_height);
        GaussianBlur(lev_img, lev_img, blur_size);
        corners_num = DetectKeypoints(lev_img, lev_corners, max_per_level);
        o.Describe(lev_img, lev_corners, lev_descr);

        // fix the coordinates due to scale level
        for (Keypoint& corner : lev_corners) {
            corner.x = (int)(corner.x * 1./sc);
            corner.y = (int)(corner.y * 1./sc);
        }

        //LOG("train " << lev_img.cols << "x" << lev_img.rows << " points: " << corners_num);

        sc /= sc_inc;
    }
}

void OrbSystem::InitDefault() {

    keypoint_match_threshold = 95;
    num_train_levels = exact_scale_only ? 1 : 10;

    y.laplacian_threshold = 2;
    y.min_eigen_value_threshold = 2;

    /*img_u8 = new jsfeat.DMatrix(sz.cx, sz.cy, jsfeat.U8_t | jsfeat.C1_t);
    // after blur
    img_u8_smooth = new jsfeat.DMatrix(sz.cx, sz.cy, jsfeat.U8_t | jsfeat.C1_t);*/
    
    // we wll limit to 1000 strongest points
    screen_descriptors.Reserve(1000);
    pattern_descriptors.SetCount(0);

    pattern_corners.SetCount(0);
    matches.SetCount(0);

	
    int i = 1000;
    matches.SetCount(0);
    matches.Reserve(i);
    screen_corners.SetCount(i);
    for (Keypoint& k : screen_corners)
        k.Set(0,0,0,0,-1);

    // transform matrix
    homo3x3.SetSize(3,3,1);
    match_mask.SetSize(1000,1,1);
    
    Grayscale(input, tmp0);
    
    TrainPattern();
}

void OrbSystem::Process() {
	ProcessROI(Rect(0, 0, sz.cx, sz.cy));
}

void OrbSystem::ProcessROI(Rect roi) {
	auto& img_u8 = tmp0;
	auto& img_u8_smooth = tmp1;
	last_profile.Clear();
	TimeStop total_ts;
	
	{
		TimeStop ts;
		Grayscale(input, img_u8);
		last_profile.grayscale_us += ts.Elapsed();
	}
	{
		TimeStop ts;
		GaussianBlur(img_u8, img_u8_smooth, blur_size);
		last_profile.blur_us += ts.Elapsed();
	}

    y.laplacian_threshold = lap_thres;
    y.min_eigen_value_threshold = eigen_thres;

	int num_corners = 0;
	{
		TimeStop ts;
		num_corners = DetectKeypoints(img_u8_smooth, roi, screen_corners, 1000);
		last_profile.detect_us += ts.Elapsed();
	}
	last_profile.num_corners = num_corners;
	{
		TimeStop ts;
		o.Describe(img_u8_smooth, screen_corners, screen_descriptors);
		last_profile.describe_us += ts.Elapsed();
	}

	if (render_debug) {
		TimeStop ts;
		RenderCorners(img_u8, &train_img, screen_corners, output);
		last_profile.render_corners_us += ts.Elapsed();
	}

    // render pattern and matches
	int num_matches = 0;
	{
		TimeStop ts;
		num_matches = MatchPattern();
		last_profile.match_us += ts.Elapsed();
	}
    ASSERT(matches.GetCount() == num_matches);
	int good_matches = 0;
	{
		TimeStop ts;
		good_matches = FindTransform(matches);
		last_profile.transform_us += ts.Elapsed();
	}
	last_match_count = num_matches;
	last_good_matches = good_matches;
	last_profile.num_matches = num_matches;
	last_profile.good_matches = good_matches;
    
    if(num_matches) {
		if (render_debug) {
			TimeStop ts;
			render_matches(matches);
			last_profile.render_matches_us += ts.Elapsed();
		}
        if(good_matches >= 4) {
			if (render_debug) {
				TimeStop ts;
	            render_pattern_shape();
				last_profile.render_shape_us += ts.Elapsed();
			}
			else {
				TCorners(homo3x3.data, pattern_sz.cx, pattern_sz.cy);
				last_corners.SetCount(corners.GetCount());
				for (int i = 0; i < corners.GetCount(); i++)
					last_corners[i] = Pointf(corners[i].x, corners[i].y);
			}
		}
		else
			last_corners.Clear();
    }
	else {
		last_corners.Clear();
	}
	last_profile.total_us = total_ts.Elapsed();
}

void OrbSystem::ProcessPrepared(const ByteMat& gray, const ByteMat& smooth, Rect roi) {
	sz = Size(gray.cols, gray.rows);
	tmp0 = gray;
	tmp1 = smooth;
	auto& img_u8 = tmp0;
	auto& img_u8_smooth = tmp1;
	last_profile.Clear();
	TimeStop total_ts;
	
	y.laplacian_threshold = lap_thres;
	y.min_eigen_value_threshold = eigen_thres;

	int num_corners = 0;
	{
		TimeStop ts;
		num_corners = DetectKeypoints(img_u8_smooth, roi, screen_corners, 1000);
		last_profile.detect_us += ts.Elapsed();
	}
	last_profile.num_corners = num_corners;
	{
		TimeStop ts;
		o.Describe(img_u8_smooth, screen_corners, screen_descriptors);
		last_profile.describe_us += ts.Elapsed();
	}

	if (render_debug) {
		TimeStop ts;
		RenderCorners(img_u8, &train_img, screen_corners, output);
		last_profile.render_corners_us += ts.Elapsed();
	}

	int num_matches = 0;
	{
		TimeStop ts;
		num_matches = MatchPattern();
		last_profile.match_us += ts.Elapsed();
	}
	ASSERT(matches.GetCount() == num_matches);
	int good_matches = 0;
	{
		TimeStop ts;
		good_matches = FindTransform(matches);
		last_profile.transform_us += ts.Elapsed();
	}
	last_match_count = num_matches;
	last_good_matches = good_matches;
	last_profile.num_matches = num_matches;
	last_profile.good_matches = good_matches;

	if(num_matches) {
		if (render_debug) {
			TimeStop ts;
			render_matches(matches);
			last_profile.render_matches_us += ts.Elapsed();
		}
		if(good_matches >= 4) {
			if (render_debug) {
				TimeStop ts;
				render_pattern_shape();
				last_profile.render_shape_us += ts.Elapsed();
			}
			else {
				TCorners(homo3x3.data, pattern_sz.cx, pattern_sz.cy);
				last_corners.SetCount(corners.GetCount());
				for (int i = 0; i < corners.GetCount(); i++)
					last_corners[i] = Pointf(corners[i].x, corners[i].y);
			}
		}
		else
			last_corners.Clear();
	}
	else {
		last_corners.Clear();
	}
	last_profile.total_us = total_ts.Elapsed();
}

void OrbSystem::OutputFromGray(const ByteMat& gray) {
	ASSERT(gray.channels == 1 && gray.cols == sz.cx && gray.rows == sz.cy);
	output.SetSize(sz.cx, sz.cy, 4);
    ASSERT(!gray.IsEmpty());
	if (gray.IsEmpty())
		return;
    ASSERT(output.data.GetCount() == gray.data.GetCount() * 4);
    byte* it = output.data.Begin();
    for (byte g : gray.data) {
        it[0] = g;
        it[1] = g;
        it[2] = g;
        it[3] = 255;
        it+=4;
    }
}

void OrbSystem::RenderCorners(const ByteMat& bg, const ByteMat* mini_img, const Vector<Keypoint>& corners, ByteMat& out) {
	ASSERT(bg.cols == sz.cx && bg.rows == sz.cy);
	int count = corners.GetCount();
	int step = sz.cx;
	
	OutputFromGray(bg);
	
	if (mini_img && mini_img->cols > 0 && mini_img->cols <= bg.cols) {
		RGBA* img = (RGBA*)output.data.Begin();
		const byte* bg_img = (byte*)mini_img->data.Begin();
		for (int y = 0; y < mini_img->rows; y++) {
			for (int x = 0; x < mini_img->cols; x++) {
				img->r = img->g = img->b = *bg_img++;
				img->a = 255;
				img++;
			}
			img += output.cols - mini_img->cols;
		}
	}
	
	uint32* img = (uint32*)(byte*)output.data.Begin();
    for(int i=0; i < count; ++i)
    {
        uint32 pix = 0xff << 24 | 0xff << 16;
        const auto& c = corners[i];
        int off = (c.x + c.y * step);
        img[off] = pix;
        img[off-1] = pix;
        img[off+1] = pix;
        img[off-step] = pix;
        img[off+step] = pix;
    }
}

int OrbSystem::DetectKeypoints(DescriptorImage& output, int max_allowed) {
	return DetectKeypointsROI(Rect(0, 0, sz.cx, sz.cy), output, max_allowed);
}

int OrbSystem::DetectKeypointsROI(Rect roi, DescriptorImage& output, int max_allowed) {
	auto& img_u8_smooth = tmp1;
	
    y.laplacian_threshold = lap_thres;
    y.min_eigen_value_threshold = eigen_thres;
	
    int num_corners = DetectKeypoints(img_u8_smooth, roi, screen_corners, max_allowed);
    o.Describe(img_u8_smooth, screen_corners, screen_descriptors);
	
	output.ClearDescriptors();
	const Keypoint* kp = screen_corners.Begin();
	for (const BinDescriptor& bd : screen_descriptors) {
		output.AddDescriptor(
			kp->x,
			kp->y,
			kp->angle,
			(void*)&bd.u8[0]
		);
		kp++;
	}
	
	return num_corners;
}

int OrbSystem::DetectKeypoints(const ByteMat& img, Vector<Keypoint>& corners, int max_allowed) {
	return DetectKeypoints(img, Rect(0, 0, img.cols, img.rows), corners, max_allowed);
}

int OrbSystem::DetectKeypoints(const ByteMat& img, const Rect& roi, Vector<Keypoint>& corners, int max_allowed) {
    // detect features
    int count = y.Detect(img, roi, corners, 5);

    // sort by score and reduce the count if needed
    if(count > max_allowed) {
        Sort(corners, Keypoint());
        count = max_allowed;
        corners.SetCount(count);
    }

    // calculate dominant orientation for each keypoint
    if (use_orientation) {
	    for (Keypoint& c : corners)
	        c.angle = IcAngle(img, c.x, c.y);
    }
    else {
	    for (Keypoint& c : corners)
	        c.angle = 0.0;
    }

    return count;
}

// central difference using image moments to find dominant orientation
const int OrbSystem::u_max[] = {15,15,15,15,14,14,14,13,13,12,11,10,9,8,6,3,0};

double OrbSystem::IcAngle(const ByteMat& img, int px, int py) {
    int half_k = 15; // half patch size
    int m_01 = 0, m_10 = 0;
    auto& src=img.data;
    int step=img.cols;
    int w=img.cols, h=img.rows;
    int center_off=(py*step + px);
    int v_sum=0,d=0,val_plus=0,val_minus=0;

    // Treat the center line differently, v=0
    for (int u = -half_k; u <= half_k; ++u) {
        if (px + u >= 0 && px + u < w && py >= 0 && py < h)
            m_10 += u * src[center_off+u];
    }

    // Go line by line in the circular patch
    for (int v = 1; v <= half_k; ++v) {
        // Proceed over the two lines
        v_sum = 0;
        d = u_max[v];
        for (int u = -d; u <= d; ++u) {
            if (px + u >= 0 && px + u < w) {
                if (py + v < h) {
                    val_plus = src[center_off+u+v*step];
                    m_10 += u * val_plus;
                    v_sum += val_plus;
                }
                if (py - v >= 0) {
                    val_minus = src[center_off+u-v*step];
                    m_10 += u * val_minus;
                    v_sum -= val_minus;
                }
            }
        }
        m_01 += v * v_sum;
    }

    return atan2(m_01, m_10);
}

// estimate homography transform between matched points
int OrbSystem::FindTransform(Vector<KeypointMatch>& matches) {
	if (!use_ransac) {
		int count = matches.GetCount();
		if (count < 4) {
			Identity3x3(homo3x3, 1.0f);
			return 0;
		}
		Vector<float> dx, dy;
		dx.SetCount(count);
		dy.SetCount(count);
		for (int i = 0; i < count; i++) {
			const KeypointMatch& m = matches[i];
			const Keypoint& s_kp = screen_corners[m.screen_idx];
			const Keypoint& p_kp = pattern_corners[m.pattern_lev][m.pattern_idx];
			dx[i] = (float)(s_kp.x - p_kp.x);
			dy[i] = (float)(s_kp.y - p_kp.y);
		}
		Sort(dx);
		Sort(dy);
		float tx = dx[count / 2];
		float ty = dy[count / 2];
		Identity3x3(homo3x3, 1.0f);
		homo3x3.data[2] = tx;
		homo3x3.data[5] = ty;
		match_mask.SetSize(count, 1, 1);
		int good_cnt = 0;
		for (int i = 0; i < count; i++) {
			const KeypointMatch& m = matches[i];
			const Keypoint& s_kp = screen_corners[m.screen_idx];
			const Keypoint& p_kp = pattern_corners[m.pattern_lev][m.pattern_idx];
			float ex = fabs((float)s_kp.x - ((float)p_kp.x + tx));
			float ey = fabs((float)s_kp.y - ((float)p_kp.y + ty));
			byte ok = (ex <= 4.0f && ey <= 4.0f) ? 1 : 0;
			match_mask.data[i] = ok;
			good_cnt += ok;
		}
		if (good_cnt < 4)
			Identity3x3(homo3x3, 1.0f);
		return good_cnt;
	}
    
    // ransac params
    int num_model_points = 4;
    int reproj_threshold = 3;
    
    ransac_param.Init(num_model_points, reproj_threshold, 0.5, 0.99);

	int count = matches.GetCount();
    pattern_xy.SetCount(count);
    screen_xy.SetCount(count);
    match_mask.SetSize(count, 1, 1);

    // construct correspondences
    auto pattern_it = pattern_xy.Begin();
    auto screen_it = screen_xy.Begin();
    for (const KeypointMatch& m : matches) {
        auto& pat = *pattern_it; pattern_it++;
        auto& scr = *screen_it; screen_it++;
        const Keypoint& s_kp = screen_corners[m.screen_idx];
        const Keypoint& p_kp = pattern_corners[m.pattern_lev][m.pattern_idx];
        pat.x = p_kp.x;
        pat.y = p_kp.y;
        scr.x = s_kp.x;
        scr.y = s_kp.y;
    }

    // estimate motion
    bool ok = false;
    if (count >= num_model_points)
        ok = mot.Ransac(ransac_param, mm_kernel, pattern_xy, screen_xy, homo3x3, &match_mask, 1000);

    int good_cnt = 0;
    if (ok) {
        for(int i = 0; i < count; ++i) {
            if (match_mask.data[i]) {
                good_cnt++;
            }
        }
    }
    else {
        Identity3x3(homo3x3, 1.0f);
    }

    // extract good matches and re-estimate
    if (ok) {
        int inlier_idx = 0;
        for(int i = 0; i < count; ++i) {
            if (match_mask.data[i]) {
                pattern_xy[inlier_idx] = pattern_xy[i];
                screen_xy[inlier_idx] = screen_xy[i];
                inlier_idx++;
            }
        }
        ASSERT(inlier_idx == good_cnt);
        pattern_xy.SetCount(good_cnt);
        screen_xy.SetCount(good_cnt);
        
        // run kernel directly with inliers only
        mm_kernel.Run(pattern_xy, screen_xy, homo3x3);
    }

    return good_cnt;
}


// naive brute-force matching.
// each on screen point is compared to all pattern points
// to find the closest match
int OrbSystem::MatchPattern() {
    int q_cnt = screen_descriptors.GetCount();
    int qidx=0,lev=0,pidx=0,k=0;
    int num_matches = 0;
    matches.SetCount(0);
    matches.Reserve(256);

    int min_dist = 256;

    for(qidx = 0; qidx < q_cnt; ++qidx) {
        const BinDescriptor& scr = screen_descriptors[qidx];
        int best_dist = 256;
        int best_dist2 = 256;
        int best_idx = -1;
        int best_lev = -1;

        for(lev = 0; lev < num_train_levels; ++lev) {
            auto& pattern = pattern_descriptors[lev];
            int ld_cnt = pattern.GetCount();

            for(pidx = 0; pidx < ld_cnt; ++pidx) {
                const BinDescriptor& lev_desc = pattern[pidx];
                int curr_d = 0;
                
                // our descriptor is 32 bytes so we have 8 Integers
                for(k=0; k < 8; ++k) {
                    curr_d += PopCount32( scr.u32[k] ^ lev_desc.u32[k] );
                }

                if(curr_d < best_dist) {
                    best_dist2 = best_dist;
                    best_dist = curr_d;
                    best_lev = lev;
                    best_idx = pidx;
                } else if(curr_d < best_dist2) {
                    best_dist2 = curr_d;
                }
            }
        }
        
        if (best_dist < min_dist) min_dist = best_dist;

        // filter out by some threshold AND ratio test
        if(best_dist < keypoint_match_threshold && (double)best_dist < ratio_test * (double)best_dist2) {
            auto& m = matches.Add();
            m.screen_idx = qidx;
            m.pattern_lev = best_lev;
            m.pattern_idx = best_idx;
            num_matches++;
        }
    }

#ifdef flagORB_DIAG
	Cout() << "ORB MatchPattern: matches=" << num_matches << " min_dist=" << min_dist 
		<< " threshold=" << keypoint_match_threshold << " ratio=" << ratio_test << "\\n";
#endif

    return num_matches;
}

// project/transform rectangle corners with 3x3 Matrix
void OrbSystem::TCorners(const Vector<float>& M, int w, int h) {
	corners.SetCount(0);
	corners.Reserve(4);
    corners << Keypoint(0,0) << Keypoint(w,0) << Keypoint(w,h) << Keypoint(0,h);
    double z=0.0;
    double px=0.0, py=0.0;

    for (Keypoint& p : corners) {
        px = M[0]*p.x + M[1]*p.y + M[2];
        py = M[3]*p.x + M[4]*p.y + M[5];
        z = M[6]*p.x + M[7]*p.y + M[8];
        p.x = px/z;
        p.y = py/z;
    }
}

void OrbSystem::render_matches(const Vector<KeypointMatch>& matches) {
	ASSERT(match_mask.data.GetCount() >= matches.GetCount());
	byte* mask = match_mask.data.Begin();
	lines.SetCount(0);
    for(const KeypointMatch& m : matches) {
        const auto& s_kp = screen_corners[m.screen_idx];
        const auto& p_kp = pattern_corners[m.pattern_lev][m.pattern_idx];
        
        byte b = 0, r,g;
        if (*mask++) {
            r = 0;
            g = 255;
        } else {
            r = 255;
            g = 0;
        }
        
        ColorLine& l = lines.Add();
        l.a.x = s_kp.x;
        l.a.y = s_kp.y;
        l.b.x = p_kp.x*0.25;
        l.b.y = p_kp.y*0.25;
        l.clr = Color(r,g,b);
    }
}

void OrbSystem::render_pattern_shape() {
    // get the projected pattern corners
    TCorners(homo3x3.data, pattern_sz.cx, pattern_sz.cy);
	
	last_corners.SetCount(corners.GetCount());
	for (int i = 0; i < corners.GetCount(); i++) {
		last_corners[i] = Pointf(corners[i].x, corners[i].y);
	}
	
	for(int i = 0; i < corners.GetCount(); i++) {
		const Keypoint& a = corners[i];
		const Keypoint& b = corners[(i + 1) % corners.GetCount()];
		
		ColorLine& l = lines.Add();
        l.a.x = a.x;
        l.a.y = a.y;
        l.b.x = b.x;
        l.b.y = b.y;
        l.clr = Color(0,255,0);
	}
}


 

void OrbSystem::ProcessGpu(const Vector<OrbSystem::GpuKp>& keypoints, const Vector<BinDescriptor>& descriptors, Rect roi) {
    if (homo3x3.IsEmpty()) homo3x3.SetSize(3, 3, 1);
    sz = roi.GetSize();
    last_profile.Clear();
    TimeStop total_ts;

    int num_corners = 0;
    {
        TimeStop ts;
        screen_corners.Clear();
        screen_descriptors.Clear();
        for(int i = 0; i < keypoints.GetCount(); i++) {
            const GpuKp& gk = keypoints[i];
            if (roi.Contains(Point((int)gk.x, (int)gk.y))) {
                Keypoint& k = screen_corners.Add();
                k.x = (int)gk.x;
                k.y = (int)gk.y;
                k.score = (int)gk.score;
                k.level = gk.level;
                k.angle = 0.0;
                if (i < descriptors.GetCount())
                    screen_descriptors.Add(descriptors[i]);
            }
        }
        num_corners = screen_corners.GetCount();
        last_profile.detect_us += ts.Elapsed();
    }
    last_profile.num_corners = num_corners;
    
    last_profile.describe_us = 0;

    int num_matches = MatchPattern();
    int good_matches = FindTransform(matches);
    
    last_match_count = num_matches;
    last_good_matches = good_matches;
    last_profile.num_matches = num_matches;
    last_profile.good_matches = good_matches;

    if(good_matches >= 4) {
        TCorners(homo3x3.data, pattern_sz.cx, pattern_sz.cy);
        last_corners.SetCount(corners.GetCount());
        for (int i = 0; i < corners.GetCount(); i++)
            last_corners[i] = Pointf(corners[i].x, corners[i].y);
    } else last_corners.Clear();
    last_profile.total_us = total_ts.Elapsed();
}

void OrbSystem::ProcessGpu(const ByteMat& gray, const Vector<OrbSystem::GpuKp>& keypoints, int level, Rect roi) {
    sz = Size(gray.cols, gray.rows);
    tmp0 = gray;
    auto& img_u8 = tmp0;
    last_profile.Clear();
    TimeStop total_ts;

    int num_corners = 0;
    {
        TimeStop ts;
        screen_corners.Clear();
        for(const GpuKp& gk : keypoints) {
            if (gk.level == level && roi.Contains(Point((int)gk.x, (int)gk.y))) {
                Keypoint& k = screen_corners.Add();
                k.x = (int)gk.x;
                k.y = (int)gk.y;
                k.score = (int)gk.score;
                k.level = gk.level;
                k.angle = (use_orientation) ? IcAngle(img_u8, k.x, k.y) : 0.0;
            }
        }
        num_corners = screen_corners.GetCount();
        last_profile.detect_us += ts.Elapsed();
    }
    last_profile.num_corners = num_corners;
    {
        TimeStop ts;
        o.Describe(img_u8, screen_corners, screen_descriptors);
        last_profile.describe_us += ts.Elapsed();
    }

    int num_matches = MatchPattern();
    int good_matches = FindTransform(matches);
    
    last_match_count = num_matches;
    last_good_matches = good_matches;
    last_profile.num_matches = num_matches;
    last_profile.good_matches = good_matches;

    if(good_matches >= 4) {
        TCorners(homo3x3.data, pattern_sz.cx, pattern_sz.cy);
        last_corners.SetCount(corners.GetCount());
        for (int i = 0; i < corners.GetCount(); i++)
            last_corners[i] = Pointf(corners[i].x, corners[i].y);
    } else last_corners.Clear();
    last_profile.total_us = total_ts.Elapsed();
}

END_UPP_NAMESPACE
