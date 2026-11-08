#include "ComputerVision.h"


NAMESPACE_UPP

void ImageToGrayByteMat(const Image& img, ByteMat& out) {
	Size sz = img.GetSize();
	out.SetSize(sz.cx, sz.cy, 1);
	for(int y = 0; y < sz.cy; y++) {
		const RGBA* s = img[y];
		for(int x = 0; x < sz.cx; x++)
			out.data[y * sz.cx + x] = (byte)(((int)s[x].r + (int)s[x].g + (int)s[x].b) / 3);
	}
}

Image FloatMatToGrayImage(const FloatMat& m, bool invert) {
	if(m.IsEmpty() || m.channels != 1)
		return Image();
	double min_v = 0, max_v = 0;
	MinMaxLoc(m, &min_v, &max_v, nullptr, nullptr);
	double span = max_v - min_v;
	if(span < 1e-20) span = 1.0;
	ImageBuffer ib(m.cols, m.rows);
	for(int y = 0; y < m.rows; y++) {
		RGBA* d = ib[y];
		for(int x = 0; x < m.cols; x++) {
			double v = m.data[y * m.cols + x];
			double t = (v - min_v) / span;
			if(invert) t = 1.0 - t;
			int g = (int)clamp(t * 255.0, 0.0, 255.0);
			d[x] = {(byte)g, (byte)g, (byte)g, 255};
		}
	}
	return ib;
}

// Shared scoring formula, factored out so the fast (single-channel, integral
// image) path below and the direct-computation fallback compute the exact
// same value from the exact same four sums -- only how sum_i/sum_ii get
// gathered differs between the two paths, never the arithmetic that turns
// them into a score.
static double MatchTemplateScore(TemplateMatchMethod method, double sum_i, double sum_ii,
                                  double sum_it, double sum_t, double sum_tt,
                                  double var_t, double inv_n, double eps)
{
	switch (method) {
	case TM_SQDIFF:
		return sum_ii - 2.0 * sum_it + sum_tt;
	case TM_SQDIFF_NORMED: {
		const double denom = sqrt(max(sum_ii * sum_tt, 0.0));
		return denom > eps ? (sum_ii - 2.0 * sum_it + sum_tt) / denom : 0.0;
	}
	case TM_CCORR:
		return sum_it;
	case TM_CCORR_NORMED: {
		const double denom = sqrt(max(sum_ii * sum_tt, 0.0));
		return denom > eps ? sum_it / denom : 0.0;
	}
	case TM_CCOEFF:
		return sum_it - (sum_i * sum_t) * inv_n;
	case TM_CCOEFF_NORMED: {
		const double num = sum_it - (sum_i * sum_t) * inv_n;
		const double var_i = max(sum_ii - (sum_i * sum_i) * inv_n, 0.0);
		const double denom = sqrt(var_i * var_t);
		return denom > eps ? num / denom : 0.0;
	}
	default:
		return sum_it;
	}
}

