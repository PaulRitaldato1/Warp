#pragma once

class IRenderer
{
public:
	IRenderer()	 = default;
	~IRenderer() = default;

	virtual void Init()		  = 0;
	virtual void Shutdown()	  = 0;
	virtual void BeginFrame() = 0;
	virtual void EndFrame()	  = 0;
	virtual void Draw()		  = 0;
};