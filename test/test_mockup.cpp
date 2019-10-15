#include <mockup/mockup.hpp>

#include <catch2/catch.hpp>

#include <memory>
#include <string>

struct not_default_constructible
{
  int value;

  explicit not_default_constructible(int v)
  : value(v)
  {
  }
};

struct base
{
  virtual int op(int a) = 0;
  virtual int op(int a, int b) = 0;
  virtual int test(int a) const = 0;
  virtual int value() const = 0;
  virtual not_default_constructible make() const = 0;
  virtual std::string const& name() const = 0;
  virtual std::string& name() = 0;
  virtual void take(std::unique_ptr<int> const& p) = 0;
  virtual void take(std::unique_ptr<int>&& p) = 0;
  virtual void take2(std::unique_ptr<int> p) = 0;
};

using namespace mockup;

struct test_base : base
{
  int op(int a) override
  {
    return invoke<overload<int(int)>(&test_base::op)>(*this, a);
  }

  int op(int a, int b) override
  {
    return invoke<overload<int(int, int)>(&test_base::op)>(*this, a, b);
  }

  int test(int a) const override
  {
    return invoke<&test_base::test>(*this, a);
  }

  int value() const override
  {
    return invoke<&test_base::value>(*this);
  }

  not_default_constructible make() const override
  {
    return invoke<&test_base::make>(*this);
  }

  std::string const& name() const override
  {
    return invoke<const_(&test_base::name)>(*this);
  }

  std::string& name() override
  {
    return invoke<non_const(&test_base::name)>(*this);
  }

  void take(std::unique_ptr<int> const& p) override;
  void take(std::unique_ptr<int>&& p) override;

  void take2(std::unique_ptr<int> p) override
  {
    // return invoke<&test_base::take2>(*this, std::move(p));
  }
};

constexpr auto take_rvalue = overload<void(std::unique_ptr<int>&&)>(&test_base::take);
constexpr auto take_lvalue =
    overload<void(std::unique_ptr<int> const&)>(&test_base::take);

inline void test_base::take(std::unique_ptr<int> const& p)
{
  invoke<take_lvalue>(*this, ref(p));
}

inline void test_base::take(std::unique_ptr<int>&& p)
{
  invoke<take_rvalue>(*this, ref(std::move(p)));
}