void MatchTemplate(const ByteMat& image, const ByteMat& templ, FloatMat& result, TemplateMatchMethod method) {
	ASSERT(!image.IsEmpty());
	ASSERT(!templ.IsEmpty());
	ASSERT(image.channels == templ.channels);
	ASSERT(image.cols >= templ.cols);
	ASSERT(image.rows >= templ.rows);

	if (image.IsEmpty() || templ.IsEmpty())
		return;
	if (image.channels != templ.channels || image.cols < templ.cols || image.rows < templ.rows) {
		result = FloatMat();
		return;
	}

	const int iw = image.cols;
	const int ih = image.rows;
	const int tw = templ.cols;
	const int th = templ.rows;
	const int ch = image.channels;
	const int rw = iw - tw + 1;
	const int rh = ih - th + 1;
	const int n = tw * th * ch;
	const double inv_n = 1.0 / (double)n;
	const double eps = 1e-12;

	const auto& id = image.data;
	const auto& td = templ.data;
	result.SetSize(rw, rh, 1);
	auto& rd = result.data;

	double sum_t = 0.0;
	double sum_tt = 0.0;
	for (int i = 0; i < td.GetCount(); i++) {
		const double t = (double)td[i];
		sum_t += t;
		sum_tt += t * t;
	}

	const double var_t = max(sum_tt - (sum_t * sum_t) * inv_n, 0.0);

	// Task 0317: sum_i/sum_ii (the per-window image statistics) do not depend
	// on the template's pixel values at all -- only on the window's position
	// and size. The direct computation below re-derives both by walking every
	// one of the tw*th window pixels for every single output position, on top
	// of the unavoidable cross-correlation term (sum_it), which needs that
	// same walk. That is 3x the necessary work. A summed-area table (integral
	// image over the image once, O(iw*ih)) turns sum_i/sum_ii into four O(1)
	// lookups per window, leaving only sum_it's walk -- roughly a 3x cut to
	// the dominant cost profiled in task 0317 (multi-scale chip-template
	// correlation: 18 templates x 15 scales, up to 32 crops/frame). Restricted
	// to the single-channel case, which is what that hot path (and every
	// other real caller measured) actually uses; multi-channel images keep
	// the original direct computation below rather than adding per-channel
	// integral-image bookkeeping for a path nothing currently exercises at
	// volume.
	if (ch == 1) {
		const int sw = iw + 1;
		Vector<double> sat, sat2;
		sat.SetCount((iw + 1) * (ih + 1), 0.0);
		sat2.SetCount((iw + 1) * (ih + 1), 0.0);
		for (int y = 0; y < ih; y++) {
			double row_sum = 0.0, row_sum2 = 0.0;
			const int src_row = y * iw;
			const int dst_row = (y + 1) * sw;
			const int prev_row = y * sw;
			for (int x = 0; x < iw; x++) {
				const double v = (double)id[src_row + x];
				row_sum += v;
				row_sum2 += v * v;
				sat[dst_row + x + 1]  = sat[prev_row + x + 1]  + row_sum;
				sat2[dst_row + x + 1] = sat2[prev_row + x + 1] + row_sum2;
			}
		}

		for (int y = 0; y < rh; y++) {
			for (int x = 0; x < rw; x++) {
				double sum_it = 0.0;
				for (int ty = 0; ty < th; ty++) {
					const int image_row = (y + ty) * iw + x;
					const int templ_row = ty * tw;
					for (int tx = 0; tx < tw; tx++)
						sum_it += (double)id[image_row + tx] * (double)td[templ_row + tx];
				}

				const int x0 = x, y0 = y, x1 = x + tw, y1 = y + th;
				const double sum_i  = sat [y1 * sw + x1] - sat [y0 * sw + x1] - sat [y1 * sw + x0] + sat [y0 * sw + x0];
				const double sum_ii = sat2[y1 * sw + x1] - sat2[y0 * sw + x1] - sat2[y1 * sw + x0] + sat2[y0 * sw + x0];

				rd[y * rw + x] = (float)MatchTemplateScore(method, sum_i, sum_ii, sum_it,
				                                           sum_t, sum_tt, var_t, inv_n, eps);
			}
		}
		return;
	}

	for (int y = 0; y < rh; y++) {
		for (int x = 0; x < rw; x++) {
			double sum_i = 0.0;
			double sum_ii = 0.0;
			double sum_it = 0.0;

			for (int ty = 0; ty < th; ty++) {
				const int image_row = ((y + ty) * iw + x) * ch;
				const int templ_row = (ty * tw) * ch;
				for (int txc = 0; txc < tw * ch; txc++) {
					const double i = (double)id[image_row + txc];
					const double t = (double)td[templ_row + txc];
					sum_i += i;
					sum_ii += i * i;
					sum_it += i * t;
				}
			}

			rd[y * rw + x] = (float)MatchTemplateScore(method, sum_i, sum_ii, sum_it,
			                                           sum_t, sum_tt, var_t, inv_n, eps);
		}
	}
}

