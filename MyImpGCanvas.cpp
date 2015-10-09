/**
 *  Copyright 2015 Daniel Korn
 */

#include "GCanvas.h"
#include <math.h>
#include "GPixel.h"
#include "GBitmap.h"
#include "GCanvas.h"
#include "GColor.h"
#include "GRect.h"
#include "GPoint.h"

#define COLOR_ROUNDING_CONSTANT 255.9999999

struct matrix {
	float a;
	float b;
	float c;
	float d;
	float e;
	float f;
	
};

struct edge {
	int yMin;
	int yMax;
	float slope;
	float curr_x;
	
	friend bool operator< (const edge a, const edge b){
		if(a.yMin == b.yMin){
			return a.curr_x < b.curr_x;
		}
		return a.yMin < b.yMin;
	}
};

class MyImpGCanvas : public GCanvas
{	
	public:
		void clear(const GColor& col) override;
		void fillRect(const GRect& rect, const GColor& col) override;
		void fillBitmapRect(const GBitmap& src, const GRect& dst) override;
		void fillConvexPolygon(const GPoint pointArray[], int count, const GColor& inputColor) override;
		void save() override;
		void restore() override;
		void concat(const float Matrix[]) override;
		
		GRect roundAndClipRectangle(const GRect& rect);
		void applyMatrix(const GBitmap& src, const GRect& dst, matrix invMat);
		
		MyImpGCanvas(const GBitmap& constructorBitmap);
				
	private:
		const GBitmap& bitmap;
		std::stack <matrix> CTM;
		edge lineFromPoints(GPoint a, GPoint b);
		
};

static matrix generateMatrixValues(const GRect& src, const GRect& dst){
	matrix valueMatrix;
	printf("dstwidth: %f, srcWidth: %f",dst.width(),src.width());
	valueMatrix.a = dst.width() / src.width();
	valueMatrix.b = 0;
	valueMatrix.c = -src.left() * valueMatrix.a + dst.left();
	valueMatrix.d = 0;
	valueMatrix.e = dst.height() / src.height();
	valueMatrix.f = -src.top() * valueMatrix.e + dst.top();
	return valueMatrix;
}

static GPixel Src_Over_Blend(const GPixel& src, const GPixel& dst){
		int a = GPixel_GetA(src);
		int pixA = a + ((255-a) * GPixel_GetA(dst) +127)/255;
		int pixR = GPixel_GetR(src) + ((255-a) * GPixel_GetR(dst) + 127)/255;
		int pixG = GPixel_GetG(src) + ((255-a) * GPixel_GetG(dst) + 127)/255;
		int pixB = GPixel_GetB(src) + ((255-a) * GPixel_GetB(dst) + 127)/255;				
//		int pixA = a + (((255-a) * GPixel_GetA(dst[x]) * 65793 + (1<<23)) >> 24);
//		int pixR = r + (((255-a) * GPixel_GetR(dst[x]) * 65793 + (1<<23)) >> 24);
//		int pixG = g + (((255-a) * GPixel_GetG(dst[x]) * 65793 + (1<<23)) >> 24);
//		int pixB = b + (((255-a) * GPixel_GetB(dst[x]) * 65793 + (1<<23)) >> 24);
		GPixel pix = GPixel_PackARGB(pixA,pixR,pixG,pixB);
		return pix;
}

static GPixel colorToPixel(const GColor& inputColor){
	GColor col = inputColor.pinToUnit();
	int a = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA);
	int r = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA * col.fR); 
	int b = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA * col.fB);
	int g = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA * col.fG);
	GPixel pix = GPixel_PackARGB(a,r,g,b);
	return pix;
}

MyImpGCanvas::MyImpGCanvas(const GBitmap& constructorBitmap):bitmap(constructorBitmap){
	matrix m = matrix();
	m.a = 1.0;
	m.b = 0;
	m.c = 0;
	m.d = 0;
	m.e = 1.0;
	m.f = 0;
	CTM.push(m);
}
	
GCanvas* GCanvas::Create(const GBitmap& bitmap){
	bool a = bitmap.rowBytes() < (bitmap.width() * sizeof(GPixel));
	if(a || bitmap.width()<0 || bitmap.height()<0){
		return NULL;
	}
	return new MyImpGCanvas(bitmap);
}
		
/**
 *  Fill the entire canvas with the specified color, using SRC porter-duff mode.
 */
void MyImpGCanvas::clear(const GColor& inputColor){
	GPixel pix = colorToPixel(inputColor);
	GPixel* dst = bitmap.fPixels;
	for (int y = 0; y < bitmap.height(); ++y) {
		for (int x = 0; x < bitmap.width(); ++x) {
            dst[x] = pix;
        }
		dst = (GPixel*)((char*)dst + bitmap.rowBytes());
    }
	return;
}
	

    /**
     *  Fill the rectangle with the color, using SRC_OVER porter-duff mode.
     *
     *  The affected pixels are those whose centers are "contained" inside the rectangle:
     *      e.g. contained == center > min_edge && center <= max_edge
     *
     *  Any area in the rectangle that is outside of the bounds of the canvas is ignored.
     */
