// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/schema_utils.h"

#include <memory>
#include <string>
#include <vector>

#include <base/values.h>
#include <chromeos/variant_dictionary.h>
#include <gtest/gtest.h>

#include "buffet/commands/object_schema.h"
#include "buffet/commands/prop_types.h"
#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_constants.h"
#include "buffet/commands/unittest_utils.h"

namespace buffet {

using unittests::CreateDictionaryValue;
using unittests::CreateValue;
using chromeos::VariantDictionary;

TEST(CommandSchemaUtils, TypedValueToJson_Scalar) {
  EXPECT_JSON_EQ("true", *TypedValueToJson(true, nullptr));
  EXPECT_JSON_EQ("false", *TypedValueToJson(false, nullptr));

  EXPECT_JSON_EQ("0", *TypedValueToJson(0, nullptr));
  EXPECT_JSON_EQ("-10", *TypedValueToJson(-10, nullptr));
  EXPECT_JSON_EQ("20", *TypedValueToJson(20, nullptr));

  EXPECT_JSON_EQ("0.0", *TypedValueToJson(0.0, nullptr));
  EXPECT_JSON_EQ("1.2", *TypedValueToJson(1.2, nullptr));

  EXPECT_JSON_EQ("'abc'", *TypedValueToJson(std::string("abc"), nullptr));

  std::vector<bool> bool_array{true, false};
  EXPECT_JSON_EQ("[true,false]", *TypedValueToJson(bool_array, nullptr));

  std::vector<int> int_array{1, 2, 5};
  EXPECT_JSON_EQ("[1,2,5]", *TypedValueToJson(int_array, nullptr));

  std::vector<double> dbl_array{1.1, 2.2};
  EXPECT_JSON_EQ("[1.1,2.2]", *TypedValueToJson(dbl_array, nullptr));

  std::vector<std::string> str_array{"a", "bc"};
  EXPECT_JSON_EQ("['a','bc']", *TypedValueToJson(str_array, nullptr));
}

TEST(CommandSchemaUtils, TypedValueToJson_Object) {
  IntPropType int_type;
  native_types::Object object;

  object.insert(std::make_pair("width", int_type.CreateValue(640, nullptr)));
  object.insert(std::make_pair("height", int_type.CreateValue(480, nullptr)));
  EXPECT_JSON_EQ("{'height':480,'width':640}",
                 *TypedValueToJson(object, nullptr));
}

TEST(CommandSchemaUtils, TypedValueToJson_Array) {
  IntPropType int_type;
  native_types::Array arr;

  arr.push_back(int_type.CreateValue(640, nullptr));
  arr.push_back(int_type.CreateValue(480, nullptr));
  EXPECT_JSON_EQ("[640,480]", *TypedValueToJson(arr, nullptr));
}

TEST(CommandSchemaUtils, TypedValueFromJson_Bool) {
  bool value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("true").get(), nullptr, &value, nullptr));
  EXPECT_TRUE(value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("false").get(), nullptr, &value, nullptr));
  EXPECT_FALSE(value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("0").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Int) {
  int value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("0").get(), nullptr, &value, nullptr));
  EXPECT_EQ(0, value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("23").get(), nullptr, &value, nullptr));
  EXPECT_EQ(23, value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("-1234").get(), nullptr, &value, nullptr));
  EXPECT_EQ(-1234, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Double) {
  double value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("0").get(), nullptr, &value, nullptr));
  EXPECT_DOUBLE_EQ(0.0, value);
  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("0.0").get(), nullptr, &value, nullptr));
  EXPECT_DOUBLE_EQ(0.0, value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("23").get(), nullptr, &value, nullptr));
  EXPECT_EQ(23.0, value);
  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("23.1").get(), nullptr, &value, nullptr));
  EXPECT_EQ(23.1, value);

  EXPECT_TRUE(TypedValueFromJson(CreateValue("-1.23E+02").get(), nullptr,
                                 &value, nullptr));
  EXPECT_EQ(-123.0, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_String) {
  std::string value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("''").get(), nullptr, &value, nullptr));
  EXPECT_EQ("", value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("'23'").get(), nullptr, &value, nullptr));
  EXPECT_EQ("23", value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, nullptr));
  EXPECT_EQ("abc", value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("12").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Object) {
  native_types::Object value;
  std::unique_ptr<ObjectSchema> schema{new ObjectSchema};

  IntPropType age_prop;
  age_prop.AddMinMaxConstraint(0, 150);
  schema->AddProp("age", age_prop.Clone());

  StringPropType name_prop;
  name_prop.AddLengthConstraint(1, 30);
  schema->AddProp("name", name_prop.Clone());

  ObjectPropType type;
  type.SetObjectSchema(std::move(schema));
  EXPECT_TRUE(TypedValueFromJson(CreateValue("{'age':20,'name':'Bob'}").get(),
                                 &type, &value, nullptr));
  native_types::Object value2;
  value2.insert(std::make_pair("age", age_prop.CreateValue(20, nullptr)));
  value2.insert(std::make_pair("name",
                               name_prop.CreateValue(std::string("Bob"),
                                                     nullptr)));
  EXPECT_EQ(value2, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Array) {
  native_types::Array arr;
  StringPropType str_type;
  str_type.AddLengthConstraint(3, 100);
  ArrayPropType type;
  type.SetItemType(str_type.Clone());

  EXPECT_TRUE(TypedValueFromJson(CreateValue("['foo', 'bar']").get(), &type,
                                 &arr, nullptr));
  native_types::Array arr2;
  arr2.push_back(str_type.CreateValue(std::string{"foo"}, nullptr));
  arr2.push_back(str_type.CreateValue(std::string{"bar"}, nullptr));
  EXPECT_EQ(arr2, arr);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(TypedValueFromJson(CreateValue("['baz', 'ab']").get(), &type,
                                  &arr, &error));
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, PropValueToDBusVariant) {
  IntPropType int_type;
  auto prop_value = int_type.CreateValue(5, nullptr);
  EXPECT_EQ(5, PropValueToDBusVariant(prop_value.get()).Get<int>());

  BooleanPropType bool_type;
  prop_value = bool_type.CreateValue(true, nullptr);
  EXPECT_TRUE(PropValueToDBusVariant(prop_value.get()).Get<bool>());

  DoublePropType dbl_type;
  prop_value = dbl_type.CreateValue(5.5, nullptr);
  EXPECT_DOUBLE_EQ(5.5, PropValueToDBusVariant(prop_value.get()).Get<double>());

  StringPropType str_type;
  prop_value = str_type.CreateValue(std::string{"foo"}, nullptr);
  EXPECT_EQ("foo", PropValueToDBusVariant(prop_value.get()).Get<std::string>());

  ObjectPropType obj_type;
  ASSERT_TRUE(obj_type.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'},"
      "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}").get(),
      nullptr, nullptr));
  native_types::Object obj{
      {"width", int_type.CreateValue(10, nullptr)},
      {"height", int_type.CreateValue(20, nullptr)},
  };
  prop_value = obj_type.CreateValue(obj, nullptr);
  VariantDictionary dict =
      PropValueToDBusVariant(prop_value.get()).Get<VariantDictionary>();
  EXPECT_EQ(20, dict["height"].Get<int>());
  EXPECT_EQ(10, dict["width"].Get<int>());

  ArrayPropType arr_type;
  arr_type.SetItemType(str_type.Clone());
  native_types::Array arr;
  arr.push_back(str_type.CreateValue(std::string{"foo"}, nullptr));
  arr.push_back(str_type.CreateValue(std::string{"bar"}, nullptr));
  arr.push_back(str_type.CreateValue(std::string{"baz"}, nullptr));
  prop_value = arr_type.CreateValue(arr, nullptr);
  chromeos::Any any = PropValueToDBusVariant(prop_value.get());
  ASSERT_TRUE(any.IsTypeCompatible<std::vector<std::string>>());
  EXPECT_EQ((std::vector<std::string>{"foo", "bar", "baz"}),
            any.Get<std::vector<std::string>>());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Int) {
  IntPropType int_type;
  ASSERT_TRUE(int_type.FromJson(CreateDictionaryValue("{'enum':[1,2]}").get(),
                                nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&int_type, 1, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_EQ(1, prop_value->GetValueAsAny().Get<int>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&int_type, 5, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Bool) {
  BooleanPropType bool_type;
  ASSERT_TRUE(bool_type.FromJson(CreateDictionaryValue("{'enum':[true]}").get(),
                                 nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&bool_type, true, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_TRUE(prop_value->GetValueAsAny().Get<bool>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&bool_type, false, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Double) {
  DoublePropType dbl_type;
  ASSERT_TRUE(dbl_type.FromJson(CreateDictionaryValue("{'maximum':2.0}").get(),
                                 nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&dbl_type, 1.0, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_DOUBLE_EQ(1.0, prop_value->GetValueAsAny().Get<double>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&dbl_type, 10.0, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_String) {
  StringPropType str_type;
  ASSERT_TRUE(str_type.FromJson(CreateDictionaryValue("{'minLength': 4}").get(),
                                 nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&str_type, std::string{"blah"},
                                             nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_EQ("blah", prop_value->GetValueAsAny().Get<std::string>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&str_type, std::string{"foo"}, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Object) {
  ObjectPropType obj_type;
  ASSERT_TRUE(obj_type.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'},"
      "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}").get(),
      nullptr, nullptr));

  VariantDictionary obj{
    {"width", 100},
    {"height", 200},
  };
  auto prop_value = PropValueFromDBusVariant(&obj_type, obj, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  auto value = prop_value->GetValueAsAny().Get<native_types::Object>();
  EXPECT_EQ(100, value["width"].get()->GetValueAsAny().Get<int>());
  EXPECT_EQ(200, value["height"].get()->GetValueAsAny().Get<int>());

  chromeos::ErrorPtr error;
  obj["height"] = 20;
  prop_value = PropValueFromDBusVariant(&obj_type, obj, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Array) {
  ArrayPropType arr_type;
  IntPropType int_type;
  int_type.AddMinMaxConstraint(0, 100);
  arr_type.SetItemType(int_type.Clone());
  std::vector<int> data{0, 1, 1, 100};
  auto prop_value = PropValueFromDBusVariant(&arr_type, data, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  auto arr = prop_value->GetValueAsAny().Get<native_types::Array>();
  ASSERT_EQ(4u, arr.size());
  EXPECT_EQ(0, arr[0]->GetInt()->GetValue());
  EXPECT_EQ(1, arr[1]->GetInt()->GetValue());
  EXPECT_EQ(1, arr[2]->GetInt()->GetValue());
  EXPECT_EQ(100, arr[3]->GetInt()->GetValue());

  chromeos::ErrorPtr error;
  data.push_back(-1);  // This value is out of bounds for |int_type|.
  prop_value = PropValueFromDBusVariant(&arr_type, data, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

}  // namespace buffet