void MinMaxLoc(const FloatMat& src, double* min_val, double* max_val, Point* min_loc, Point* max_loc) {
	ASSERT(src.channels == 1);
	ASSERT(!src.IsEmpty());
	
	if (src.IsEmpty() || src.channels != 1)
		return;
	
	const auto& d = src.data;
	const int count = d.GetCount();
	int min_i = 0;
	int max_i = 0;
	float min_v = d[0];
	float max_v = d[0];
	for (int i = 1; i < count; i++) {
		const float v = d[i];
		if (v < min_v) {
			min_v = v;
			min_i = i;
		}
		if (v > max_v) {
			max_v = v;
			max_i = i;
		}
	}
	
	if (min_val) *min_val = min_v;
	if (max_val) *max_val = max_v;
	if (min_loc) *min_loc = Point(min_i % src.cols, min_i / src.cols);
	if (max_loc) *max_loc = Point(max_i % src.cols, max_i / src.cols);
}


void ResampleByte(const pyra8::Mat& src, pyra8::Mat& dst, int nw, int nh) {
	int xofs_count = 0;
	int ch = src.channels;
	int w = src.cols;
	int h = src.rows;
	const auto& src_d = src.data;
	auto& dst_d = dst.data;
	double scale_x = (double)w / nw;
	double scale_y = (double)h / nh;
	double inv_scale_256 = (scale_x * scale_y * 0x10000);
	
	static thread_local Vector<int> buf;
	static thread_local Vector<int> sum;
	static thread_local Vector<int> xofs;
	
	buf.SetCount((nw * ch) << 2);
	sum.SetCount((nw * ch) << 2);
	xofs.SetCount((w * 2 * 3) << 2);
	
	int k = 0;
	for (int dx = 0; dx < nw; dx++) {
		double fsx1 = dx * scale_x;
		double fsx2 = fsx1 + scale_x;
		double sx1 = (fsx1 + 1.0 - 1e-6);
		double sx2 = fsx2;
		sx1 = min(sx1, w - 1.0);
		sx2 = min(sx2, w - 1.0);
		
		if (sx1 > fsx1) {
			xofs[k++] = (int)(dx * ch);
			xofs[k++] = (int)((sx1 - 1) * ch);
			xofs[k++] = (int)((sx1 - fsx1) * 0x100);
			xofs_count++;
		}
		for (double sx = sx1; sx < sx2; sx += 1.0) {
			xofs_count++;
			xofs[k++] = (int)(dx * ch);
			xofs[k++] = (int)(sx * ch);
			xofs[k++] = 256;
		}
		if (fsx2 - sx2 > 1e-3) {
			xofs_count++;
			xofs[k++] = (int)(dx * ch);
			xofs[k++] = (int)(sx2 * ch);
			xofs[k++] = (int)((fsx2 - sx2) * 256);
		}
	}
	
	for (int dx = 0; dx < nw * ch; dx++) {
		buf[dx] = 0;
		sum[dx] = 0;
	}
	
	for (int sy = 0, dy = 0; sy < h; sy++) {
		int a = w * sy;
		for (int k = 0; k < xofs_count; k++) {
			int dxn = xofs[k*3];
			int sx1 = xofs[k*3+1];
			double alpha = xofs[k*3+2];
			for (int i = 0; i < ch; i++) {
				buf[dxn + i] += (int)(src_d[a+sx1+i] * alpha);
			}
		}
		if ((dy + 1) * scale_y <= sy + 1 || sy == h - 1) {
			double beta = (max(sy + 1 - (dy + 1) * scale_y, 0.0) * 256);
			double beta1 = 256 - beta;
			int b = nw * dy;
			int c = nw * ch;
			if (beta <= 0) {
				for (int dx = 0; dx < c; dx++) {
					dst_d[b+dx] = (int)min(max((sum[dx] + buf[dx] * 256) / inv_scale_256, 0.0), 255.0);
					sum[dx] = buf[dx] = 0;
				}
			}
			else {
				for (int dx = 0; dx < c; dx++) {
					dst_d[b+dx] = (int)min(max((sum[dx] + buf[dx] * beta1) / inv_scale_256, 0.0), 255.0);
					sum[dx] = (int)(buf[dx] * beta);
					buf[dx] = 0;
				}
			}
			dy++;
		}
		else {
			for (int dx = 0; dx < nw * ch; dx++) {
				sum[dx] += buf[dx] * 256;
				buf[dx] = 0;
			}
		}
	}
	
}

