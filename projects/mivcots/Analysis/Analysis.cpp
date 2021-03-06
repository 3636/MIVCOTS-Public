#include "Analysis.h"
#include <wx/log.h>
#include "AnalysisReturnCodes.h"



AnalysisParent::AnalysisParent()
{
	analysisFinishedCounterInt.store(0, std::memory_order_relaxed);
	this->analysisSyncVars.analysisFinishedCounterInt = &(this->analysisFinishedCounterInt);
	this->analysisSyncVars.analysisFinishedCounterMutex = &(this->analysisFinishedCounterMutex);
	this->analysisSyncVars.analysisStepConditionVariable = &(this->analysisStepConditionVariable);
	this->analysisSyncVars.analysisStepInt = &(this->analysisStepInt);
	this->analysisSyncVars.analysisStepMutex = &(this->analysisStepMutex);
}


AnalysisParent::~AnalysisParent()
{
}



int AnalysisParent::init(
	sharedCache<CarData*> * boxCache,
	sharedCache<CarData*> * carCache,
	endpoint<CarData*> * updateQueue,
	endpoint<CarData*>* storageQueue,
	CarPool * carPool,
	std::string configFileName
)
{
	int returnCode = 0;
	if (boxCache != nullptr) {
		this->boxCache = boxCache;
	}
	else {
		wxLogError("Analysis Parent - Init Function: boxCache Pointer is Null");
		returnCode |= ERR_BOX_CACHE_PTR_IS_NULL;
	}
	if (carCache != nullptr) {
		this->carCache = carCache;
	}
	else {
		wxLogError("Analysis Parent - Init Function: carCache Pointer is Null");
		returnCode |= ERR_CAR_CACHE_PTR_IS_NULL;
		return returnCode;
	}
	if (updateQueue != nullptr) {
		this->updateQueue = updateQueue;
	}
	else {
		wxLogError("Analysis Parent - Init Function: updateQueue Pointer is Null");
		returnCode |= ERR_UPDATE_QUEUE_PTR_IS_NULL;
		return returnCode;
	}
	if (carPool != nullptr) {
		this->carPool = carPool;
	}
	else {
		wxLogError("Analysis Parent - Init Function: carPool Pointer is Null");
		returnCode |= ERR_CAR_CACHE_PTR_IS_NULL;
		return returnCode;
	}
	if (!(configFileName.empty())) {
		this->configFileName = configFileName;
	}
	else {
		wxLogError("Analysis Parent - Init Function: Config File Name is Empty");
		returnCode |= WARN_CONFIG_FILENAME_IS_EMPTY;

	}

	this->storageQueue = storageQueue; // TODO: error checking

	return returnCode;
}

int AnalysisParent::start()
{
	analysisParentThread = std::thread(&AnalysisParent::runAnalysisThread, this);
	return 0;
}

int AnalysisParent::stop()

{
	
	//TODO: Make sure to call delete on all the stuff that start() calls new on.
	while (analysisChildVector.size() > 0) {	// TODO: This should be a for loop
		analysisChildVector.back()->stop();
		analysisChildVector.pop_back();
	}
	analysisChildVector.clear();
	isRunning.store(false, std::memory_order_relaxed);
	// notify all
	std::unique_lock<std::mutex> analysisStepLock(analysisStepMutex);
	analysisStepInt.store(true, std::memory_order_relaxed);
	analysisStepLock.unlock();
	analysisStepConditionVariable.notify_all();
	
	if (analysisParentThread.joinable()) {
		analysisParentThread.join();
	}

	return 0;
}

int AnalysisParent::runAnalysisThread()
{
	isRunning.store(true, std::memory_order_relaxed);
	setup();
	//Sleep(1000);
	while (isRunning.load(std::memory_order_relaxed)) {
		loop();
	}
	return 0;
}

int AnalysisParent::setup()
{
	int returnCode = SUCCESS;
	analysisChildVector.push_back(new AnalysisChild);
	analysisChildVector.push_back(new AnomalyDetection);
	for (AnalysisChild* tempPtr : analysisChildVector) {
		returnCode |= tempPtr->init(
			boxCache,
			carCache,
			&analysisChildrenUpdateQueue,
			carPool,
			&analysisSyncVars
		);
	}
	
	if (returnCode) {
		return returnCode;
	}
	for (AnalysisChild* tempPtr : analysisChildVector) {
		returnCode = tempPtr->start();
	}
	
	return returnCode;
}

