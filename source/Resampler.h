#pragma once
// Streaming stereo windowed-sinc resampler. Used twice: host rate -> 44.1 kHz in front of the engine and
// 44.1 kHz -> host rate behind it, so the engine (tuned for 44.1 kHz) sounds the same at any host rate.
//
// Time mapping: output sample j is taken at input position j * step (step = inRate / outRate), so the
// resampler itself adds no time offset; it just has to wait for `half` future input samples.
#include <cmath>
#include <vector>
#include <algorithm>

namespace dub
{
class StreamResampler
{
  public:
    void prepare(double inRate, double outRate, int maxInputBlock)
    {
        step_   = inRate / outRate;
        cutoff_ = std::min(1.0, outRate / inRate) * 0.91; // fraction of the input Nyquist
        half_   = (int)std::ceil(14.0 / cutoff_);         // wider kernel when band-limiting harder
        buildTable();
        const size_t cap = (size_t)(maxInputBlock + 4 * half_ + 64);
        for(auto &b : buf_)
            b.assign(cap, 0.f);
        reset();
    }

    void reset()
    {
        for(auto &b : buf_)
            std::fill(b.begin(), b.end(), 0.f);
        len_ = half_;          // `half` zeros of history
        pos_ = (double)half_;  // time 0 = first real input sample
    }

    int halfWidth() const { return half_; }

    // append input frames
    void push(const float *l, const float *r, int n)
    {
        ensure(len_ + n);
        std::copy(l, l + n, buf_[0].begin() + len_);
        std::copy(r, r + n, buf_[1].begin() + len_);
        len_ += n;
    }

    // produce as many output frames as the buffered input allows (at most maxOut)
    int pull(float *l, float *r, int maxOut)
    {
        int produced = 0;
        while(produced < maxOut)
        {
            const int ip = (int)std::floor(pos_);
            if(ip + half_ >= len_)
                break;
            const double frac = pos_ - ip;
            float        sl = 0.f, sr = 0.f;
            const float *bl = buf_[0].data();
            const float *br = buf_[1].data();
            for(int k = ip - half_ + 1; k <= ip + half_; k++)
            {
                const float h = kernel((double)(k - ip) - frac);
                sl += bl[k] * h;
                sr += br[k] * h;
            }
            l[produced] = sl;
            r[produced] = sr;
            produced++;
            pos_ += step_;
        }
        compact();
        return produced;
    }

  private:
    static constexpr int kOversample = 512;

    void buildTable()
    {
        const int n = half_ * kOversample + 2;
        table_.assign((size_t)n, 0.f);
        for(int i = 0; i < n; i++)
        {
            const double x = (double)i / kOversample; // distance in input samples
            double       w = 0.0;
            if(x < half_)
            {
                // Blackman-Harris window over [-half, half]
                const double t = 0.5 + 0.5 * x / half_;
                const double a = 2.0 * M_PI * t;
                w = 0.35875 - 0.48829 * std::cos(a) + 0.14128 * std::cos(2 * a) - 0.01168 * std::cos(3 * a);
            }
            const double cx = cutoff_ * x;
            const double s  = cx < 1e-9 ? 1.0 : std::sin(M_PI * cx) / (M_PI * cx);
            table_[(size_t)i] = (float)(cutoff_ * s * w);
        }
    }

    float kernel(double d) const
    {
        const double ax  = std::fabs(d) * kOversample;
        const int    i   = (int)ax;
        if(i >= (int)table_.size() - 1)
            return 0.f;
        const float  f = (float)(ax - i);
        return table_[(size_t)i] + (table_[(size_t)i + 1] - table_[(size_t)i]) * f;
    }

    void ensure(int needed)
    {
        if((size_t)needed > buf_[0].size())
            for(auto &b : buf_)
                b.resize((size_t)needed * 2, 0.f); // only if the host exceeds the announced block size
    }

    void compact()
    {
        const int drop = (int)std::floor(pos_) - half_;
        if(drop <= 0)
            return;
        for(auto &b : buf_)
            std::copy(b.begin() + drop, b.begin() + len_, b.begin());
        len_ -= drop;
        pos_ -= drop;
    }

    double              step_ = 1.0, cutoff_ = 0.91, pos_ = 0.0;
    int                 half_ = 16, len_ = 0;
    std::vector<float>  buf_[2];
    std::vector<float>  table_;
};
} // namespace dub
