#include "display.h"

#define FIXED_POINT_SHIFT 8
#define fixed_t int32_t
#define FLOAT_TO_FIXED(x) ((fixed_t)((x) * (1 << FIXED_POINT_SHIFT)))
#define FIXED_TO_FLOAT(x) ((float)(x) / (1 << FIXED_POINT_SHIFT))
#define INT32_TO_FIXED(x) ((fixed_t)((x) << FIXED_POINT_SHIFT))
#define FIXED_TO_INT32(x) ((int32_t)(x) >> FIXED_POINT_SHIFT)
#define FIXED_MUL(x, y) (((x) * (y)) >> FIXED_POINT_SHIFT)
#define FIXED_DIV(x, y) (((x) << FIXED_POINT_SHIFT) / (y))

#define LCD_COLUMNS 400
#define LCD_ROWS 240
#define LCD_STRIDE 52

static inline int32_t int32_min(int32_t a, int32_t b)
{ 
	return b + ((a-b) & (a-b)>>31);
}

static inline int32_t int32_max(int32_t a, int32_t b)
{ 
	return a - ((a-b) & (a-b)>>31);
}

static inline uint32_t swap(uint32_t n)
{
#if TARGET_PLAYDATE
	//return __REV(n);
	uint32_t result;
	
	__asm volatile ("rev %0, %1" : "=l" (result) : "l" (n));
	return(result);
#else
	return ((n & 0xff000000) >> 24) | ((n & 0xff0000) >> 8) | ((n & 0xff00) << 8) | (n << 24);
#endif
}

Polygon* original_polygon;
Polygon* work_polygon;

Polygon* createPolygon(int numPoints)
{
  Polygon* polygon = (Polygon*)malloc(sizeof(Polygon));
  polygon->points = (Point*)malloc(sizeof(Point) * numPoints);
  polygon->numPoints = numPoints;
  return polygon;
}

void freePolygon(Polygon** polygon)
{
  free((*polygon)->points);
  free(*polygon);
}

static inline void
_drawMaskPattern(uint32_t* p, uint32_t mask, uint32_t color)
{
	if ( mask == 0xffffffff )
		*p = color;
	else
		*p = (*p & ~mask) | (color & mask);
}

static void
drawFragment(uint32_t* row, int x1, int x2, uint32_t color)
{
	if ( x2 < 0 || x1 >= LCD_COLUMNS )
		return;
	
	if ( x1 < 0 )
		x1 = 0;
	
	if ( x2 > LCD_COLUMNS )
		x2 = LCD_COLUMNS;
	
	if ( x1 > x2 )
		return;
	
	// Operate on 32 bits at a time
	
	int startbit = x1 % 32;
	uint32_t startmask = swap((1 << (32 - startbit)) - 1);
	int endbit = x2 % 32;
	uint32_t endmask = swap(((1 << endbit) - 1) << (32 - endbit));
	
	int col = x1 / 32;
	uint32_t* p = row + col;

	if ( col == x2 / 32 )
	{
		uint32_t mask = 0;
		
		if ( startbit > 0 && endbit > 0 )
			mask = startmask & endmask;
		else if ( startbit > 0 )
			mask = startmask;
		else if ( endbit > 0 )
			mask = endmask;
		
		_drawMaskPattern(p, mask, color);
	}
	else
	{
		int x = x1;
		
		if ( startbit > 0 )
		{
			_drawMaskPattern(p++, startmask, color);
			x += (32 - startbit);
		}
		
		while ( x + 32 <= x2 )
		{
			_drawMaskPattern(p++, 0xffffffff, color);
			x += 32;
		}
		
		if ( endbit > 0 )
			_drawMaskPattern(p, endmask, color);
	}
}

void initDisplay(PlaydateAPI* pd)
{
  original_polygon = createPolygon(5);

  original_polygon->points[0].x = 200;
  original_polygon->points[0].y = 120;
  original_polygon->points[1].x = 100;
  original_polygon->points[1].y = 150;
  original_polygon->points[2].x = 100;
  original_polygon->points[2].y = 170;
  original_polygon->points[3].x = 250;
  original_polygon->points[3].y = 220;
  original_polygon->points[4].x = 300;
  original_polygon->points[4].y = 200;

  work_polygon = createPolygon(5);
}

