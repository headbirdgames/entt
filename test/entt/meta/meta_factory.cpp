#include <array>
#include <iterator>
#include <string>
#include <utility>
#include <gtest/gtest.h>
#include <entt/core/type_info.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/range.hpp>
#include <entt/meta/resolve.hpp>
#include "../../common/boxed_type.h"
#include "../../common/config.h"
#include "../../common/meta_traits.h"

struct base {
    char member{};
};

struct clazz: base {
    clazz(int val)
        : value{val} {}

    explicit operator int() const noexcept {
        return get_int();
    }

    void set_int(int val) noexcept {
        value = val;
    }

    void set_boxed_int(test::boxed_int val) noexcept {
        value = val.value;
    }

    int get_int() const noexcept {
        return value;
    }

    static std::string to_string(const clazz &instance) {
        return std::to_string(instance.get_int());
    }

    static clazz from_string(const std::string &value) {
        return clazz{std::stoi(value)};
    }

private:
    int value{};
};

struct dtor_callback {
    dtor_callback(bool &ref)
        : cb{&ref} {}

    static void on_destroy(dtor_callback &instance) {
        *instance.cb = !*instance.cb;
    }

private:
    bool *cb;
};

struct MetaFactory: ::testing::Test {
    void TearDown() override {
        entt::meta_reset();
    }
};

using MetaFactoryDeathTest = MetaFactory;

TEST_F(MetaFactory, Constructors) {
    entt::meta_ctx ctx{};

    ASSERT_EQ(entt::resolve(entt::type_id<int>()), entt::meta_type{});
    ASSERT_EQ(entt::resolve(ctx, entt::type_id<int>()), entt::meta_type{});

    entt::meta_factory<int> factory{};

    ASSERT_NE(entt::resolve(entt::type_id<int>()), entt::meta_type{});
    ASSERT_EQ(entt::resolve(ctx, entt::type_id<int>()), entt::meta_type{});

    // this is because of entt::meta, which should be deprecated nowadays
    ASSERT_FALSE(entt::resolve(entt::type_id<int>()).is_integral());

    factory = entt::meta_factory<int>{ctx};

    ASSERT_NE(entt::resolve(entt::type_id<int>()), entt::meta_type{});
    ASSERT_NE(entt::resolve(ctx, entt::type_id<int>()), entt::meta_type{});

    // this is because of entt::meta, which should be deprecated nowadays
    ASSERT_FALSE(entt::resolve(ctx, entt::type_id<int>()).is_integral());
}

TEST_F(MetaFactory, Type) {
    using namespace entt::literals;

    auto factory = entt::meta<int>();

    ASSERT_EQ(entt::resolve("foo"_hs), entt::meta_type{});

    factory.type("foo"_hs);

    ASSERT_NE(entt::resolve("foo"_hs), entt::meta_type{});
    ASSERT_EQ(entt::resolve<int>().id(), "foo"_hs);

    factory.type("bar"_hs);

    ASSERT_EQ(entt::resolve("foo"_hs), entt::meta_type{});
    ASSERT_NE(entt::resolve("bar"_hs), entt::meta_type{});
    ASSERT_EQ(entt::resolve<int>().id(), "bar"_hs);
}

ENTT_DEBUG_TEST_F(MetaFactoryDeathTest, Type) {
    using namespace entt::literals;

    auto factory = entt::meta<int>();
    auto other = entt::meta<double>();

    factory.type("foo"_hs);

    ASSERT_DEATH(other.type("foo"_hs);, "");
}

TEST_F(MetaFactory, Base) {
    auto factory = entt::meta<clazz>();
    decltype(std::declval<entt::meta_type>().base()) range{};

    ASSERT_NE(entt::resolve(entt::type_id<clazz>()), entt::meta_type{});
    ASSERT_EQ(entt::resolve(entt::type_id<base>()), entt::meta_type{});

    range = entt::resolve<clazz>().base();

    ASSERT_EQ(range.begin(), range.end());

    factory.base<base>();
    range = entt::resolve<clazz>().base();

    ASSERT_EQ(entt::resolve(entt::type_id<base>()), entt::meta_type{});
    ASSERT_NE(range.begin(), range.end());
    ASSERT_EQ(std::distance(range.begin(), range.end()), 1);
    ASSERT_EQ(range.begin()->first, entt::type_id<base>().hash());
    ASSERT_EQ(range.begin()->second.info(), entt::type_id<base>());
}

