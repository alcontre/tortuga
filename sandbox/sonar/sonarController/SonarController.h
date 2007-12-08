/*
 *  SonarController.h
 *  sonarController
 *
 *  Created by Leo Singer on 11/25/07.
 *  Copyright 2007 Robotics@Maryland. All rights reserved.
 *
 *  Acquire samples from ADCs, compute a single-band, sliding DFT, and 
 *  detect edges.
 *
 */


#ifndef SONARCONTROLLER_H
#define SONARCONTROLLER_H


#include <stdint.h>
#include <vector>


#include "Sonar.h"
#include "SonarChunk.h"


namespace ram {
namespace sonar {


/** Describes the state of the entire SONAR system.
 */
typedef enum sonarstate_e {
	
	//	INIT
	//	Starting up, allocating buffers.  Currently this state is unused because
	//	the setup is very fast; the constructor does not take long to run.
	//	This state will probably be removed.
	//	Not implemented.
	SONAR_INIT,
	
	//	DISABLED
	//	The SONAR system is on and initialized, but not in use.  Eventually,
	//	the SONAR system will sit in DISABLED mode while it is on the crane
	//	and while we are preparing to launch the vehicle.
	//	Not yet implemented.
	SONAR_DISABLED, 
	
	//	TRAINING
	//	The SONAR system is attempting to determine the precise timing between
	//	pings.  This enables us to predict when pings should arrive in order
	//	to reject false positives.  It is also a necessary precondition for 
	//	estimating range rate.
	//	Not yet implemented.
	SONAR_TRAINING, 
	
	//	SLEEPING
	//	The SONAR system is on and active, but not listening for pings.
	//	Generally, during SLEEP the SONAR system should be working on analyzing
	//	recently acquired data sets, doing processor intensive but not time 
	//	critical tasks.  Each SLEEP lasts for about two seconds.
	SONAR_SLEEPING, 
	
	//	LISTENING
	//	The SONAR system is on, active, and listening for pings.  It will go to
	//	sleep after a certain time limit has expired, or all hydrophones have
	//	received a ping.
	SONAR_LISTENING, 
	
	//	IDLE
	//	The SONAR system is on, active, but not receiving data from the ADC.
	//	Typically, we would enter the IDLE state if we need to lift the vehicle 
	//	out of the water temporarily or if we need to restart the course.
	//	IDLE mode tells the SONAR system to maintain its timing so that when
	//	the vehicle re-enters the water it is still TRAINed.
	//	Not yet implemented.
	SONAR_IDLE
	
} sonarstate_t;


/**
 * Describes the state of each individual channel (hydrophone).
 */
typedef enum sonarchannelstate_e {
	
	//	CHANNEL_SLEEPING
	//	The channel is neither capturing nor waiting for a rising edge.
	//	A channel is aSLEEP when the entire system is aSLEEP, or when it
	//	has already captured an entire ping.
	SONAR_CHANNEL_SLEEPING, 
	
	//	CHANNEL_LISTENING
	//	The channel is waiting for a rising edge to trigger capturing.
	SONAR_CHANNEL_LISTENING,
	
	//	CHANNEL_CAPTURING
	//	The channel is logging sound samples to memory for further analysis.
	//	The channel will continue to CAPTURE until (1) the amplitude at the
	//	frequency of interest drops below the threshold value or (2) the buffer
	//	for the current SonarChunk becomes full.  In either case, when the
	//	channel finishes CAPTUREing it goes to SLEEP.
	SONAR_CHANNEL_CAPTURING
	
} sonarchannelstate_t;


/** Collects, manages, and analyzes acoustic samples to find TDOAs.
 *  
 *  At the moment, SonarController is set up to do a single shot:
 *  it records a single set of pings and computes the TDOAs between all of them,
 *  then permanently goes to sleep.
 */
class SonarController
{
	
public:
	SonarController(int numberOfChannels);
	~SonarController();
	void receiveSample(adcdata_t*);
	adcmath_t getMag(int channel) const;
	sonarstate_t getState() const;
	sonarchannelstate_t getChannelState(int channel) const;
	void go();
	
private:
	void setstate(sonarstate_t);
	void setupCoefficients();
	void setupWindow();
	void purge();
	
	void updateSlidingDFT();
	bool listenTimeIsUp() const;
	
	void wake();
	void sleep();
	
	bool exceedsThreshold(int channel) const;
	void wakeChannel(int channel);
	void sleepChannel(int channel);
	void startCapture(int channel);
	void stopCapture(int channel);
	void captureSample(int channel);
	
	void analyzeChunks();
	
	adcmath_t *coefreal, *coefimag;
	adcmath_t **windowreal, **windowimag;
	adcmath_t *sumreal, *sumimag, *mag;
	adcdata_t *sample; 
	SonarChunk **currentChunks;
	std::vector<SonarChunk*> oldChunks;
	
	adcmath_t threshold;
	int curidx;
	int bufidx;
	int nchannels;
	int numPeriods;
	int windowlength, nearestperiod;
	int samprate;
	int targetfreq;
	int sleepingChannelCount, listeningChannelCount, captureChannelCount;
	adcsampleindex_t 
		maxSamplesTDOA, 
		minSamplesBetweenPings, 
		maxSamplesToWaitForFirstPing;
	adcsampleindex_t sampleIndex;
	sonarstate_t sonarstate;
	sonarchannelstate_t  *sonarchannelstate;
	adcsampleindex_t indexOfLastRisingEdge, indexOfLastWake;
	
};


} // namespace sonar
} // namespace ram


#endif
