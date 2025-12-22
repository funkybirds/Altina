#include "TestHarness.h"

#include "Container/Array.h"
#include "Container/Vector.h"
#include "Container/Span.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Container;

TEST_CASE("TSpan from C array, static extent")
{
	int data[3] = {1, 2, 3};

	using FSpan3 = TSpan<int, 3>;
	FSpan3 span(data);

	REQUIRE(!span.IsEmpty());
	REQUIRE_EQ(span.Size(), static_cast<usize>(3));
	REQUIRE_EQ(span.ExtentValue(), static_cast<usize>(3));
	REQUIRE_EQ(span.Front(), 1);
	REQUIRE_EQ(span.Back(), 3);

	span[1] = 42;
	REQUIRE_EQ(data[1], 42);
}

TEST_CASE("TSpan from C array, dynamic extent")
{
	int data[4] = {10, 20, 30, 40};

	using FSpanDyn = TSpan<int>;
	FSpanDyn span(data);

	REQUIRE(!span.IsEmpty());
	REQUIRE_EQ(span.Size(), static_cast<usize>(4));
	REQUIRE_EQ(span.ExtentValue(), static_cast<usize>(4));
	REQUIRE_EQ(span[0], 10);
	REQUIRE_EQ(span[3], 40);

	usize sum = 0;
	for (auto value : span)
	{
		sum += static_cast<usize>(value);
	}
	REQUIRE_EQ(sum, static_cast<usize>(100));
}

TEST_CASE("TSpan from TArray")
{
	TArray<int, 2> array{};
	array[0] = 7;
	array[1] = 9;

	TSpan<int, 2> spanStatic(array);
	TSpan<int>    spanDynamic(array);

	REQUIRE_EQ(spanStatic.Size(), static_cast<usize>(2));
	REQUIRE_EQ(spanDynamic.Size(), static_cast<usize>(2));
	REQUIRE_EQ(spanStatic[0], 7);
	REQUIRE_EQ(spanDynamic[1], 9);

	spanDynamic[0] = 11;
	REQUIRE_EQ(array[0], 11);
}

TEST_CASE("TSpan from TVector")
{
	TVector<int> values;
	values.PushBack(5);
	values.PushBack(6);
	values.PushBack(7);

	TSpan<int> span(values);

	REQUIRE(!span.IsEmpty());
	REQUIRE_EQ(span.Size(), static_cast<usize>(3));
	REQUIRE_EQ(span.Front(), 5);
	REQUIRE_EQ(span.Back(), 7);

	// Mutate through span and see underlying vector updated.
	span[1] = 42;
	REQUIRE_EQ(values[1], 42u - 0u); // compare as unsigned to avoid narrowing
}
