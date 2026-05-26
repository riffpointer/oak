/***  OakShared C++ Utility Tests  ***/

#include <gtest/gtest.h>
#include "olive/core/util/rational.h"
#include "olive/core/util/timerange.h"
#include "olive/core/util/stringutils.h"
#include "olive/core/util/value.h"
#include "olive/core/util/timecodefunctions.h"

using namespace olive::core;

/* ------------------------------------------------------------------ */
/*  Rational                                                          */
/* ------------------------------------------------------------------ */

class SharedRationalTest : public ::testing::Test {};

TEST_F(SharedRationalTest, DefaultIsZero) {
    rational r;
    EXPECT_EQ(r.toDouble(), 0.0);
}

TEST_F(SharedRationalTest, ConstructFromInt) {
    rational r(5);
    EXPECT_EQ(r.toDouble(), 5.0);
}

TEST_F(SharedRationalTest, ConstructFromPair) {
    rational r(3, 2);
    EXPECT_EQ(r.toDouble(), 1.5);
}

TEST_F(SharedRationalTest, Addition) {
    rational a(1, 2);
    rational b(1, 3);
    rational c = a + b;
    EXPECT_EQ(c.toDouble(), 5.0 / 6.0);
}

TEST_F(SharedRationalTest, Subtraction) {
    rational a(3, 4);
    rational b(1, 4);
    rational c = a - b;
    EXPECT_EQ(c.toDouble(), 0.5);
}

TEST_F(SharedRationalTest, Multiplication) {
    rational a(2, 3);
    rational b(3, 4);
    rational c = a * b;
    EXPECT_EQ(c.toDouble(), 0.5);
}

TEST_F(SharedRationalTest, Division) {
    rational a(3, 4);
    rational b(1, 2);
    rational c = a / b;
    EXPECT_EQ(c.toDouble(), 1.5);
}

TEST_F(SharedRationalTest, Comparison) {
    rational a(1, 2);
    rational b(2, 4);
    rational c(3, 4);
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
}

