#pragma once

#include <Common/CommonTypes.h>

#include <algorithm>

template <typename... Args>
class DelegateBase
{
public:
    virtual ~DelegateBase() = default;
    virtual void Invoke(Args... args) = 0;
};

template <typename Instance, typename Function, typename... Args>
class MemberFuncDelegate : public DelegateBase<Args...>
{
public:
    MemberFuncDelegate(Instance* instance, Function func)
        : m_instance(instance), m_func(func) {}

    void Invoke(Args... args) override { (m_instance->*m_func)(args...); }

private:
    Instance* m_instance;
    Function m_func;
};

template <typename ClassType, typename... Args>
using MemberFuncType = MemberFuncDelegate<ClassType, void (ClassType::*)(Args...), Args...>;

// Holds raw pointers: the subscriber owns the delegate and must unsubscribe it
// before destroying it, or the next Broadcast calls into freed memory.
template <typename... Args>
class EventManager
{
public:

    void Subscribe(DelegateBase<Args...>* delegate)
    {
      m_delegates.push_back(delegate);
    }

    void Unsubscribe(DelegateBase<Args...>* delegate)
    {
      m_delegates.erase(std::remove(m_delegates.begin(), m_delegates.end(), delegate), m_delegates.end());
    }

    void Broadcast(Args... args)
    {
        // Iterates a copy so a delegate can subscribe or unsubscribe while being
        // called. Subscriber lists are a handful long, so the copy is cheap.
        const Vector<DelegateBase<Args...>*> delegates = m_delegates;
        for(DelegateBase<Args...>* delegate : delegates)
        {
          delegate->Invoke(args...);
        }
    }

private:
    Vector<DelegateBase<Args...>*> m_delegates;
};
