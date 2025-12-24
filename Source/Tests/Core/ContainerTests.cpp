#include "TestHarness.h"

#include "Container/Deque.h"
#include "Container/Queue.h"
#include "Container/Stack.h"

using namespace AltinaEngine::Core::Container;

TEST_CASE("Deque basic ops")
{
    TDeque<int> d;
    REQUIRE(d.IsEmpty());
    d.PushBack(2);
    d.PushFront(1);
    d.PushBack(3);
    REQUIRE_EQ(d.Size(), 3);
    REQUIRE_EQ(d.Front(), 1);
    REQUIRE_EQ(d.Back(), 3);
    d.PopFront();
    REQUIRE_EQ(d.Front(), 2);
    d.PopBack();
    REQUIRE_EQ(d.Back(), 2);
}

TEST_CASE("Queue basic ops")
{
    TQueue<int> q;
    REQUIRE(q.IsEmpty());
    q.Push(1);
    q.Push(2);
    REQUIRE_EQ(q.Size(), 2);
    REQUIRE_EQ(q.Front(), 1);
    q.Pop();
    REQUIRE_EQ(q.Front(), 2);
}

TEST_CASE("Stack basic ops")
{
    TStack<int> s;
    REQUIRE(s.IsEmpty());
    s.Push(1);
    s.Push(2);
    REQUIRE_EQ(s.Size(), 2);
    REQUIRE_EQ(s.Top(), 2);
    s.Pop();
    REQUIRE_EQ(s.Top(), 1);
}
