#pragma once

#include <Common/CommonTypes.h>

template<typename... Args>
class DelegateBase
{
public:
    virtual ~DelegateBase() = default;
    virtual void Invoke(Args... args) = 0;
};

template<typename Instance, typename Function, typename... Args>
class OnMouseDelegate : public DelegateBase<Args...>
{
public:
    OnMouseDelegate(Instance* instance, Function func) : m_instance(instance), m_func(func) {}

    void Invoke(Args... args) override
    {
        (m_instance->*m_func)(args...);
    }
private:
    Instance* m_instance;
    Function m_func;
};

//TODO: should I just specialize this to specifically what the OnKey function should take? Or delete it and just use generic args one
template<typename Instance, typename Function, typename... Args>
class OnKeyDelegate : public DelegateBase<Args...>
{
public:
    OnKeyDelegate(Instance* instance, Function func) : m_instance(instance), m_func(func) {}

private:
    Instance* m_instance;
    Function m_func;
};

template <typename... Args>
class EventManager
{
public:

    void Subscribe(DelegateBase<Args...>* delegate)
    {
        m_delegates.push_back(delegate);
    }

    void Broadcast(Args... args)
    {
        for(DelegateBase<Args...>* delegate : m_delegates)
        {
            delegate->Invoke(args...);
        }
    }

private:
    Vector<DelegateBase<Args...>*> m_delegates;
};
