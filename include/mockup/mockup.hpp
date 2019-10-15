#ifndef MOCKUP_MOCKUP_HPP
#define MOCKUP_MOCKUP_HPP

#include <array>
#include <atomic>
#include <functional>
#include <map>
#include <tuple>
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
    std::function<bool(std::function<bool(std::decay_t<Args> const&...)> const&)> match;
    std::size_t order;

    template <typename Arguments>
    explicit invocation(Arguments&& args, std::size_t order)
    : match([args = std::forward<Arguments>(args)](
                std::function<bool(std::decay_t<Args> const&...)> const& match) {
      return std::apply(match, args);
    })
    , order(order)
    {
    }
  };

  template <typename>
  struct action;

  template <typename R, typename... Args>
  struct action<R(Args...)>
  {
    std::function<bool(std::decay_t<Args> const&...)> match;
    std::function<R(Args...)> function;

    template <typename Arguments>
    explicit action(Arguments&& arguments, std::function<R(Args...)> function)
    : match([arguments =
                 std::forward<Arguments>(arguments)](std::decay_t<Args> const&... args) {
      return arguments == std::tie(args...);
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

    member_function_instance()
    {
      if constexpr (std::is_default_constructible_v<std::decay_t<R>>)
      {
        actions.emplace_back(wildcard, [r = std::decay_t<R>()](Args...) mutable -> R {
          return std::forward<R>(r);
        });
      }
    }

    template <typename... FuncArgs>
    R operator()(FuncArgs&&... args)
    {
      invocations.push_back(invocation<Args...>{std::make_tuple(args...), ++order});
      for (auto it = std::rbegin(actions); it != std::rend(actions); ++it)
      {
        if (it->match(args...))
        {
          return it->function(std::forward<FuncArgs>(args)...);
        }
      }
      if constexpr (!std::is_void_v<R>)
      {
        throw std::runtime_error(
            "no action registered for member function with non-default constructible "
            "return type");
      }
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

  struct converts_to_any
  {
    template <typename T>
    operator T()
    {
      return {};
    }
  };

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
        static_assert(
            std::is_invocable_v<decltype(function), Args&&...>,
            "function object cannot be called with required arguments");
        instance.actions.emplace_back(
            std::move(args), std::forward<decltype(function)>(function));
      };
    }

    template <auto MemberFunction, typename... Args>
    bool invoked(Args const&... args)
    {
      auto& instance = detail::get_member_function_instance<MemberFunction>(&m_mock);
      for (auto& invocation : instance.invocations)
      {
        if (invocation.match([expected = std::tie(args...)](auto const&... args) {
              return expected == std::tie(args...);
            }))
        {
          return true;
        }
      }
      return false;
    }

    template <auto MemberFunction, typename... Args>
    bool invoked(sequence& seq, Args const&... args)
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
        if (it->match([expected = std::tie(args...)](Args const&... args) {
              return expected == std::tie(args...);
            }))
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
    static_assert(
        (... && std::is_copy_constructible_v<std::decay_t<Args>>),
        "all arguments must be copyable");
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

    template <typename T>
    bool operator!=(wildcard_t, T const&)
    {
      return false;
    }

    template <typename T>
    bool operator!=(T const&, wildcard_t)
    {
      return false;
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

    template <typename... En>
    auto throw_(En&&... en)
    {
      std::array<std::common_type_t<En...>, sizeof...(En)> r{std::forward<En>(en)...};
      return [r = std::move(r),
              i = std::size_t()](auto&&...) mutable -> detail::converts_to_any {
        if (i + 1 < r.size())
        {
          throw r[i++];
        }
        else
        {
          throw r[i];
        }
      };
    }

    template <typename T>
    class reference
    {
    private:
      std::decay_t<T>* m_value;

    public:
      explicit reference(T value)
      : m_value(&value)
      {
      }

      operator T const&() const&
      {
        return *m_value;
      }

      operator T&() &
      {
        return *m_value;
      }

      operator T const &&() const&&
      {
        return std::move(*m_value);
      }

      operator T &&() &&
      {
        return std::move(*m_value);
      }

      bool operator==(std::decay_t<T> const& other) const
      {
        return *m_value == other;
      }
    };

    template <typename T>
    auto ref(T&& t)
    {
      return reference<T&&>(std::forward<T>(t));
    };
  } // namespace helpers

  using namespace helpers;

  template <typename>
  struct overload_t;

  template <typename R, typename... Args>
  struct overload_t<R(Args...)>
  {
    explicit constexpr overload_t(int)
    {
    }

    template <typename T>
    constexpr auto operator()(R (T::*m)(Args...)) const
    {
      return m;
    }
  };

  template <typename R, typename... Args>
  struct overload_t<R(Args...) const>
  {
    explicit constexpr overload_t(int)
    {
    }

    template <typename T>
    constexpr auto operator()(R (T::*m)(Args...) const) const
    {
      return m;
    }
  };

  template <typename F>
  inline constexpr overload_t<F> overload{0};

  template <typename... Args, typename R, typename T>
  constexpr auto const_(R (T::*m)(Args...) const)
  {
    return m;
  }

  template <typename... Args, typename R, typename T>
  constexpr auto non_const(R (T::*m)(Args...))
  {
    return m;
  }

  template <typename P>
  class predicate_t
  {
  private:
    P m_predicate;

  public:
    template <typename F>
    explicit predicate_t(F&& f)
    : m_predicate(std::forward<F>(f))
    {
    }

    template <typename T>
    bool operator==(T const& t) const
    {
      return m_predicate(t);
    }
  };

  template <typename P>
  auto predicate(P&& p)
  {
    return predicate_t<std::decay_t<P>>(std::forward<P>(p));
  }

  template <typename T>
  auto equal_to(T&& t)
  {
    return predicate([t = std::forward<T>(t)](auto const& x) {
      return x == t;
    });
  }

  template <typename T>
  auto not_equal_to(T&& t)
  {
    return predicate([t = std::forward<T>(t)](auto const& x) {
      return x != t;
    });
  }

  template <typename T>
  auto less_than(T&& t)
  {
    return predicate([t = std::forward<T>(t)](auto const& x) {
      return x < t;
    });
  }

  template <typename T>
  auto less_than_or_equal_to(T&& t)
  {
    return predicate([t = std::forward<T>(t)](auto const& x) {
      return x <= t;
    });
  }

  template <typename T>
  auto greater_than(T&& t)
  {
    return predicate([t = std::forward<T>(t)](auto const& x) {
      return x > t;
    });
  }

  template <typename T>
  auto greater_than_or_equal_to(T&& t)
  {
    return predicate([t = std::forward<T>(t)](auto const& x) {
      return x >= t;
    });
  }
} // namespace mockup

#endif // MOCKUP_MOCKUP_HPP
