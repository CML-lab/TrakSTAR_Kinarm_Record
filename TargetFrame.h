#ifndef TGTFRAME_H
#define TGTFRAME_H
#pragma once

// Data type used to store trial data for data writer
struct TargetFrame
{
	int trial;

	float startx;
	float starty;

	float tgtxl;
	float tgtyl;

	float tgtxr;
	float tgtyr;

	int trace;
	int path;
	int region;

	int score;

	int TrType;

	float velmin;
	float velmax;

	int probe1, probe2;
	int trcounter, tentrcounter;
	
	char key;

	int lat;

};

#endif