int AnalysisParent::loop()
{
	analysisFinishedCounterInt.store(0, std::memory_order_relaxed);

	// acquire read lock on cache
	//wxLogDebug("Analysis is trying to get read lock");
	std::shared_lock<std::shared_mutex> toLock;
	carCache->acquireReadLock(&toLock);
	//wxLogDebug("Analysis got read lock");


	// notify all
	std::unique_lock<std::mutex> analysisStepLock(analysisStepMutex);
	analysisStepInt.store(true, std::memory_order_relaxed);
	analysisStepLock.unlock();
	analysisStepConditionVariable.notify_all();
	// while counter is less than length of children vector
	while (analysisFinishedCounterInt.load(std::memory_order_relaxed) < (int)analysisChildVector.size()) {
		// aggregate stuff in output queues from children
		this->aggregate();
		Sleep(1);
	}
	while (analysisChildrenUpdateQueue.size()) {
		this->aggregate();// finish aggregating
	}

	if (analysisAggregationSet.empty()) {
		carCache->releaseReadLock(&toLock);
		//analysisStepLock.lock();
		analysisStepInt.store(false, std::memory_order_relaxed);
		//analysisStepLock.unlock();
		Sleep(ANALYSIS_PARENT_DELAY_MS);// This is necessary to stop the thread from eating the processor
		return 0;
	}

	for (CarData* currentCarDataPtr : analysisAggregationSet) {
		CarData* tempCarDataMergedPtr;
		carPool->getCar(&tempCarDataMergedPtr);						// Allocate a CarData 
		sharedCache<CarData*>::cacheIter tempCarDataOriginalIter;
		carCache->find(currentCarDataPtr, &tempCarDataOriginalIter);// grab iterators to originals
		(*tempCarDataMergedPtr) += *(*tempCarDataOriginalIter);		// copy original to update slot
		(*tempCarDataMergedPtr) += (*currentCarDataPtr);			// apply updates to copies
		//analysisUpdateQueue.push_back(tempCarDataMergedPtr);

		// push updated copies into databases and cache update queues
		updateQueue->send(tempCarDataMergedPtr);					// Send stuff back to cache
		// new copy for the database
		CarData* tempCarDataCopy;
		carPool->copyCar(&tempCarDataCopy, tempCarDataMergedPtr);
		storageQueue->send(tempCarDataCopy);						// Send copy to database

		tempCarDataMergedPtr->printCar();	// For debugging :)
		
		// TODO: add second queue for sending stuff back to database
		
		carPool->releaseCar(currentCarDataPtr);
	}
	analysisAggregationSet.clear();

	// TODO: Does this need to happen earlier?
	//analysisStepLock.lock();
	analysisStepInt.store(false, std::memory_order_relaxed);
	//analysisStepLock.unlock();
	// release read lock on cache
	carCache->releaseReadLock(&toLock);
	// try acquire write lock on cache
	wxLogDebug("Analysis attempting Cache write");
	carCache->updateCache();
	wxLogDebug("Analysis wrote cache");

	Sleep(100);

	return 0;
}

int AnalysisParent::aggregate()
{
	CarData * tmpCarDataPtr = nullptr; // Create temporary Car Data Pointer
	analysisChildrenUpdateQueue.pop(&tmpCarDataPtr); // Pop item from analysisChildrenUpdateQueue
	if (tmpCarDataPtr == nullptr) {
		return ERR_NULLPTR;
	}
	std::pair<std::set<CarData*, carTimeStampCompareLess>::iterator, bool> tmpPair = analysisAggregationSet.insert(tmpCarDataPtr);// TODO: add typedef for the iterator... seriously
	if (!(tmpPair.second)) {
		*(*(tmpPair.first)) += (*tmpCarDataPtr);	// Insert Item into analysisAggregationQueue or Merge into existing update entry
		
		carPool->releaseCar(tmpCarDataPtr);
	}
	// TODO: get rid of the memory leak (it should be fixed, but leaving this here until we know for sure)
	return 0;
}