void ResampleFloat(const pyraf::Mat& src, pyraf::Mat& dst, int nw, int nh) {
	int xofs_count = 0;
	int ch = src.channels;
	int w = src.cols;
	int h = src.rows;
	const auto& src_d = src.data;
	auto& dst_d = dst.data;
	double scale_x = (double)w / nw;
	double scale_y = (double)h / nh;
	double scale = 1.0 / (scale_x * scale_y);
	
	Vector<float> buf;
	Vector<float> sum;
	Vector<float> xofs;
	
	buf.SetCount((nw * ch) << 2);
	sum.SetCount((nw * ch) << 2);
	xofs.SetCount((w * 2 * 3) << 2);
	
	int k = 0;
	for (int dx = 0; dx < nw; dx++) {
		double fsx1 = dx * scale_x;
		double fsx2 = fsx1 + scale_x;
		double sx1 = (fsx1 + 1.0 - 1e-6);
		double sx2 = fsx2;
		sx1 = min(sx1, w - 1.0);
		sx2 = min(sx2, w - 1.0);
		
		if (sx1 > fsx1) {
			xofs_count++;
			xofs[k++] = (float)((sx1 - 1) * ch);
			xofs[k++] = (float)(dx * ch);
			xofs[k++] = (float)((sx1 - fsx1) * scale);
		}
		for (double sx = sx1; sx < sx2; sx += 1.0) {
			xofs_count++;
			xofs[k++] = (float)(sx * ch);
			xofs[k++] = (float)(dx * ch);
			xofs[k++] = (float)scale;
		}
		if (fsx2 - sx2 > 1e-3) {
			xofs_count++;
			xofs[k++] = (float)(sx2 * ch);
			xofs[k++] = (float)(dx * ch);
			xofs[k++] = (float)((fsx2 - sx2) * scale);
		}
	}
	
	for (int dx = 0; dx < nw * ch; dx++) {
		buf[dx] = 0;
		sum[dx] = 0;
	}
	
	for (int sy = 0, dy = 0; sy < h; sy++) {
		int a = w * sy;
		for (int k = 0; k < xofs_count; k++) {
			float sx1 = xofs[k*3];
			float dxn = xofs[k*3+1];
			double alpha = xofs[k*3+2];
			for (int i = 0; i < ch; i++) {
				int j = (int)(a+sx1+i);
				int k = (int)(dxn + i);
				buf[k] += (float)(src_d[j] * alpha);
			}
		}
		if ((dy + 1) * scale_y <= sy + 1 || sy == h - 1) {
			double beta = max(sy + 1 - (dy + 1) * scale_y, 0.0);
			double beta1 = 1.0 - beta;
			int b = nw * dy;
			if (abs(beta) < 1e-3) {
				for (int dx = 0; dx < nw * ch; dx++) {
					dst_d[b+dx] = (byte)(sum[dx] + buf[dx]);
					sum[dx] = buf[dx] = 0;
				}
			}
			else {
				for (int dx = 0; dx < nw * ch; dx++) {
					dst_d[b+dx] = (byte)(sum[dx] + buf[dx] * beta1);
					sum[dx] = (float)(buf[dx] * beta);
					buf[dx] = 0;
				}
			}
			dy++;
		}
		else {
			for (int dx = 0; dx < nw * ch; dx++) {
				sum[dx] += buf[dx];
				buf[dx] = 0;
			}
		}
	}
	
}

