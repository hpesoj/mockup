#ifndef MOCKUP_MOCKUP_HPP
#define MOCKUP_MOCKUP_HPP

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <vector>

namespace mockup::detail
{
  struct class_instance
  {
    std::vector<std::function<void()>> destructors;
  };

  template <typename Mock>
  struct class_
  {
    static std::map<Mock const*, class_instance> instances;
  };

  template <typename Mock>
  std::map<Mock const*, class_instance> class_<Mock>::instances;

  template <typename Mock>
  class_instance& get_class_instance(Mock const* mock)
  {
    return class_<Mock>::instances[mock];
  }

  template <typename Mock>
  void destroy_class_instance(Mock const* mock)
  {
    auto& instances = class_<Mock>::instances;
    if (auto it = instances.find(mock); it != instances.end())
    {
      for (auto& destructor : it->second.destructors)
      {
        destructor();
      }
      instances.erase(it);
    }
  }

  inline std::size_t order = 0;

  template <typename... Args>
  struct invocation
  {
    std::size_t order;
    std::tuple<std::decay_t<Args>...> arguments;
  };

  template <typename>
  struct action;

  template <typename R, typename... Args>
  struct action<R(Args...)>
  {
    std::function<bool(Args...)> match;
    std::function<R(Args...)> function;

    template <typename Arguments>
    explicit action(Arguments&& arguments, std::function<R(Args...)> function)
    : match([arguments = std::forward<Arguments>(arguments)](auto const&... args) {
      return std::tie(args...) == arguments;
    })
    , function(std::move(function))
    {
    }
  };

  template <typename>
  struct member_function_instance;

  template <typename R, typename... Args>
  struct member_function_instance<R(Args...)>
  {
    std::vector<invocation<Args...>> invocations;
    std::vector<action<R(Args...)>> actions;

    R operator()(Args... args)
    {
      R result{};
      for (auto it = std::rbegin(actions); it != std::rend(actions); ++it)
      {
        if (it->match(args...))
        {
          result = it->function(args...);
          break;
        }
      }
      invocations.push_back({++order, std::make_tuple(std::forward<Args>(args)...)});
      return result;
    }
  };

  template <
      auto MemberFunction,
      typename Mock,
      typename MemberFunctionType = decltype(MemberFunction)>
  struct member_function;

  template <
      typename Mock,
      typename R,
      typename T,
      typename... Args,
      R (T::*MemberFunction)(Args...) const>
  struct member_function<MemberFunction, Mock, R (T::*)(Args...) const>
  {
    static std::map<Mock const*, member_function_instance<R(Args...)>> instances;
  };

  template <
      typename Mock,
      typename R,
      typename T,
      typename... Args,
      R (T::*MemberFunction)(Args...) const>
  std::map<Mock const*, member_function_instance<R(Args...)>>
      member_function<MemberFunction, Mock, R (T::*)(Args...) const>::instances;

  template <
      typename Mock,
      typename R,
      typename T,
      typename... Args,
      R (T::*MemberFunction)(Args...)>
  struct member_function<MemberFunction, Mock, R (T::*)(Args...)>
  {
    static std::map<Mock const*, member_function_instance<R(Args...)>> instances;
  };

  template <
      typename Mock,
      typename R,
      typename T,
      typename... Args,
      R (T::*MemberFunction)(Args...)>
  std::map<Mock const*, member_function_instance<R(Args...)>>
      member_function<MemberFunction, Mock, R (T::*)(Args...)>::instances;

  template <auto MemberFunction, typename = decltype(MemberFunction)>
  struct member_function_class_type;

  template <
      typename R,
      typename T,
      typename... Args,
      R (T::*MemberFunction)(Args...) const>
  struct member_function_class_type<MemberFunction, R (T::*)(Args...) const>
  {
    using type = T;
  };

  template <typename R, typename T, typename... Args, R (T::*MemberFunction)(Args...)>
  struct member_function_class_type<MemberFunction, R (T::*)(Args...)>
  {
    using type = T;
  };