SCENARIO("member functions can be mocked")
{
  GIVEN("a mocked class")
  {
    mock<test_base> tb1;

    WHEN("a function is invoked")
    {
      tb1.when<&test_base::value>()(return_(1, 2, 4));

      CHECK(tb1->value() == 1);
      CHECK(tb1->value() == 2);
      CHECK(tb1->value() == 4);
      CHECK(tb1->value() == 4);

      THEN("the function invocation can be checked")
      {
        CHECK(tb1.invoked<&test_base::value>());
      }
    }

    WHEN("a function is invoked multiple times with different arguments")
    {
      tb1.when<&test_base::test>(_)(return_(42));
      tb1.when<&test_base::test>(2)(return_(4));

      CHECK(tb1->test(1) == 42);
      CHECK(tb1->test(2) == 4);
      CHECK(tb1->value() == 0);
      CHECK(tb1->test(3) == 42);

      THEN("the function invocations can be checked")
      {
        CHECK(tb1.invoked<&test_base::test>(3));
        CHECK(tb1.invoked<&test_base::test>(1));
        CHECK(tb1.invoked<&test_base::test>(2));
        CHECK(tb1.invoked<&test_base::test>(_));
        CHECK(tb1.invoked<&test_base::value>());
      }

      sequence seq;

      THEN("the function invocations can be checked in sequence")
      {
        CHECK(tb1.invoked<&test_base::test>(seq, 1));
        CHECK(tb1.invoked<&test_base::test>(seq, 2));
        CHECK_FALSE(tb1.invoked<&test_base::test>(seq, 1));
        CHECK(tb1.invoked<&test_base::value>(seq));
        CHECK_FALSE(tb1.invoked<&test_base::value>(seq));
        CHECK(tb1.invoked<&test_base::test>(seq, 3));
      }

      THEN("the function invocations can be checked using predicates")
      {
        CHECK_FALSE(tb1.invoked<&test_base::test>(equal_to(0)));
        CHECK(tb1.invoked<&test_base::test>(equal_to(1)));
        CHECK(tb1.invoked<&test_base::test>(equal_to(2)));
        CHECK(tb1.invoked<&test_base::test>(equal_to(3)));
        CHECK_FALSE(tb1.invoked<&test_base::test>(equal_to(4)));

        CHECK(tb1.invoked<&test_base::test>(not_equal_to(0)));
        CHECK(tb1.invoked<&test_base::test>(not_equal_to(1)));
        CHECK(tb1.invoked<&test_base::test>(not_equal_to(2)));
        CHECK(tb1.invoked<&test_base::test>(not_equal_to(3)));
        CHECK(tb1.invoked<&test_base::test>(not_equal_to(4)));

        CHECK_FALSE(tb1.invoked<&test_base::test>(less_than(0)));
        CHECK_FALSE(tb1.invoked<&test_base::test>(less_than(1)));
        CHECK(tb1.invoked<&test_base::test>(less_than(2)));
        CHECK(tb1.invoked<&test_base::test>(less_than(3)));
        CHECK(tb1.invoked<&test_base::test>(less_than(4)));

        CHECK_FALSE(tb1.invoked<&test_base::test>(less_than_or_equal_to(0)));
        CHECK(tb1.invoked<&test_base::test>(less_than_or_equal_to(1)));
        CHECK(tb1.invoked<&test_base::test>(less_than_or_equal_to(2)));
        CHECK(tb1.invoked<&test_base::test>(less_than_or_equal_to(3)));
        CHECK(tb1.invoked<&test_base::test>(less_than_or_equal_to(4)));

        CHECK(tb1.invoked<&test_base::test>(greater_than(0)));
        CHECK(tb1.invoked<&test_base::test>(greater_than(1)));
        CHECK(tb1.invoked<&test_base::test>(greater_than(2)));
        CHECK_FALSE(tb1.invoked<&test_base::test>(greater_than(3)));
        CHECK_FALSE(tb1.invoked<&test_base::test>(greater_than(4)));

        CHECK(tb1.invoked<&test_base::test>(greater_than_or_equal_to(0)));
        CHECK(tb1.invoked<&test_base::test>(greater_than_or_equal_to(1)));
        CHECK(tb1.invoked<&test_base::test>(greater_than_or_equal_to(2)));
        CHECK(tb1.invoked<&test_base::test>(greater_than_or_equal_to(3)));
        CHECK_FALSE(tb1.invoked<&test_base::test>(greater_than_or_equal_to(4)));
      }

      THEN("the function invocations can be checked using custom predicates")
      {
        auto less_than_4 = predicate([](auto x) {
          return x < 4;
        });

        auto greater_than_3 = predicate([](auto x) {
          return x > 3;
        });

        CHECK(tb1.invoked<&test_base::test>(less_than_4));
        CHECK_FALSE(tb1.invoked<&test_base::test>(greater_than_3));
      }
    }

    WHEN("calling a function that returns a non-default constructible value")
    {
      AND_WHEN("no action is registered")
      {
        THEN("the function invocation throws")
        {
          CHECK_THROWS(tb1->make());
        }
      }

      AND_WHEN("an action is registered")
      {
        tb1.when<&test_base::make>()(return_(not_default_constructible(42)));

        CHECK(tb1->make().value == 42);
      }
    }

    WHEN("calling a function that returns a reference to const")
    {
      THEN("the function return a default value that can be modified")
      {
        CHECK(tb1->name() == "");
        tb1->name() = "hello, world";
        CHECK(tb1->name() == "hello, world");
      }
    }

    WHEN("calling a function that returns a reference to non-const")
    {
      THEN("the function return a default value")
      {
        CHECK(static_cast<mock<test_base> const&>(tb1)->name() == "");
      }
    }

    WHEN("calling a function that takes an lvalue reference")
    {
      THEN("the function can successfully record it by reference")
      {
        auto p = std::make_unique<int>(42);
        tb1->take(p);
        CHECK(tb1.invoked<take_lvalue>(p));
      }
    }

    WHEN("calling a function that takes an rvalue reference")
    {
      THEN("the function can successfully record it by reference")
      {
        auto p = std::make_unique<int>(42);
        tb1->take(std::move(p));
        CHECK(tb1.invoked<take_rvalue>(p));
      }
    }

    WHEN("an action throws an exception")
    {
      tb1.when<&test_base::test>(42)(throw_(std::runtime_error("poop")));

      THEN("the function invocation throws")
      {
        CHECK_THROWS(tb1->test(42));
        CHECK(tb1->test(43) == 0);
      }
    }
  }
}
