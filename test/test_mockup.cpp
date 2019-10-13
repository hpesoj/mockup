#include <mockup/mockup.hpp>

#include <catch2/catch.hpp>

struct base
{
  virtual int op(int a, int b)
  {
    return a * b;
  }

  virtual int opx(int a, int b)
  {
    return a * b;
  }

  virtual int opx(int a, int b, int c)
  {
    return a * b * c;
  }

  virtual int test(int a) const = 0;

  virtual int value() const = 0;
};

struct test_base : base
{
  int op(int a, int b) override
  {
    return mockup::invoke<&base::op>(*this, a, b);
  }

  int test(int a) const override
  {
    return mockup::invoke<&base::test>(*this, a);
  }

  int value() const override
  {
    return mockup::invoke<&base::value>(*this);
  }
};

SCENARIO("member functions can be mocked")
{
  using namespace mockup;

  GIVEN("a mocked class")
  {
    mock<test_base> tb1;

    WHEN("a function is invoked")
    {
      tb1.when<&base::value>()(return_(1, 2, 4));

      CHECK(tb1->value() == 1);
      CHECK(tb1->value() == 2);
      CHECK(tb1->value() == 4);
      CHECK(tb1->value() == 4);

      THEN("the function invocation can be checked")
      {
        CHECK(tb1.invoked<&base::value>());
      }
    }

    WHEN("a function is invoked multiple times with different arguments")
    {
      tb1.when<&base::test>(_)(return_(42));
      tb1.when<&base::test>(2)(return_(4));

      CHECK(tb1->test(1) == 42);
      CHECK(tb1->test(2) == 4);
      CHECK(tb1->value() == 0);
      CHECK(tb1->test(3) == 42);

      THEN("the function invocations can be checked")
      {
        CHECK(tb1.invoked<&base::test>(3));
        CHECK(tb1.invoked<&base::test>(1));
        CHECK(tb1.invoked<&base::test>(2));
        CHECK(tb1.invoked<&base::value>());
      }

      sequence seq;

      THEN("the function invocations can be checked in sequence")
      {
        CHECK(tb1.invoked<&base::test>(seq, 1));
        CHECK(tb1.invoked<&base::test>(seq, 2));
        CHECK_FALSE(tb1.invoked<&base::test>(seq, 1));
        CHECK(tb1.invoked<&base::value>(seq));
        CHECK_FALSE(tb1.invoked<&base::value>(seq));
        CHECK(tb1.invoked<&base::test>(seq, 3));
      }
    }
  }
}
