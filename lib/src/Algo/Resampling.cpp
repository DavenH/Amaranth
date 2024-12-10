#include "Resampling.h"
#include "../Curve/Vertex2.h"

Resampling::Resampling() = default;

float Resampling::at(float x, const vector<Vertex2>& points, int& currentIndex) {
	if (points.empty())
		return 0;

	if(points.size() == 2)
		return lerp(points[0].x, points[0].y, points[1].x, points[1].y, x);

	if(x == points.front().x)
		return points.front().y;

	if(x == points.back().x)
		return points.back().y;

	while (currentIndex > 0 && x < points[currentIndex].x)
		--currentIndex;

	int limit = points.size() - 1;
	while (currentIndex < limit && x > points[currentIndex].x)
		++currentIndex;

	const Vertex2& one = points[currentIndex - 1];
	const Vertex2& two = points[currentIndex];

	float ret = (x - one.x) / (two.x - one.x) * (two.y - one.y) + one.y;
	return ret;
}

void Resampling::linResample(
        const Buffer<float>& source,
        const Buffer<float>& dest,
        double sourceToDestRatio,
        double& phase) {
	int srcSizeSub1 = source.size() - 1;

	jassert(! (srcSizeSub1 & (srcSizeSub1 + 1)));

	if(phase > source.size())
		phase -= double(source.size());

	jassert(phase + dest.size() * sourceToDestRatio > source.size());

	for (auto& v : dest) {
		int trunc = (int) phase;
		float remainder = phase - trunc;

		v = (1 - remainder) * source[trunc & srcSizeSub1] + remainder * source[(trunc + 1) & srcSizeSub1];
		phase += sourceToDestRatio;
	}
}

void Resampling::linResample2(
        const Buffer<float>& source,
        Buffer<float> dest,
        double sourceToDestRatio,
        float& padA,
        float& padB,
        double& phase) {
    float remainder;
	int trunc;
	int start = 0;

	if(phase > 0.)
		phase -= double(source.size());

	jassert(phase > -sourceToDestRatio - 0.001);
	jassert(phase + dest.size() * sourceToDestRatio < source.size());

    if (phase < -2) {
        phase = -1.999999;
    }

    if (phase < -1) {
        trunc = floorf(phase);
        remainder = phase - trunc;

        dest[start++] = (1 - remainder) * padA + remainder * padB;
        phase += sourceToDestRatio;
    }

    if (phase < 0) {
        trunc = floorf(phase);
		remainder = phase - trunc;

		dest[start++] = (1 - remainder) * padB + remainder * source[0];
		phase += sourceToDestRatio;
	}

	jassert(phase >= 0);

    for (int i = start; i < dest.size(); ++i) {
        trunc = (int) phase;
		remainder = phase - trunc;

		dest[i] = (1 - remainder) * source[trunc] + remainder * source[trunc + 1];
		phase += sourceToDestRatio;
	}

	padA = source[source.size() - 2];
	padB = source[source.size() - 1];
}


float Resampling::hermite6_3(float x, float yn2, float yn1, float y0, float y1, float y2, float y3) {
	float c0 = y0;
	float c1 = 1/12.f * (yn2 - y2) + 2/3.f  * (y1 - yn1);
	float c2 = 5/4.f  * yn1  	   - 7/3.f  * y0 		 + 5/3.f * y1 - 1/2.f * y2 + 1/12.f * y3 - 1/6.f * yn2;
	float c3 = 1/12.f * (yn2 - y3) + 7/12.f * (y2 - yn1) + 4/3.f * (y0 - y1);

	return ((c3 * x + c2) * x + c1) * x + c0;
}


