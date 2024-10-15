#pragma once

#include <Common/CommonTypes.h>

template <typename... Args> class DelegateBase {
public:
  virtual ~DelegateBase() = default;
  virtual void Invoke(Args... args) = 0;
};

template <typename Instance, typename Function, typename... Args>
class MemberFuncDelegate : public DelegateBase<Args...> {
public:
  MemberFuncDelegate(Instance *instance, Function func)
      : m_instance(instance), m_func(func) {}

  void Invoke(Args... args) override { (m_instance->*m_func)(args...); }

private:
  Instance *m_instance;
  Function m_func;
};

template <typename ClassType, typename... Args>
using MemberFuncType = MemberFuncDelegate<ClassType, void (ClassType::*)(Args...), Args...>;

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