TEST_F(MetaFactory, Conv) {
    clazz instance{3};
    auto factory = entt::meta<clazz>();
    const entt::meta_any any = entt::forward_as_meta(instance);

    ASSERT_FALSE(any.allow_cast<int>());
    ASSERT_FALSE(any.allow_cast<std::string>());

    factory.conv<int>().conv<&clazz::to_string>();

    ASSERT_TRUE(any.allow_cast<int>());
    ASSERT_TRUE(any.allow_cast<std::string>());
    ASSERT_EQ(any.allow_cast<int>().cast<int>(), instance.get_int());
    ASSERT_EQ(any.allow_cast<std::string>().cast<std::string>(), clazz::to_string(instance));
}

TEST_F(MetaFactory, Ctor) {
    const std::array values{1, 3};
    auto factory = entt::meta<clazz>();

    ASSERT_FALSE(entt::resolve<clazz>().construct(values[0u]));
    ASSERT_FALSE(entt::resolve<clazz>().construct(std::to_string(values[1u])));

    factory.ctor<int>().ctor<&clazz::from_string>();

    const auto instance = entt::resolve<clazz>().construct(values[0u]);
    const auto other = entt::resolve<clazz>().construct(std::to_string(values[1u]));

    ASSERT_TRUE(instance);
    ASSERT_TRUE(other);
    ASSERT_TRUE(instance.allow_cast<clazz>());
    ASSERT_TRUE(other.allow_cast<clazz>());
    ASSERT_EQ(instance.cast<const clazz &>().get_int(), values[0u]);
    ASSERT_EQ(other.cast<const clazz &>().get_int(), values[1u]);
}

TEST_F(MetaFactory, Dtor) {
    bool check = false;
    auto factory = entt::meta<dtor_callback>();
    entt::meta_any any{std::in_place_type<dtor_callback>, check};

    any.reset();

    ASSERT_FALSE(check);

    factory.dtor<&dtor_callback::on_destroy>();
    any.emplace<dtor_callback>(check);
    any.reset();

    ASSERT_TRUE(check);
}

TEST_F(MetaFactory, DataMemberObject) {
    using namespace entt::literals;

    base instance{'c'};
    auto factory = entt::meta<base>();
    entt::meta_type type = entt::resolve<base>();

    ASSERT_FALSE(type.data("member"_hs));

    factory.data<&base::member>("member"_hs);
    type = entt::resolve<base>();

    ASSERT_TRUE(type.data("member"_hs));
    ASSERT_EQ(type.get("member"_hs, std::as_const(instance)), instance.member);
    ASSERT_EQ(type.get("member"_hs, instance), instance.member);
    ASSERT_FALSE(type.set("member"_hs, std::as_const(instance), instance.member));
    ASSERT_TRUE(type.set("member"_hs, instance, instance.member));
}

TEST_F(MetaFactory, DataPointer) {
    using namespace entt::literals;

    auto factory = entt::meta<int>();
    entt::meta_type type = entt::resolve<int>();

    ASSERT_FALSE(type.data("value"_hs));

    static int value = 1;
    factory.data<&value>("value"_hs);
    type = entt::resolve<int>();

    ASSERT_TRUE(type.data("value"_hs));
    ASSERT_EQ(type.get("value"_hs, {}), value);
    ASSERT_TRUE(type.set("value"_hs, {}, value));
}

TEST_F(MetaFactory, DataValue) {
    using namespace entt::literals;

    constexpr int value = 1;
    auto factory = entt::meta<int>();
    entt::meta_type type = entt::resolve<int>();

    ASSERT_FALSE(type.data("value"_hs));

    factory.data<value>("value"_hs);
    type = entt::resolve<int>();

    ASSERT_TRUE(type.data("value"_hs));
    ASSERT_EQ(type.get("value"_hs, {}), value);
    ASSERT_FALSE(type.set("value"_hs, {}, value));
}

