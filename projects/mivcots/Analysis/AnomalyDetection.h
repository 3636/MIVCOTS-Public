#pragma once
#include "AnalysisChild.h"
class AnomalyDetection :
	public AnalysisChild
{
public:
	AnomalyDetection();
	~AnomalyDetection();


private:
	bool speedAnomalyDetected;
	bool upsideDownAnomalyDetected;

	int setup();
	int loop();

	
};

