#include "GameTimer.h"

using namespace std::chrono;

GameTimer::GameTimer()
: m_deltaTime(-1.0), m_baseTime(high_resolution_clock::now()),
  m_pausedTime(), m_prevTime(), m_currTime(), m_bStopped(false)
{
}

// Returns the total time elapsed since Reset() was called, NOT counting any
// time when the clock is stopped.
f64 GameTimer::TotalTime()const
{
	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// mStopTime - mBaseTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from mStopTime:  
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime

	if( m_bStopped )
	{
        return duration<f64>(m_stopTime - m_pausedTime).count() - duration<f64>(m_baseTime.time_since_epoch()).count();
    }

	// The distance mCurrTime - mBaseTime includes paused time,
	// which we do not want to count.  To correct this, we can subtract 
	// the paused time from mCurrTime:  
	//
	//  (mCurrTime - mPausedTime) - mBaseTime 
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mCurrTime
	else
	{
		return duration<f64>(m_currTime-m_pausedTime).count()-duration<f64>(m_baseTime.time_since_epoch()).count();
	}
}

f64 GameTimer::DeltaTime()const
{
	return m_deltaTime;
}

void GameTimer::Reset()
{
	m_baseTime = high_resolution_clock::now();
	m_prevTime = high_resolution_clock::now();
	m_stopTime = {};
	m_bStopped  = false;
}

void GameTimer::Start()
{
    TimePoint startTime = high_resolution_clock::now();


	// Accumulate the time elapsed between stop and start pairs.
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  mBaseTime       mStopTime        startTime     

	if( m_bStopped )
	{
		m_pausedTime += (startTime - m_stopTime);

		m_prevTime = startTime;
		m_stopTime = {};
		m_bStopped  = false;
	}
}

void GameTimer::Stop()
{
	if( !m_bStopped )
	{
		TimePoint currTime = high_resolution_clock::now();

		m_stopTime = currTime;
		m_bStopped  = true;
	}
}

void GameTimer::Tick()
{
	if( m_bStopped )
	{
		m_deltaTime = 0.0;
		return;
	}

	TimePoint currTime = high_resolution_clock::now();
	m_currTime = currTime;

	// Time difference between this frame and the previous.
	m_deltaTime = DiffTimePoints(m_currTime, m_prevTime);

	// Prepare for next frame.
	m_prevTime = m_currTime;
 
	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then mDeltaTime can be negative.
	if(m_deltaTime < 0.0)
	{
		m_deltaTime = 0.0;
	}
}

f64 GameTimer::DiffTimePoints(TimePoint& left, TimePoint& right)
{
    return duration<f64>(left-right).count();
}
