#include "PlotData.hh"
#include "test_util.hh"

int main() {
    smg::RollingBuffer<int> b{ 3 };
    CHECK(b.capacity() == 3);
    CHECK(b.size() == 0);

    b.push(1);
    b.push(2);
    const std::vector<int> two = b.ordered();
    CHECK(b.size() == 2);
    CHECK(two.size() == 2);
    CHECK(two[0] == 1 && two[1] == 2);

    // fill then overflow: oldest dropped, order preserved
    b.push(3);
    b.push(4); // drops 1
    b.push(5); // drops 2
    const std::vector<int> o = b.ordered();
    CHECK(b.size() == 3);
    CHECK(o.size() == 3);
    CHECK(o[0] == 3 && o[1] == 4 && o[2] == 5);

    b.clear();
    CHECK(b.size() == 0);
    CHECK(b.ordered().empty());

    TEST_RETURN();
}