void drawPolygon(uint8_t* frameBuffer, Polygon* polygon, PlaydateAPI* pd)
{
  // Find top-most point
  // Find bottom-most point

  float topY = polygon->points[0].y;
  float bottomY = polygon->points[0].y;
  int topIndex = 0;
  int bottomIndex = 0;
  int currentIndex = 1;
  while(currentIndex < polygon->numPoints)
  {
    if(polygon->points[currentIndex].y < topY)
    {
      topY = polygon->points[currentIndex].y;
      topIndex = currentIndex;
    }
    if(polygon->points[currentIndex].y > bottomY)
    {
      bottomY = polygon->points[currentIndex].y;
      bottomIndex = currentIndex;
    }
    currentIndex++;
  }


  int leftBranchIndex = topIndex;
  int rightBranchIndex = topIndex;
  float leftX = polygon->points[leftBranchIndex].x;
  float rightX = polygon->points[rightBranchIndex].x;
  float currentY = polygon->points[leftBranchIndex].y;

  int leftBranchNextIndex = leftBranchIndex + 1;
  if(leftBranchNextIndex >= polygon->numPoints)
    leftBranchNextIndex = 0;
  
  int rightBranchNextIndex = rightBranchIndex - 1;
  if(rightBranchNextIndex < 0)
    rightBranchNextIndex = polygon->numPoints - 1;

  float leftSlope = (polygon->points[leftBranchNextIndex].x - polygon->points[leftBranchIndex].x) / (polygon->points[leftBranchNextIndex].y - polygon->points[leftBranchIndex].y);
  float rightSlope = (polygon->points[rightBranchNextIndex].x - polygon->points[rightBranchIndex].x) / (polygon->points[rightBranchNextIndex].y - polygon->points[rightBranchIndex].y);

  int done = 0;
  while (!done)
  {
    if(polygon->points[leftBranchNextIndex].y <= polygon->points[rightBranchNextIndex].y)
    {
      fillTrapeze(frameBuffer, &leftX, &rightX, currentY, polygon->points[leftBranchNextIndex].y, leftSlope, rightSlope, pd);
      currentY = polygon->points[leftBranchNextIndex].y;
      leftX = polygon->points[leftBranchNextIndex].x;
      leftBranchIndex = leftBranchNextIndex;
      leftBranchNextIndex = leftBranchIndex + 1;
      if(leftBranchNextIndex >= polygon->numPoints)
        leftBranchNextIndex = 0;
      leftSlope = (polygon->points[leftBranchNextIndex].x - polygon->points[leftBranchIndex].x) / (polygon->points[leftBranchNextIndex].y - polygon->points[leftBranchIndex].y);
    }
    else
    {
      fillTrapeze(frameBuffer, &leftX, &rightX, currentY, polygon->points[rightBranchNextIndex].y, leftSlope, rightSlope, pd);
      currentY = polygon->points[rightBranchNextIndex].y;
      rightX = polygon->points[rightBranchNextIndex].x;
      rightBranchIndex = rightBranchNextIndex;
      rightBranchNextIndex = rightBranchIndex - 1;
      if(rightBranchNextIndex < 0)
        rightBranchNextIndex = polygon->numPoints - 1;
      rightSlope = (polygon->points[rightBranchNextIndex].x - polygon->points[rightBranchIndex].x) / (polygon->points[rightBranchNextIndex].y - polygon->points[rightBranchIndex].y);
    }

    done = leftBranchIndex == bottomIndex || rightBranchIndex == bottomIndex;
  }

}

void fillTrapeze(uint8_t* frameBuffer, float* xLeft, float* xRight, float yStart, float yEnd, float leftSlope, float rightSlope, PlaydateAPI* pd)
{
  //pd->system->logToConsole("fillTrapeze %f %f %f %f %f %f", *xLeft, *xRight, yStart, yEnd, leftSlope, rightSlope);
  float y = yStart;
  while(y < yEnd && y < 240)
  {
    if(y>=0)
    {
      drawFragment((uint32_t*)(frameBuffer + ((int)y * LCD_STRIDE)), (int)(*xLeft+0.5f), (int)(*xRight+0.5f), 0xFFFFFFFF);
    }
    y++;
    *xLeft += leftSlope;
    *xRight += rightSlope;
  }
}

int draw(uint8_t* frameBuffer, PlaydateAPI* pd)
{
  float angle = pd->system->getCrankAngle()/180.f*M_PI;

  for(int i = 0; i < original_polygon->numPoints; i++)
  {
    work_polygon->points[i].x = (original_polygon->points[i].x-200.f) * cosf(angle) - (original_polygon->points[i].y-120.f) * sinf(angle) + 200.f;
    work_polygon->points[i].y = (original_polygon->points[i].x-200.f) * sinf(angle) + (original_polygon->points[i].y-120.f) * cosf(angle) + 120.f;
  }

  drawPolygon(frameBuffer, work_polygon, pd);

  return 1;
}