  template <auto MemberFunction>
  using member_function_class_type_t =
      typename member_function_class_type<MemberFunction>::type;

  template <auto MemberFunction, typename Mock>
  auto& get_member_function_instance(Mock const* mock)
  {
    auto& instances =
        member_function<MemberFunction, member_function_class_type_t<MemberFunction>>::
            instances;
    auto it = instances.find(mock);
    if (it == instances.end())
    {
      it = instances
               .emplace(
                   std::piecewise_construct,
                   std::forward_as_tuple(mock),
                   std::forward_as_tuple())
               .first;
      get_class_instance(mock).destructors.push_back([&, mock]() {
        instances.erase(mock);
      });
    }
    return it->second;
  }
} // namespace mockup::detail

namespace mockup
{
  struct sequence
  {
    std::size_t order = 0;
  };

  template <typename Mock>
  class mock
  {
  private:
    Mock m_mock;

  public:
    template <typename... Args>
    explicit mock(Args&&... args)
    : m_mock(std::forward<Args>(args)...)
    {
    }

    mock(mock const&) = delete;
    mock(mock&&) = delete;

    ~mock()
    {
      detail::destroy_class_instance(&m_mock);
    }

    mock& operator=(mock const&) = delete;
    mock& operator=(mock&&) = delete;

    Mock const& operator*() const
    {
      return m_mock;
    }

    Mock& operator*()
    {
      return m_mock;
    }

    Mock const* operator->() const
    {
      return &m_mock;
    }

    Mock* operator->()
    {
      return &m_mock;
    }

    template <auto MemberFunction, typename... Args>
    auto when(Args&&... args)
    {
      auto& instance = detail::get_member_function_instance<MemberFunction>(&m_mock);
      return [&, args = std::make_tuple(std::forward<Args>(args)...)](
                 auto&& function) mutable {
        instance.actions.emplace_back(
            std::move(args), std::forward<decltype(function)>(function));
      };
    }

    template <auto MemberFunction, typename... Args>
    bool invoked(Args&&... args)
    {
      auto& instance = detail::get_member_function_instance<MemberFunction>(&m_mock);
      for (auto& invocation : instance.invocations)
      {
        if (invocation.arguments == std::tie(args...))
        {
          return true;
        }
      }
      return false;
    }

    template <auto MemberFunction, typename... Args>
    bool invoked(sequence& seq, Args&&... args)
    {
      auto& instance = detail::get_member_function_instance<MemberFunction>(&m_mock);
      auto it = std::upper_bound(
          std::begin(instance.invocations),
          std::end(instance.invocations),
          seq.order,
          [](auto order, auto const& invocation) {
            return order < invocation.order;
          });
      for (; it != std::end(instance.invocations); ++it)
      {
        if (it->arguments == std::tie(args...))
        {
          seq.order = it->order;
          return true;
        }
      }
      return false;
    }
  };

  template <auto MemberFunction, typename Mock, typename... Args>
  decltype(auto) invoke(Mock const& mock, Args&&... args)
  {
    return detail::get_member_function_instance<MemberFunction>(&mock)(
        std::forward<Args>(args)...);
  }

  namespace helpers
  {
    struct wildcard_t
    {
      explicit constexpr wildcard_t(int)
      {
      }
    };

    template <typename T>
    bool operator==(wildcard_t, T const&)
    {
      return true;
    }

    template <typename T>
    bool operator==(T const&, wildcard_t)
    {
      return true;
    }

    constexpr wildcard_t wildcard{0};
    constexpr wildcard_t _{0};

    template <typename... Rn>
    auto return_(Rn&&... rn)
    {
      std::array<std::common_type_t<Rn...>, sizeof...(Rn)> r{std::forward<Rn>(rn)...};
      return [r = std::move(r), i = std::size_t()](auto&&...) mutable {
        if (i + 1 < r.size())
        {
          return r[i++];
        }
        else
        {
          return r[i];
        }
      };
    }
  } // namespace helpers

  using namespace helpers;
} // namespace mockup

#endif // MOCKUP_MOCKUP_HPP
