#include "PlotData.hh"
#include "test_util.hh"

#include <vector>

int main() {
    const std::vector<float> v{ 0.0f, 2.0f, 4.0f, 6.0f, 8.0f };
    const smg::analysis::Stats<float> s = smg::analysis::stats(v);
    CHECK(smgtest::approx(s.min, 0.0f));
    CHECK(smgtest::approx(s.max, 8.0f));
    CHECK(smgtest::approx(float(s.mean), 4.0f));
    CHECK(s.stddev > 2.8 && s.stddev < 2.9); // population std = sqrt(8) ~= 2.828

    const std::vector<float> n = smg::analysis::normalize(v);
    CHECK(n.size() == 5);
    CHECK(smgtest::approx(n.front(), 0.0f));
    CHECK(smgtest::approx(n.back(), 1.0f));
    CHECK(smgtest::approx(n[2], 0.5f));

    // constant series -> zeros, no divide-by-zero
    const std::vector<float> c{ 3.0f, 3.0f, 3.0f };
    const std::vector<float> cn = smg::analysis::normalize(c);
    CHECK(cn.size() == 3);
    CHECK(smgtest::approx(cn[0], 0.0f));
    CHECK(smgtest::approx(cn[2], 0.0f));

    // decimate caps the count and preserves endpoints
    std::vector<float> big(1000);
    for(int i = 0; i < 1000; ++i) big[std::size_t(i)] = float(i);
    const std::vector<float> d = smg::analysis::decimate(big, 100);
    CHECK(d.size() <= 100);
    CHECK(d.size() >= 2);
    CHECK(smgtest::approx(d.front(), 0.0f));
    CHECK(smgtest::approx(d.back(), 999.0f));

    // small series returned as-is
    const std::vector<float> d2 = smg::analysis::decimate(v, 100);
    CHECK(d2.size() == 5);

    // empty handling
    const std::vector<float> e;
    const smg::analysis::Stats<float> es = smg::analysis::stats(e);
    CHECK(smgtest::approx(es.min, 0.0f));
    CHECK(smgtest::approx(float(es.mean), 0.0f));

    TEST_RETURN();
}
