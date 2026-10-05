#pragma once

#include <Common/CommonTypes.h>

#include <algorithm>
#include <functional>

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

// Free functions and lambdas, captures included. Costs a std::function call over
// a member delegate's direct call, which only matters in a hot loop.
//   FunctionDelegate<bool> onVSync([this](bool bEnabled) { RecreateSwapChain(); });
template <typename... Args>
class FunctionDelegate : public DelegateBase<Args...>
{
public:
    explicit FunctionDelegate(std::function<void(Args...)> func)
        : m_func(std::move(func)) {}

    void Invoke(Args... args) override { m_func(args...); }

private:
    std::function<void(Args...)> m_func;
};

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

    bool HasSubscribers() const
    {
      return !m_delegates.empty();
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
