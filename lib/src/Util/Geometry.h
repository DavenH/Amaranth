#pragma once
class Vertex2;

class Geometry {
public:
    enum IsctModes {
		IntersectAtEnds,
		Intersect,
		DoNotIntersect
	};

	template<class T>
    static float getTriangleArea(const T& a, const T& b, const T& c) {
        return 0.5f * fabsf((a.x - c.x) * (b.y - a.y) - (a.x - b.x) * (c.y - a.y));
	}

	static Vertex2 getCrossPoint(float x1, float x2, float x3, float x4,
								 float y1, float y2, float y3, float y4);


	static bool isOnSegment(double xi, double yi, double xj, double yj,
	                        double xk, double yk);

	static char computeDirection(double xi, double yi, double xj, double yj,
	                             double xk, double yk);

	static int doLineSegmentsIntersect(double x1, double y1, double x2, double y2,
									   double x3, double y3, double x4, double y4);

	static bool withinBoundingBox(Vertex2& v, Vertex2& one, Vertex2& two);

	static Vertex2 getSpacedVertex(int dist, Vertex2& a, Vertex2& b);
};