float Resampling::bspline6_5(float x, float yn2, float yn1, float y0, float y1, float y2, float y3) {
	float c0, c1, c2, c3, c4, c5;
	float ym1py1, y1mym1, ym2py2, y2mym2, sixthym1py1;

	ym2py2 		= yn2 + y2;
	ym1py1 		= yn1 + y1;
	y2mym2 		= y2 - yn2;
	y1mym1 		= y1 - yn1;
	sixthym1py1 = 1/6.f * ym1py1;

	c0 = 1/120.f * ym2py2 + 13/60.f * ym1py1 + 11/12.f * y0;
	c1 = 1/24.f  * y2mym2 + 5/12.f * y1mym1;
	c2 = 1/12.f  * ym2py2 + sixthym1py1 - 1/2.f * y0;
	c3 = 1/12.f  * y2mym2 - 1/6.f * y1mym1;
	c4 = 1/24.f  * ym2py2 - sixthym1py1 + 1/4.f * y0;
	c5 = 1/120.f * (y3 - yn2) + 1/24.f * (yn1 - y2) + 1/12.f * (y1 - y0);

	return 0.73170767f * (((((c5 * x + c4) * x + c3) * x + c2) * x + c1) * x + c0);
}


void Resampling::resample(
        const Buffer<float>& source,
        Buffer<float> dest, double sourceToDestRatio,
		float& p0, float& p1, float& p2, float& p3, float& p4, float& p5, float& p6,
		double& phase, int algo) {
	int srcSize = source.size();
	float* src = source.get();

	float* pads[][6] = {
			//	0	1		2		3		4		5
			{ &p0,	&p1,	&p2, 	&p3, 	&p4, 	&p5, 	},
			{ &p1, 	&p2, 	&p3, 	&p4, 	&p5, 	&p6, 	},
			{ &p2, 	&p3, 	&p4, 	&p5,	&p6, 	src, 	},
			{ &p3, 	&p4, 	&p5,	&p6, 	src,	src+1,	},
			{ &p4, 	&p5,	&p6, 	src,	src+1,	src+2,	},
			{ &p5,	&p6, 	src,	src+1,	src+2,	src+3,	},
			{ &p6, 	src,	src+1,	src+2,	src+3,	src+4,	}
	};

	float x;
	int trunc;
	int start = 0;

	if(srcSize < 6)
		return;

//	jassert(phase > -2);

    while (floorf(phase) < 5.f) {
        trunc = jmax(-2, (int) floorf(phase));
		jassert(trunc >= -2);

		x 			= jmax(0., phase - trunc);
		float** p 	= pads[trunc + 2];

		switch(algo) {
            case Linear:
				dest[start++] = (1 - x) * *p[4] + x * *p[5];
				break;

			case Hermite:
				dest[start++] = hermite6_3(x, *p[0], *p[1], *p[2], *p[3], *p[4], *p[5]);
				break;

			case BSpline:
				dest[start++] = bspline6_5(x, *p[0], *p[1], *p[2], *p[3], *p[4], *p[5]);
				break;
		}

		phase += sourceToDestRatio;
	}

    switch (algo) {
        case Linear: {
            for (int i = start; i < dest.size(); ++i) {
				trunc = (int) phase;
				x 	  = phase - trunc;

				dest[i] = (1 - x) * source[trunc] + x * source[trunc + 1];
				phase += sourceToDestRatio;
			}

			break;
		}

        case Hermite: {
            float c0, c1, c2, c3;
            float yn2, yn1, y0, y1, y2, y3;

            for (int i = start; i < dest.size(); ++i) {
                trunc = (int) (phase + 1e-6f);
				x 	  = phase - trunc;

				yn2 = source[trunc-5];
				yn1 = source[trunc-4];
				y0  = source[trunc-3];
				y1  = source[trunc-2];
				y2  = source[trunc-1];
				y3  = source[trunc];

				c0 = y0;
				c1 = 1/12.f * (yn2 - y2) + 2/3.f  * (y1 - yn1);
				c2 = 5/4.f  * yn1  		 - 7/3.f  * y0 			+ 5/3.f * y1 - 1/2.f * y2 + 1/12.f * y3 - 1/6.f * yn2;
				c3 = 1/12.f * (yn2 - y3) + 7/12.f * (y2 - yn1) 	+ 4/3.f * (y0 - y1);

				dest[i] = ((c3 * x + c2) * x + c1) * x + c0;

				phase += sourceToDestRatio;
			}

			break;
		}

        case BSpline: {
            float c0, c1, c2, c3, c4, c5;
            float yn2, yn1, y0, y1, y2, y3;
            float ym1py1, y1mym1, ym2py2, y2mym2, sixthym1py1;

            for (int i = start; i < dest.size(); ++i) {
				trunc = (int) phase;
				x = phase - trunc;

				yn2 = source[trunc-5];
				yn1 = source[trunc-4];
				y0  = source[trunc-3];
				y1  = source[trunc-2];
				y2  = source[trunc-1];
				y3  = source[trunc];

				ym2py2 		= yn2 + y2;
				ym1py1 		= yn1 + y1;
				y2mym2 		= y2 - yn2;
				y1mym1 		= y1 - yn1;
				sixthym1py1 = 1/6.f * ym1py1;

				c0 = 1/120.f * ym2py2 + 13/60.f * ym1py1 + 11/12.f * y0;
				c1 = 1/24.f  * y2mym2 + 5/12.f * y1mym1;
				c2 = 1/12.f  * ym2py2 + sixthym1py1 - 1/2.f * y0;
				c3 = 1/12.f  * y2mym2 - 1/6.f * y1mym1;
				c4 = 1/24.f  * ym2py2 - sixthym1py1 + 1/4.f * y0;
				c5 = 1/120.f * (y3 - yn2) + 1/24.f * (yn1 - y2) + 1/12.f * (y1 - y0);

				dest[i] = 0.73170767f * (((((c5 * x + c4) * x + c3) * x + c2) * x + c1) * x + c0);

				phase += sourceToDestRatio;
			}

			break;
		}

		default:
			jassertfalse;
	}

	if(phase > 0.)
		phase -= double(srcSize);

	jassert(phase > -3);

	p0 = source[srcSize - 7];
	p1 = source[srcSize - 6];
	p2 = source[srcSize - 5];
	p3 = source[srcSize - 4];
	p4 = source[srcSize - 3];
	p5 = source[srcSize - 2];
	p6 = source[srcSize - 1];
}