void ConvoluteByte(Vector<int>& buf, const Vector<byte>& src_d, Vector<byte>& dst_d, int w, int h, Vector<double>& filter, int kernel_size, int half_kernel) {
	int  w2 = w << 1, w3 = w * 3, w4 = w << 2;
	
	double f0 = filter[0];
	
	// hor pass
	for (int i = 0, dp = 0, sp = 0; i < h; ++i) {
		int sum = src_d[sp];
		for (int j = 0; j < half_kernel; ++j) {
			buf[j] = sum;
		}
		{
			int j;
			for (j = 0; j <= w - 2; j += 2) {
				buf[j + half_kernel] = src_d[sp+j];
				buf[j + half_kernel+1] = src_d[sp+j+1];
			}
			for (; j < w; ++j) {
				buf[j + half_kernel] = src_d[sp+j];
			}
		}
		
		sum = src_d[sp+w-1];
		for (int j = w; j < half_kernel + w; ++j) {
			buf[j + half_kernel] = sum;
		}
		
		{
			int j;
			for (j = 0; j <= w - 4; j += 4) {
				int sum  = buf[j]   * f0;
				int sum1 = buf[j+1] * f0;
				int sum2 = buf[j+2] * f0;
				int sum3 = buf[j+3] * f0;
				
				for (int k = 1; k < kernel_size; ++k) {
					double fk = filter[k];
					sum  += buf[k + j] * fk;
					sum1 += buf[k + j+1] * fk;
					sum2 += buf[k + j+2] * fk;
					sum3 += buf[k + j+3] * fk;
				}
				dst_d[dp+j]   = min(sum,  255);
				dst_d[dp+j+1] = min(sum1, 255);
				dst_d[dp+j+2] = min(sum2, 255);
				dst_d[dp+j+3] = min(sum3, 255);
			}
			for (; j < w; ++j) {
				int sum = buf[j] * f0;
				for (int k = 1; k < kernel_size; ++k) {
					sum += buf[k + j] * filter[k];
				}
				dst_d[dp+j] = min(sum, 255);
			}
		}
		
		sp += w;
		dp += w;
	}
	
	// vert pass
	for (int i = 0; i < w; ++i) {
		int sum = dst_d[i];
		for (int j = 0; j < half_kernel; ++j) {
			buf[j] = sum;
		}
		
		{
			int k = i;
			int j;
			for (j = 0; j <= h - 2; j += 2, k += w2) {
				buf[j+half_kernel] = dst_d[k];
				buf[j+half_kernel+1] = dst_d[k+w];
			}
			for (; j < h; ++j, k += w) {
				buf[j+half_kernel] = dst_d[k];
			}
		}
		sum = dst_d[(h-1)*w + i];
		for (int j = h; j < half_kernel + h; ++j) {
			buf[j + half_kernel] = sum;
		}
		{
			int dp = i;
			int j;
			for (j = 0; j <= h - 4; j += 4, dp += w4) {
				int sum  = buf[j]   * f0;
				int sum1 = buf[j+1] * f0;
				int sum2 = buf[j+2] * f0;
				int sum3 = buf[j+3] * f0;
				for (int k = 1; k < kernel_size; ++k) {
					double fk = filter[k];
					sum += buf[k + j] * fk;
					sum1 += buf[k + j+1] * fk;
					sum2 += buf[k + j+2] * fk;
					sum3 += buf[k + j+3] * fk;
				}
				dst_d[dp] = min(sum, 255);
				dst_d[dp+w] = min(sum1, 255);
				dst_d[dp+w2] = min(sum2, 255);
				dst_d[dp+w3] = min(sum3, 255);
			}
			for (; j < h; ++j, dp += w) {
				sum = buf[j] * f0;
				for (int k = 1; k < kernel_size; ++k) {
					sum += buf[k + j] * filter[k];
				}
				dst_d[dp] = min(sum, 255);
			}
		}
	}
}


