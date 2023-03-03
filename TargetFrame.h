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

	int score;

	int TrType;

	int probe1, probe2;
	int trcounter;
	int databit0, databit1, databit2, databit3;
	
	char key;

};

#endif