float Resampling::lerp(float a, float b, float pos) {
    return (1 - pos) * a + pos * b;
}

float Resampling::lerp(float x1, float y1, float x2, float y2, float pos) {
    if (pos == 0)
		return y1;
	if(pos == 1.f)
		return y2;

	float remainder = pos - x1;
	float diffX = x2 - x1;
	float ratio = diffX == 0 ? 1.f : remainder / diffX;
	float lerpY = (1 - ratio) * y1 + ratio * y2;
	return lerpY;
}

float Resampling::lerpC(Buffer<float> buff, float unitPos) {
	int size = buff.size();

	if(size == 0)
		return 0;

	unitPos 		= jlimit(0.f, 1.f, unitPos);
	float findex 	= size * unitPos;
	int iindex 		= (int) findex;

	if(iindex >= size - 1)
		return buff.back();

	float remainder = findex - (float) iindex;
	return buff[iindex] * (1 - remainder) + buff[iindex + 1] * remainder;
}

float Resampling::interpValueQuadratic(float y1, float y2, float y3) {
	float d = (y1 - 2 * y2 + y3);

	if(d != 0.f)
	{
		float peak = 0.5 * (y1 - y3) / d;
		return y2 - 0.25f * (y1 - y3) * peak;
	}

	return y2;
}

float Resampling::interpIndexQuadratic(float y1, float y2, float y3) {
	float peak = 0;
	float d = (y1 - 2 * y2 + y3);

	if(d != 0.f)
		peak = 0.5 * (y1 - y3) / d;

	return peak;
}