TEST_F(MetaFactory, DataGetterOnly) {
    using namespace entt::literals;

    clazz instance{1};
    auto factory = entt::meta<clazz>();
    entt::meta_type type = entt::resolve<clazz>();

    ASSERT_FALSE(type.data("value"_hs));

    factory.data<nullptr, &clazz::get_int>("value"_hs);
    type = entt::resolve<clazz>();

    ASSERT_TRUE(type.data("value"_hs));
    ASSERT_EQ(type.get("value"_hs, std::as_const(instance)), instance.get_int());
    ASSERT_EQ(type.get("value"_hs, instance), instance.get_int());
    ASSERT_FALSE(type.set("value"_hs, std::as_const(instance), instance.get_int()));
    ASSERT_FALSE(type.set("value"_hs, instance, instance.get_int()));
}

TEST_F(MetaFactory, DataSetterGetter) {
    using namespace entt::literals;

    clazz instance{1};
    auto factory = entt::meta<clazz>();
    entt::meta_type type = entt::resolve<clazz>();

    ASSERT_FALSE(type.data("value"_hs));

    factory.data<&clazz::set_int, &clazz::get_int>("value"_hs);
    type = entt::resolve<clazz>();

    ASSERT_TRUE(type.data("value"_hs));
    ASSERT_EQ(type.get("value"_hs, std::as_const(instance)), instance.get_int());
    ASSERT_EQ(type.get("value"_hs, instance), instance.get_int());
    ASSERT_FALSE(type.set("value"_hs, std::as_const(instance), instance.get_int()));
    ASSERT_TRUE(type.set("value"_hs, instance, instance.get_int()));
}

TEST_F(MetaFactory, DataMultiSetterGetter) {
    using namespace entt::literals;

    clazz instance{1};
    auto factory = entt::meta<clazz>();
    entt::meta_type type = entt::resolve<clazz>();

    ASSERT_FALSE(type.data("value"_hs));

    factory.data<entt::value_list<&clazz::set_int, &clazz::set_boxed_int>, &clazz::get_int>("value"_hs);
    type = entt::resolve<clazz>();

    ASSERT_TRUE(type.data("value"_hs));
    ASSERT_EQ(type.get("value"_hs, std::as_const(instance)), instance.get_int());
    ASSERT_EQ(type.get("value"_hs, instance), instance.get_int());
    ASSERT_FALSE(type.set("value"_hs, std::as_const(instance), instance.get_int()));
    ASSERT_TRUE(type.set("value"_hs, instance, instance.get_int()));
    ASSERT_FALSE(type.set("value"_hs, std::as_const(instance), test::boxed_int{instance.get_int()}));
    ASSERT_TRUE(type.set("value"_hs, instance, test::boxed_int{instance.get_int()}));
}

TEST_F(MetaFactory, DataOverwrite) {
    using namespace entt::literals;

    clazz instance{1};
    auto factory = entt::meta<clazz>();
    entt::meta_type type = entt::resolve<clazz>();

    ASSERT_FALSE(type.data("value"_hs));

    factory.data<nullptr, &clazz::get_int>("value"_hs);
    type = entt::resolve<clazz>();

    ASSERT_TRUE(type.data("value"_hs));
    ASSERT_TRUE(type.data("value"_hs).is_const());

    factory.data<&clazz::set_int, &clazz::get_int>("value"_hs);
    type = entt::resolve<clazz>();

    ASSERT_TRUE(type.data("value"_hs));
    ASSERT_FALSE(type.data("value"_hs).is_const());
}

TEST_F(MetaFactory, Func) {
    // TODO
}

TEST_F(MetaFactory, FuncOverwrite) {
    // TODO
}

TEST_F(MetaFactory, FuncOverload) {
    // TODO
}

TEST_F(MetaFactory, Traits) {
    // TODO
}

TEST_F(MetaFactory, Custom) {
    // TODO
}

TEST_F(MetaFactory, Meta) {
    // TODO
}

TEST_F(MetaFactory, MetaReset) {
    // TODO
}