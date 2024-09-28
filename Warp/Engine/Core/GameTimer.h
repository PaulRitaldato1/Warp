#pragma once
#include <Common/CommonTypes.h>
#include <chrono>

class GameTimer
{
public:
	GameTimer();

	f64 TotalTime()const; // in seconds
	f64 DeltaTime()const; // in seconds

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.

private:
    using TimePoint = std::chrono::high_resolution_clock::time_point;

    f64 DiffTimePoints(TimePoint& left, TimePoint& right);

	f64 m_deltaTime;

	TimePoint m_baseTime;
	TimePoint m_pausedTime;
	TimePoint m_stopTime;
	TimePoint m_prevTime;
	TimePoint m_currTime;

	bool m_bStopped;
};