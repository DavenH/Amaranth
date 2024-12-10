#include <fstream>
#include <ipp.h>
#include "Curve.h"
#include "../Util/NumberUtils.h"

using std::fstream;
float*** Curve::table = nullptr;

ostream& operator<<(ostream& stream, TransformParameters t) {
	stream.precision(3);
	stream << "theta:\t" << t.theta * 180 / IPP_PI << "\n" << "scaleX:\t" << t.scaleX << "\n"
			<< "scaleY:\t" << t.scaleY << "\n" << "shear:\t" << t.shear << "\n" << "sinrot:\t"
			<< t.sinrot << "\n" << "cosrot:\t" << t.cosrot << "\n" << "dpole:\t" << t.dpole << "\n"
			<< "ypole:\t" << t.ypole << "\n" << "d:\t" << t.d.x << " " << t.d.y << "\n" << "\n";
	return stream;
}

Curve::Curve(const Intercept& one, Intercept two, Intercept thr) :
		a(one)
	,	b(two)
	,	c(thr) {
	nullify();
	validate();
	update();
}

Curve::Curve(Vertex2& one, Vertex2& two, Vertex2& thr) {
	a = Intercept(one.x, one.y);
	b = Intercept(two.x, two.y);
	c = Intercept(thr.x, thr.y);

	nullify();
	validate();
	update();
}

Curve::Curve(const Curve& curve):
transformX{},
transformY{},
tp(), curveRes(0) {
	this->a = curve.a;
	this->b = curve.b;
	this->c = curve.c;

	waveIdx = curve.waveIdx;
	tableCurveIdx = curve.tableCurveIdx;
	tableCurvePos = curve.tableCurvePos;
	resIndex = curve.resIndex;
	interpolate = curve.interpolate;
}

void Curve::validate() {
  #ifdef _DEBUG
	jassert(a.x <= b.x);
	jassert(b.x <= c.x);
	jassert(a.x < c.x);

	if(a.x >= c.x)
	{
		a.x -= 0.0002f;
		b.x -= 0.0001f;
	}
	else
	{
		if(b.x > c.x)
			b.x = c.x - 0.0001f;

		if(a.x > b.x)
			a.x = b.x - 0.0001f;
	}
  #endif
}

// must be done independently of constructor / destructor
// as these will be called during vector copy
void Curve::construct() {
  #if USE_IPP_AFFINE_TRANSFORM
	affineMatrix = ippsMalloc_32f(16);
	transformX = ippsMalloc_32f(resolution);
	transformY = ippsMalloc_32f(resolution);
	paired = ippsMalloc_32f(resolution * 2);
	dest = ippsMalloc_32f(2 * (resolution << resIndex));
  #endif
}

void Curve::calcTable() {
    table = new float** [resolutions];
    for (int r = 0; r < resolutions; ++r) {
        table[r] = new float* [numCurvelets];
        for (int j = 0; j < numCurvelets; ++j) {
            table[r][j] = ippsMalloc_32f(2 * resolution >> r);
        }
	}

	float tn, temp, x, y, value, sx;
	int res;

    for (int r = 0; r < resolutions; ++r) {
        for (int i = 0; i < numCurvelets; ++i) {
            x = double(i) / numCurvelets;
            sx = IPP_PI2 * (2 * x - x * x);
            tn = ::tan(sx);
            res = resolution >> r;

            for (int j = 0; j < res / 2; ++j) {
                y = yValue(i, j, res);
                temp = tn - y * tn + 2.0 / IPP_PI;
                value = 0.5f * ::acosf(y / temp) * temp / (1 + tn * IPP_PI2) + 0.5f;

                table[r][i][res - 1 - j] = value;
                table[r][i][res - 1 - j + res] = y;
            }

            for (int j = res / 2; j < res; ++j) {
				table[r][i][res - 1 - j] = 1 - table[r][i][j];
				table[r][i][res - 1 - j + res] = table[r][i][j + res];
			}
		}
	}
}


double Curve::function(const double x, const double t) {
    double tant = tanf(t);
    double tantz = tant + 1 - x;
    return 2.0 / IPP_PI * acos(tant * x / tantz) * tantz / (tant + 1);
}

float Curve::yValue(int theta, int index, int res) {
    double tan, root, x, xx, ret;
	xx 		= double(theta) / numCurvelets;
	tan 	= ::tan(IPP_PI2 * (2 * xx - xx * xx));
	root 	= (2.0 / IPP_PI + tan) / (1 + tan);
	x 		= 2 * double(index) / (res - 0.9999);
	ret 	= (1 - (x - 1) * (x - 1)) * root;
	return ret;
}