void MyImpGCanvas::fillRect(const GRect& rect, const GColor& inputColor){
	GColor col = inputColor.pinToUnit();
	if(rect.bottom() < 0 || rect.top() > bitmap.height() || rect.right() < 0 || rect.left() > bitmap.width()){
		return;//Checks for invalid rectangles
	}
	
	GRect roundedRect = roundAndClipRectangle(rect);
	
	int top = roundedRect.top();
	int bottom = roundedRect.bottom();;
	int left = roundedRect.left();
	int right = roundedRect.right();
	
	
	GPixel* dst = bitmap.fPixels;
	dst = (GPixel*)((char*)dst + bitmap.rowBytes()*top);//multiply by the row of top to reach the proper first row
	
	int a = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA);
	int r = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA * col.fR); 
	int b = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA * col.fB);
	int g = (int)floor(COLOR_ROUNDING_CONSTANT * col.fA * col.fG);
	
	for (int y = top; y < bottom; ++y) {
		for (int x = left; x < right; ++x) {
			int pixA = a + ((255-a) * GPixel_GetA(dst[x]) + 127)/255;
			int pixR = r + ((255-a) * GPixel_GetR(dst[x]) + 127)/255;
			int pixG = g + ((255-a) * GPixel_GetG(dst[x]) + 127)/255;
			int pixB = b + ((255-a) * GPixel_GetB(dst[x]) + 127)/255;				
//			pixA = a + (((255-a) * GPixel_GetA(dst[x]) * 65793 + (1<<23)) >> 24);
//			pixR = r + (((255-a) * GPixel_GetR(dst[x]) * 65793 + (1<<23)) >> 24);
//			pixG = g + (((255-a) * GPixel_GetG(dst[x]) * 65793 + (1<<23)) >> 24);
//			pixB = b + (((255-a) * GPixel_GetB(dst[x]) * 65793 + (1<<23)) >> 24);
			GPixel pix = GPixel_PackARGB(pixA,pixR,pixG,pixB);
			dst[x] = pix;
		}
		dst = (GPixel*)((char*)dst + bitmap.rowBytes());
	}
}

void MyImpGCanvas::fillBitmapRect(const GBitmap& src, const GRect& dst){
	
	bool a = src.rowBytes() < (src.width() * sizeof(GPixel));
	if(a || src.width()<0 || src.height()<0){
		return;
	}
	
	matrix invMatrix;
	invMatrix.a = ((float)src.width())/((float)dst.width());
	invMatrix.b = 0;
	invMatrix.c = -(dst.left() * invMatrix.a);
	invMatrix.d = 0;
	invMatrix.e = ((float)src.height())/((float)dst.height());
	invMatrix.f = -(dst.top() * invMatrix.e);
		
	applyMatrix(src,dst,invMatrix);
	
}

void MyImpGCanvas::applyMatrix(const GBitmap& src, const GRect& dst, matrix invMat){
	GRect roundedRect = roundAndClipRectangle(dst);dRect.bottom());

	float initalX = (roundedRect.left() + 0.5) * invMat.a + (roundedRect.top() + 0.5) * invMat.b + invMat.c;
	float initalY = (roundedRect.left() + 0.5) * invMat.c + (roundedRect.top() + 0.5) * invMat.e + invMat.f;
	
	for(int y = roundedRect.top(); y<roundedRect.bottom();y++){
		for(int x = roundedRect.left(); x<roundedRect.right();x++){
			float matrixX = (x + 0.5) * invMat.a + (y + 0.5) * invMat.b + invMat.c;
			float matrixY = (x + 0.5) * invMat.d + (y + 0.5) * invMat.e + invMat.f;
			GPixel* srcPixel = src.getAddr((int)floor(matrixX),(int)floor(matrixY));
			GPixel* dstPixel = bitmap.getAddr(x,y);
			if(GPixel_GetA(*srcPixel)==255){
				dstPixel[0] = GPixel_PackARGB(255,GPixel_GetR(*srcPixel),GPixel_GetG(*srcPixel),GPixel_GetB(*srcPixel));
			}
			else{
				GPixel blendedPixel = Src_Over_Blend(*srcPixel, *dstPixel);
				dstPixel[0] = blendedPixel;
			}
			//X += invMat.a;
			//Y += invMat.d;
		}
		//XalongY += invMat.b;
		//YalongY += invMat.e;
	}
}

edge MyImpGCanvas::lineFromPoints(GPoint a, GPoint b){
	//x = my+b
	//m = run/rise = x_1 - x_0 / y_1 - y_0
	//x - my = b
	edge e;
	float fMin = std::min(a.y(),b.y());
	float fMax = std::max(a.y(),b.y());
	e.yMin = std::max((int)(floor(fMin+0.5)),0);
	e.yMin = std::min(e.yMin,this->bitmap.height());
	e.yMax = std::min((int)(floor(fMax+0.5)),this->bitmap.height());
	e.yMax = std::max(e.yMax,0);
	if(b.y() == a.y()){
		e.slope = 0;
	}
	else{
		e.slope = (b.x() - a.x()) / (b.y() - a.y());
	}
	float xIntercept = a.x() - e.slope * a.y();
	e.curr_x = xIntercept + e.slope * (e.yMin + 0.5);
	return e;
}