TEST_F(SharedRationalTest, FromDouble) {
    bool ok = false;
    rational r = rational::fromDouble(0.5, &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(r.toDouble(), 0.5);
}

TEST_F(SharedRationalTest, FromString) {
    bool ok = false;
    rational r = rational::fromString("3/2", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(r.toDouble(), 1.5);
}

TEST_F(SharedRationalTest, FromStringInvalid) {
    bool ok = true;
    rational r = rational::fromString("not-a-number", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(SharedRationalTest, Flipped) {
    rational r(3, 2);
    rational f = r.flipped();
    EXPECT_EQ(f.toDouble(), 2.0 / 3.0);
}

TEST_F(SharedRationalTest, NaN) {
    rational r = rational::NaN;
    EXPECT_TRUE(r.isNaN());
}

TEST_F(SharedRationalTest, NegativeDenominator) {
    rational r(1, -2);
    EXPECT_EQ(r.toDouble(), -0.5);
}

/* ------------------------------------------------------------------ */
/*  TimeRange                                                         */
/* ------------------------------------------------------------------ */

class SharedTimeRangeTest : public ::testing::Test {};

TEST_F(SharedTimeRangeTest, DefaultConstruct) {
    TimeRange tr;
    EXPECT_EQ(tr.in().toDouble(), 0.0);
    EXPECT_EQ(tr.out().toDouble(), 0.0);
}

TEST_F(SharedTimeRangeTest, ConstructInOut) {
    TimeRange tr(rational(0), rational(10));
    EXPECT_EQ(tr.in().toDouble(), 0.0);
    EXPECT_EQ(tr.out().toDouble(), 10.0);
    EXPECT_EQ(tr.length().toDouble(), 10.0);
}

TEST_F(SharedTimeRangeTest, OverlapsWith) {
    TimeRange a(rational(0), rational(10));
    TimeRange b(rational(5), rational(15));
    TimeRange c(rational(10), rational(20));
    TimeRange d(rational(11), rational(20));
    EXPECT_TRUE(a.OverlapsWith(b));
    // OverlapsWith default is inclusive, so touching ranges overlap
    EXPECT_TRUE(a.OverlapsWith(c));
    EXPECT_FALSE(a.OverlapsWith(d));
}

TEST_F(SharedTimeRangeTest, ContainsRange) {
    TimeRange a(rational(0), rational(10));
    TimeRange b(rational(2), rational(8));
    EXPECT_TRUE(a.Contains(b));
    EXPECT_FALSE(b.Contains(a));
}

TEST_F(SharedTimeRangeTest, ContainsPoint) {
    TimeRange a(rational(0), rational(10));
    EXPECT_TRUE(a.Contains(rational(5)));
    EXPECT_FALSE(a.Contains(rational(15)));
}

TEST_F(SharedTimeRangeTest, Combined) {
    TimeRange a(rational(0), rational(10));
    TimeRange b(rational(5), rational(15));
    TimeRange c = a.Combined(b);
    EXPECT_EQ(c.in().toDouble(), 0.0);
    EXPECT_EQ(c.out().toDouble(), 15.0);
}

TEST_F(SharedTimeRangeTest, Intersected) {
    TimeRange a(rational(0), rational(10));
    TimeRange b(rational(5), rational(15));
    TimeRange c = a.Intersected(b);
    EXPECT_EQ(c.in().toDouble(), 5.0);
    EXPECT_EQ(c.out().toDouble(), 10.0);
}

TEST_F(SharedTimeRangeTest, Shift) {
    TimeRange a(rational(0), rational(10));
    TimeRange b = a + rational(5);
    EXPECT_EQ(b.in().toDouble(), 5.0);
    EXPECT_EQ(b.out().toDouble(), 15.0);
}

/* ------------------------------------------------------------------ */
/*  StringUtils                                                       */
/* ------------------------------------------------------------------ */

class SharedStringUtilsTest : public ::testing::Test {};

TEST_F(SharedStringUtilsTest, SplitBasic) {
    auto parts = StringUtils::split("a,b,c", ',');
    EXPECT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST_F(SharedStringUtilsTest, SplitEmpty) {
    auto parts = StringUtils::split("", ',');
    EXPECT_EQ(parts.size(), 1u);
    EXPECT_EQ(parts[0], "");
}

TEST_F(SharedStringUtilsTest, ToIntValid) {
    bool ok = false;
    int v = StringUtils::to_int("42", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(v, 42);
}

TEST_F(SharedStringUtilsTest, ToIntInvalid) {
    bool ok = true;
    int v = StringUtils::to_int("abc", &ok);
    EXPECT_FALSE(ok);
}

TEST_F(SharedStringUtilsTest, Trim) {
    std::string s = "  hello  ";
    StringUtils::trim(s);
    EXPECT_EQ(s, "hello");
}

TEST_F(SharedStringUtilsTest, TrimCopy) {
    EXPECT_EQ(StringUtils::trimmed("  world  "), "world");
}

TEST_F(SharedStringUtilsTest, LeftPad) {
    EXPECT_EQ(StringUtils::to_string_leftpad(7, 3), "007");
}

/* ------------------------------------------------------------------ */
/*  Value                                                             */
/* ------------------------------------------------------------------ */

class SharedValueTest : public ::testing::Test {};

TEST_F(SharedValueTest, DefaultConstruct) {
    Value v;
    (void)v;
}

TEST_F(SharedValueTest, IntConstruct) {
    Value v(int64_t(42));
    (void)v;
}

TEST_F(SharedValueTest, FloatConstruct) {
    Value v(3.14);
    (void)v;
}

TEST_F(SharedValueTest, StringConstruct) {
    Value v("hello");
    (void)v;
}

TEST_F(SharedValueTest, MapGetSet) {
    ValueMap map;
    map["key"] = Value(int64_t(99));
    EXPECT_TRUE(map.find("key") != map.end());
}

/* ------------------------------------------------------------------ */
/*  Timecode                                                          */
/* ------------------------------------------------------------------ */

class SharedTimecodeTest : public ::testing::Test {};

TEST_F(SharedTimecodeTest, TimeToTimecodeSeconds) {
    rational time(5, 1);    // 5 seconds
    rational tb(1, 24);     // 24 fps
    std::string tc = Timecode::time_to_timecode(time, tb, Timecode::kTimecodeSeconds);
    EXPECT_NE(tc.find("5"), std::string::npos);
}

TEST_F(SharedTimecodeTest, TimeToTimecodeFrames) {
    rational time(12, 24);  // 12 frames at 24fps = 0.5s
    rational tb(1, 24);
    std::string tc = Timecode::time_to_timecode(time, tb, Timecode::kFrames);
    // Should contain 12 somewhere
    EXPECT_NE(tc.find("12"), std::string::npos);
}

TEST_F(SharedTimecodeTest, TimecodeToTime) {
    rational tb(1, 24);
    rational t = Timecode::timecode_to_time("00:00:01:00", tb, Timecode::kTimecodeNonDropFrame);
    // Returns time in seconds, not frames
    EXPECT_EQ(t.toDouble(), 1.0);
}