void Curve::deleteTable() {
    if (table) {
        for (int r = 0; r < resolutions; r++) {
            for (int j = 0; j < numCurvelets; ++j)
                ippsFree(table[r][j]);

            delete[] table[r];
        }

        delete[] table;
        table = nullptr;
    }
}

void Curve::update() {
    updateCurrentIndex();
}

void Curve::setResIndex(int index) {
    resIndex = index;
}

void Curve::recalculateCurve() {
    // the curve uses a time-guide, so no need for transform array
    if (b.padAfter || b.padBefore) {
		recalculatedPadded();
		tp.scaleY = 1.f;
		return;
	}

	float m, dx, dyca, distbi, distbd;
	float diff;

	Vertex2 icpt;

	dx 			= (c.x - a.x);
	dyca 		= (c.y - a.y);

	Ipp32fc cvec = { dx, dyca };
	ippsPhase_32fc(&cvec, &tp.theta, 1);

	float ntheta = -tp.theta;
	ippsSin_32f_A21(&ntheta, &tp.sinrot, 1);
	ippsCos_32f_A24(&ntheta, &tp.cosrot, 1);

	float dist2 = dx * dx + dyca * dyca;
	ippsSqrt_32f_I(&dist2, 1);

	// good luck understanding this
	tp.scaleX 	= dist2;
	m 			= dyca / dx;
	icpt.x 		= (m * m * a.x + b.y * m - a.y * m + b.x) / (1.f + m * m);		// not cheap, can improve?
	icpt.y 		= m * (icpt.x - a.x) + a.y;
	tp.d.x 		= 0.5f * (a.x + c.x);
	tp.d.y 		= 0.5f * (a.y + c.y);
	tp.ypole 	= a.y + m * (b.x - a.x) < b.y ? 1 : -1;
	tp.dpole 	= tp.d.x < icpt.x ? 1 : -1;
	distbi 		= (b.x - icpt.x) * (b.x - icpt.x) + (b.y - icpt.y) * (b.y - icpt.y);
	distbd 		= (b.x - tp.d.x) * (b.x - tp.d.x) + (b.y - tp.d.y) * (b.y - tp.d.y);
	diff 		= distbd - distbi;

	if (diff < 0)
		diff = 0;

	ippsSqrt_32f_I(&diff, 1);
	tp.shear = diff * tp.dpole * tp.ypole;

	ippsSqrt_32f_I(&distbi, 1);
	tp.scaleY = distbi;
	
	int res  = resolution >> resIndex;
	float ma = tp.scaleX * tp.cosrot;
	float mc = tp.scaleX * tp.sinrot * -1;
	float mb = tp.ypole * (tp.cosrot * tp.shear + tp.sinrot * tp.scaleY);
	float md = tp.ypole * (-tp.sinrot * tp.shear + tp.cosrot * tp.scaleY);

	// res	clk
	// 0	900
	// 1	836
	// 2	833
	float* t  = table[resIndex][tableCurveIdx];
	float* t2 = table[resIndex][tableCurveIdx + 1];

	float alpha = interpolate ? 1 - (tableCurvePos - tableCurveIdx) : 1.f;
	bool actuallyShouldInterpolate = tableCurveIdx < numCurvelets - 1 && alpha < 1.f;

	ippsSet_32f			(a.x, transformX, res);
	ippsAddProductC_32f	(t, 		ma * alpha, transformX, res);
	ippsAddProductC_32f	(t + res, 	mb * alpha, transformX, res);

    if (actuallyShouldInterpolate) {
		ippsAddProductC_32f	(t2, 		ma * (1 - alpha), transformX, res);
		ippsAddProductC_32f	(t2 + res, 	mb * (1 - alpha), transformX, res);
	}

	ippsSet_32f			(a.y, transformY, res);
	ippsAddProductC_32f	(t, 		mc * alpha, transformY, res);
	ippsAddProductC_32f	(t + res, 	md * alpha, transformY, res);

    if (actuallyShouldInterpolate) {
		ippsAddProductC_32f	(t2, 		mc * (1 - alpha), transformY, res);
		ippsAddProductC_32f	(t2 + res, 	md * (1 - alpha), transformY, res);
	}
}

void Curve::recalculatedPadded() {
    int res = resolution >> resIndex;

    ippsSet_32f(b.x, transformX, res);
    ippsSet_32f(b.y, transformY, res);
}

float Curve::getCentreX() {
    return 0.33333f * (a.x + b.x + c.x);
}

bool Curve::operator==(const Curve& other) {
    return a == other.a && b == other.b && c == other.c;
}

void Curve::updateCurrentIndex() {
    NumberUtils::constrain<float>(b.shp, 0.f, 1.f);
    tableCurvePos = (numCurvelets - 1) * b.shp;
	tableCurveIdx = int(tableCurvePos);
}
