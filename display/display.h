#pragma once

#include "pd_api.h"

typedef struct
{
  float x;
  float y;
} Point;

typedef struct
{
  Point* points;
  int numPoints;
  uint32_t pattern;
} Polygon;

Polygon* createPolygon(int numPoints);

void freePolygon(Polygon** polygon);

void initDisplay(PlaydateAPI* pd);

void drawPolygon(uint8_t* frameBuffer, Polygon* polygon, PlaydateAPI* pd);

void fillTrapeze(uint8_t* frameBuffer, float* xLeft, float* xRight, float yStart, float yEnd, float leftSlope, float rightSlope, PlaydateAPI* pd);

int draw(uint8_t* frameBuffer, PlaydateAPI* pd);