void lineDescription(edge e){
	printf("yMin:%d yMax:%d slope:%f curr_x:%f\n",e.yMin,e.yMax,e.slope,e.curr_x);
}



/**
*  Fill the convex polygon with the color, following the same "containment" rule as
*  rectangles.
*
*  Any area in the polygon that is outside of the bounds of the canvas is ignored.
*
*  If the color's alpha is < 1, blend it using SRCOVER blend mode.
*/
void MyImpGCanvas::fillConvexPolygon(const GPoint pointArray[], int count, const GColor& inputColor){
	if(count < 2){
		return;
	}
	
	GPixel pix = colorToPixel(inputColor);
	GPixel *srcPixel = &pix;
	
	edge lineArray[count];
	
	for(int i = 0; i < count-1; i++){
		lineArray[i] = lineFromPoints(pointArray[i],pointArray[i+1]);
	}
	
	lineArray[count-1] = lineFromPoints(pointArray[count-1],pointArray[0]);
	
	std::sort(&lineArray[0],&lineArray[count]);

	int left = 0;
	int right = 1;
	int i = 2;
	for(int i = 2; i <= count; i++){
		int yMin = std::max(lineArray[left].yMin,lineArray[right].yMin);
		int yMax = std::min(lineArray[left].yMax,lineArray[right].yMax);
		for(int y = yMin; y< yMax; y++){
			int xLeft = floor(lineArray[left].curr_x + 0.5);
			int xRight = floor(lineArray[right].curr_x + 0.5);
			int xMin = std::min(xLeft,xRight);
			xMin = std::max(xMin,0);
			int xMax = std::max(xLeft,xRight);
			xMax = std::min(xMax,bitmap.width());
			for(int x = xMin; x < xMax; x++){
				GPixel *dstPixel = bitmap.getAddr(x,y);
				if(GPixel_GetA(*srcPixel)==255){
					dstPixel[0] = GPixel_PackARGB(255,GPixel_GetR(*srcPixel),GPixel_GetG(*srcPixel),GPixel_GetB(*srcPixel));
				}
				else{
					GPixel blendedPixel = Src_Over_Blend(*srcPixel, *dstPixel);
					dstPixel[0] = blendedPixel;
				}
			}
			lineArray[left].curr_x = lineArray[left].curr_x + lineArray[left].slope;
			lineArray[right].curr_x = lineArray[right].curr_x + lineArray[right].slope;
		}
		if(yMax == lineArray[left].yMax){
			left = i;
		}
		else{
			right = i;
		}
	}

}

/**
 *  Saves a copy of the CTM, allowing subsequent modifications (by calling concat()) to be
 *  undone when restore() is called.
 */
void MyImpGCanvas::save(){
	matrix m = matrix(); 
	matrix curr = CTM.top();
	m.a = curr.a;
	m.b = curr.b;
	m.c = curr.c;
	m.d = curr.d;
	m.e = curr.e;
	m.f = curr.f;
	CTM.push(m);
}

/**
 *  Balances calls to save(), returning the CTM to the state it was in when the corresponding
 *  call to save() was made. These calls can be nested.
 */
void MyImpGCanvas::restore(){
	CTM.pop();
}

/**
 *  Modifies the CTM (current transformation matrix) by pre-concatenating it with the specfied
 *  matrix.
 *
 *  CTM' = CTM * matrix
 *
 *  The result is that any drawing that uses the new CTM' will be affected AS-IF it were
 *  first transformed by matrix, and then transformed by the previous CTM.
 */
void MyImpGCanvas::concat(const float matrix[]){
	matrix m = CTM.top();
	matrix concated = matrix();
	concated.a = matrix[0]*m.a + matrix[1]*m.d;
	concated.b = matrix[0]*m.b + matrix[1]*m.e;
	concated.c = matrix[0]*m.c + matrix[1]*m.f + matrix[2];	
	concated.d = matrix[3]*m.d + matrix[4]*m.e;
	concated.e = matrix[3]*m.b + matrix[4]*m.e;
	concated.f = matrix[3]*m.c + matrix[4]*m.f + matrix[5];
	&m = &concated;
	
}


GRect MyImpGCanvas::roundAndClipRectangle(const GRect& rect){
	int top;
	int bottom;
	int left;
	int right;
	
	top = (rect.top() < 0) ? 0 : (int)(floor(rect.top()+.5));
	bot = (rect.bottom() > bitmap.height()) ? bitmap.height() : (int)(floor(rect.bottom()+.5));
	left = (rect.left() < 0) ? 0 : (int)(floor(rect.left()+.5));
	right = (rect.right() > bitmap.width()) ? bitmap.width() : right = (int)(floor(rect.right()+.5));
	
	GRect betterRect;
	betterRect.setLTRB(left,top,right,bottom);
	
	return betterRect;

}