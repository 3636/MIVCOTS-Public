#include "AnomalyDetection.h"



AnomalyDetection::AnomalyDetection()
{
}


AnomalyDetection::~AnomalyDetection()
{
}

int AnomalyDetection::setup()
{
	return 0;
}

int AnomalyDetection::loop()
{
	int returnCode = SUCCESS;
	//wxLogDebug("Analysis Child Loop");
	sharedCache<CarData *>::cacheIter startIter, endIter, tempIter;
	carCache->readCache(&startIter, &endIter);
	for (tempIter = startIter; tempIter != endIter; tempIter++) {
		unsigned long analysisCount;
		(*tempIter)->get(ANALYSIS_COUNT_U, &analysisCount);
		if (analysisCount == 1) {
			// get new CarData
			CarData * tempCarDataPtr = nullptr;
			carPool->getCar(&tempCarDataPtr);
			if (tempCarDataPtr == nullptr) {
				returnCode = ERR_ANALYSIS_CHILD | ERR_LOOP | ERR_NULLPTR;
			}
			// update analysis count
			tempCarDataPtr->addKey(ANALYSIS_COUNT_U);
			tempCarDataPtr->set(ANALYSIS_COUNT_U, (unsigned long)2);

			double accelZ;
			(*tempIter)->get(ACC_Z_D, &accelZ);

			if ((accelZ < 0) && (!upsideDownAnomalyDetected)) {
				++upsideDownCounter;
			}
			else if (accelZ > 0) {
				if (upsideDownCounter > 0) {
					--upsideDownCounter;
				}
				else {
					upsideDownAnomalyDetected = false; // stable
				}
			}

			if (upsideDownCounter > 30) {
				//upsideDownCounter = 0;
				
				long carNum;
				(*tempIter)->get(ID_S, &carNum);
				if (!upsideDownAnomalyDetected) {
					wxLogWarning("Car %d Appears to be upside down!", carNum);
				}
				upsideDownAnomalyDetected = true;
			}

			long timeStamp = 0;
			(*tempIter)->get(TIME_S, &timeStamp);
			tempCarDataPtr->addKey(TIME_S);
			tempCarDataPtr->set(TIME_S, (long)timeStamp);


			// push to update Queue
			updateQueue->push(tempCarDataPtr);


		}
	}
	return returnCode;
}
