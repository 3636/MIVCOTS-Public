#pragma once

#include "Analysis.h"
#include "CacheBank.h"
#include "CarData.h"
#include "CarPool.h"
#include "DataInterface.h"
#include "DatabaseConnector.h"
#include "InterThreadComm.h"
#include "sharedCache.h"

//TODO: send analyzed data back to datastorage
class MIVCOTS
{
public:
	MIVCOTS();
	~MIVCOTS();

	typedef sharedCache<CarData*> mCache;

	int initialize();
	int initSerial(long baud, std::string port);
	// Start and stop the data interface, database, and analysis threads
	int start();
	int startSerial();
	bool serialOpen();
	int stopSerial();
	int stop();

	// Read functions for the GUI
	int acquireReadLock(long carNum, std::shared_lock<std::shared_mutex>* toLock);
	int readCache(mCache::cacheIter* startIter, mCache::cacheIter* endIter, long carNum);
	int readCache(mCache::cacheIter* startIter, mCache::cacheIter* endIter, long carNum, unsigned int length);
	int readCacheUpdates(long carNum, mCache::cacheIter* startIter, mCache::cacheIter* endIter, unsigned int updateCount);
	int readLatestUpdate(long carNum, mCache::cacheIter* iter, unsigned long updateCount);
	int endCacheRead(long carNum, std::shared_lock<std::shared_mutex>* toLock);

	bool newData(long carNum);
	int getCarNums(std::vector<long>* toWrite);

	int startPlayback(databaseInfo playbackRequest, double timeFactor);

private:
	DataInterface dataSource;
	DatabaseConnector dataStorage;
	CacheBank cacheBank;
	AnalysisParent analysisEngine;
	CarPool carSource;

	InterThreadComm<CarData*, CarData*> dataSource_dataStorage;
	InterThreadComm<CarData*, CarData*> boxDataSource_dataStorage;
	InterThreadComm<CarData*, CarData*> analysis_dataStorage;
};