void Resample(const pyra8::Mat& src, pyra8::Mat& dst, int nw, int nh) {
	int h = src.rows;
	int w = src.cols;
	if (h > nh && w > nw) {
		dst.SetSize(nw, nh, src.channels);
		ResampleByte(src, dst, nw, nh);
	}
	else {
		dst = src;
	}
}

void Resample(const pyraf::Mat& src, pyraf::Mat& dst, int nw, int nh) {
	int h = src.rows;
	int w = src.cols;
	if (h > nh && w > nw) {
		dst.SetSize(nw, nh, src.channels);
		ResampleFloat(src, dst, nw, nh);
	}
	else {
		dst = src;
	}
}


void BoxBlurGray(ByteMat& src, ByteMat& dst, int radius, dword options) {
	int w = src.cols;
	int h = src.rows;
	int h2 = h << 1;
	int w2 = w << 1;
	int windowSize = ((radius << 1) + 1);
	int radiusPlusOne = (radius + 1);
	int radiusPlus2 = (radiusPlusOne + 1);
	double scale = options & BOX_BLUR_NOSCALE ? 1.0 : (1.0 / (windowSize * windowSize));
	
	int sum = 0, srcIndex = 0, nextPixelIndex = 0, previousPixelIndex = 0;
	static thread_local Vector<int> data_i32;
	auto& src_data = src.data;
	int hold = 0;
	
	data_i32.SetCount((w * h) << 2);
	
	dst.SetSize(w, h, src.channels);
	
	// first pass
	// no need to scale
	for (int y = 0; y < h; ++y) {
		int dstIndex = y;
		int sum = radiusPlusOne * src_data[srcIndex];
		
		for (int i = (srcIndex + 1), end = (srcIndex + radius); i <= end; ++i) {
			sum += src_data[i];
		}
		
		nextPixelIndex = (srcIndex + radiusPlusOne);
		previousPixelIndex = srcIndex;
		hold = src_data[previousPixelIndex];
		
		int x;
		for (x = 0; x < radius; ++x, dstIndex += h) {
			data_i32[dstIndex] = sum;
			sum += src_data[nextPixelIndex] - hold;
			nextPixelIndex ++;
		}
		for (; x < w - radiusPlus2; x += 2, dstIndex += h2) {
			data_i32[dstIndex] = sum;
			sum += src_data[nextPixelIndex] - src_data[previousPixelIndex];
			
			data_i32[dstIndex+h] = sum;
			sum += src_data[nextPixelIndex+1] - src_data[previousPixelIndex+1];
			
			nextPixelIndex += 2;
			previousPixelIndex += 2;
		}
		for (; x < w - radiusPlusOne; ++x, dstIndex += h) {
			data_i32[dstIndex] = sum;
			sum += src_data[nextPixelIndex] - src_data[previousPixelIndex];
			
			nextPixelIndex ++;
			previousPixelIndex ++;
		}
		
		hold = src_data[nextPixelIndex-1];
		for (; x < w; ++x, dstIndex += h) {
			data_i32[dstIndex] = sum;
			
			sum += hold - src_data[previousPixelIndex];
			previousPixelIndex ++;
		}
		
		srcIndex += w;
	}
	
	// second pass
	srcIndex = 0;
	auto& dst_data = dst.data;
	
	// dont scale result
	if (scale == 1) {
		for (int y = 0; y < w; ++y) {
			int dstIndex = y;
			int sum = radiusPlusOne * data_i32[srcIndex];
			
			for (int i = (srcIndex + 1), end = (srcIndex + radius); i <= end; ++i) {
				sum += data_i32[i];
			}
			
			nextPixelIndex = srcIndex + radiusPlusOne;
			previousPixelIndex = srcIndex;
			hold = data_i32[previousPixelIndex];
			
			int x;
			for (x = 0; x < radius; ++x, dstIndex += w) {
				dst_data[dstIndex] = sum;
				sum += data_i32[nextPixelIndex] - hold;
				nextPixelIndex ++;
			}
			for (; x < h - radiusPlus2; x += 2, dstIndex += w2) {
				dst_data[dstIndex] = sum;
				sum += data_i32[nextPixelIndex] - data_i32[previousPixelIndex];
				
				dst_data[dstIndex+w] = sum;
				sum += data_i32[nextPixelIndex+1] - data_i32[previousPixelIndex+1];
				
				nextPixelIndex += 2;
				previousPixelIndex += 2;
			}
			for (; x < h - radiusPlusOne; ++x, dstIndex += w) {
				dst_data[dstIndex] = sum;
				
				sum += data_i32[nextPixelIndex] - data_i32[previousPixelIndex];
				nextPixelIndex ++;
				previousPixelIndex ++;
			}
			hold = data_i32[nextPixelIndex-1];
			for (; x < h; ++x, dstIndex += w) {
				dst_data[dstIndex] = sum;
				
				sum += hold - data_i32[previousPixelIndex];
				previousPixelIndex ++;
			}
			
			srcIndex += h;
		}
	}
	else {
		for (int y = 0; y < w; ++y) {
			int dstIndex = y;
			int sum = radiusPlusOne * data_i32[srcIndex];
			
			for (int i = (srcIndex + 1), end = (srcIndex + radius); i <= end; ++i) {
				sum += data_i32[i];
			}
			
			nextPixelIndex = srcIndex + radiusPlusOne;
			previousPixelIndex = srcIndex;
			hold = data_i32[previousPixelIndex];
			
			int x;
			for (x = 0; x < radius; ++x, dstIndex += w) {
				dst_data[dstIndex] = (byte)(sum * scale);
				sum += data_i32[nextPixelIndex] - hold;
				nextPixelIndex ++;
			}
			for (; x < h - radiusPlus2; x += 2, dstIndex += w2) {
				dst_data[dstIndex] = (byte)(sum * scale);
				sum += data_i32[nextPixelIndex] - data_i32[previousPixelIndex];
				
				dst_data[dstIndex+w] = (byte)(sum * scale);
				sum += data_i32[nextPixelIndex+1] - data_i32[previousPixelIndex+1];
				
				nextPixelIndex += 2;
				previousPixelIndex += 2;
			}
			for (; x < h - radiusPlusOne; ++x, dstIndex += w) {
				dst_data[dstIndex] = (byte)(sum * scale);
				
				sum += data_i32[nextPixelIndex] - data_i32[previousPixelIndex];
				nextPixelIndex ++;
				previousPixelIndex ++;
			}
			hold = data_i32[nextPixelIndex-1];
			for (; x < h; ++x, dstIndex += w) {
				dst_data[dstIndex] = (byte)(sum * scale);
				
				sum += hold - data_i32[previousPixelIndex];
				previousPixelIndex ++;
			}
			
			srcIndex += h;
		}
	}
}


void Grayscale(const ByteMat& src, ByteMat& dst) {
	int w = src.cols;
	int h = src.rows;
	dst.SetSize(w, h, 1);
	
	const byte* from = src.data.Begin();
	byte* to = dst.data.Begin();
	byte* end = dst.data.End();
	int cn = min(src.channels, 3);
	int step = src.channels;
	double mul = 1.0 / cn;
	
	if (cn == 3) {
		while (to != end) {
			int av = 0;
			for(int i = 0; i < 3; i++)
				av += from[i];
			av *= mul;
			*to++ = (byte)av;
			from += step;
		}
	}
	else {
		while (to != end) {
			int av = 0;
			for(int i = 0; i < cn; i++)
				av += from[i];
			av *= mul;
			*to++ = (byte)av;
			from += step;
		}
	}
}

void WarpPerspective(const ByteMat& src, ByteMat& dst, const FloatMat& transform, int fill_value) {
	if (dst.IsEmpty())
		dst.SetSize(src.cols, src.rows, src.channels);
	
	int src_width = src.cols;
	int src_height = src.rows;
	int src_len = src.data.GetCount();
	int dst_width = dst.cols;
	int dst_height = dst.rows;
	
	auto& src_d = src.data;
	auto& dst_d = dst.data;
	double xs = 0.0, ys = 0.0, xs0 = 0.0, ys0 = 0.0, ws = 0.0, sc = 0.0, a = 0.0, b = 0.0, p0 = 0.0, p1 = 0.0;
	auto& td = transform.data;
	auto m00 = td[0];
	auto m01 = td[1];
	auto m02 = td[2];
	auto m10 = td[3];
	auto m11 = td[4];
	auto m12 = td[5];
	auto m20 = td[6];
	auto m21 = td[7];
	auto m22 = td[8];
	
	auto dst_iter = dst_d.Begin();
	ASSERT(dst_d.GetCount() == dst_height * dst_width);
	
	for (int y = 0; y < dst_height; ++y) {
		xs0 = m01 * y + m02;
		ys0 = m11 * y + m12;
		ws  = m21 * y + m22;
		for (int x = 0; x < dst_width; ++x, xs0 += m00, ys0 += m10, ws += m20) {
			sc = 1.0 / ws;
			xs = xs0 * sc, ys = ys0 * sc;
			int ixs = xs;
			int iys = ys;
			
			if (ixs >= 0 && iys >= 0 && ixs < (src_width - 1) && iys < (src_height - 1)) {
				double a = xs - ixs;
				double b = ys - iys;
				int off0 = (int)(src_width * iys + ixs);
				int off1 = off0 + src_width;
				
				byte v0 = src_d[off0];
				byte v1 = src_d[off0 + 1];
				byte v10 = off1 < src_len ? src_d[off1] : v0;
				byte v11 = off1 < src_len ? src_d[off1+1] : v1;
				
				double p0 =		v0  + a *  (v1 -  v0);
				double p1 =		v10 + a * (v11 - v10);
				
				auto v = (p0 + b * (p1 - p0));
				*dst_iter = v;
			}
			else
				*dst_iter = fill_value;
			
			dst_iter++;
		}
	}
}

void WarpAffine(const ByteMat& src, ByteMat& dst, const FloatMat& transform, int fill_value) {
	if (dst.IsEmpty())
		dst.SetSize(src.cols, src.rows, src.channels);
	
	int src_width = src.cols;
	int src_height = src.rows;
	int src_len = src.data.GetCount();
	int dst_width = dst.cols;
	int dst_height = dst.rows;
	const auto& src_d = src.data;
	auto& dst_d = dst.data;
	
	const auto& td = transform.data;
	double m00 = td[0];
	double m01 = td[1];
	double m02 = td[2];
	double m10 = td[3];
	double m11 = td[4];
	double m12 = td[5];
	
	auto dst_iter = dst_d.Begin();
	ASSERT(dst_d.GetCount() == dst_height * dst_width);
	
	for (int y = 0; y < dst_height; ++y) {
		double xs = m01 * y + m02;
		double ys = m11 * y + m12;
		
		for (int x = 0; x < dst_width; ++x, xs += m00, ys += m10) {
			int ixs = xs;
			int iys = ys;
			
			if (ixs >= 0 && iys >= 0 && ixs < (src_width - 1) && iys < (src_height - 1)) {
				double a = xs - ixs;
				double b = ys - iys;
				int off0 = (int)(src_width * iys + ixs);
				int off1 = off0 + src_width;
				
				byte v0 = src_d[off0];
				byte v1 = src_d[off0 + 1];
				byte v10 = off1 < src_len ? src_d[off1] : v0;
				byte v11 = off1 < src_len ? src_d[off1+1] : v1;
				
				double p0 =		v0  + a *  (v1 -  v0);
				double p1 =		v10 + a * (v11 - v10);
				
				auto v = (p0 + b * (p1 - p0));
				*dst_iter = v;
			}
			else
				*dst_iter = fill_value;
			
			dst_iter++;
		}
	}
}


END_UPP_NAMESPACE
