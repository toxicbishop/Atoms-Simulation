#ifndef ORBITAL_MATH_H
#define ORBITAL_MATH_H

// =============================================================================
// orbital_math.h — Hydrogen Atom Orbital Sampling & Visualization Utilities
// =============================================================================
//
// Shared math for sampling the hydrogen atom wavefunction ψ_{n,l,m}(r,θ,φ).
//
// The wavefunction separates as:
//   ψ_{n,l,m}(r,θ,φ) = R_{nl}(r) · Y_l^m(θ,φ)
//
// where R_{nl} is the radial wavefunction (Associated Laguerre polynomials)
// and Y_l^m are the spherical harmonics (Associated Legendre polynomials).
//
// Sampling approach: CDF-inversion — we precompute the cumulative distribution
// for r and θ independently (φ is uniform for |ψ|² of real orbitals), then
// draw random samples by inverting the CDF via binary search.
//
// =============================================================================

#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace orbital {

// ========================== Special Functions ================================

/// Evaluate the Associated Laguerre polynomial L_k^{alpha}(x)
/// using the standard recurrence relation:
///   j·L_j = (2j - 1 + alpha - x)·L_{j-1} - (j - 1 + alpha)·L_{j-2}
inline double assoc_laguerre(int k, int alpha, double x) {
    if (k == 0) return 1.0;

    double Lm2 = 1.0;
    double Lm1 = 1.0 + alpha - x;
    if (k == 1) return Lm1;

    double L = Lm1;
    for (int j = 2; j <= k; ++j) {
        L = ((2 * j - 1 + alpha - x) * Lm1 - (j - 1 + alpha) * Lm2) / j;
        Lm2 = Lm1;
        Lm1 = L;
    }
    return L;
}

/// Evaluate the Associated Legendre polynomial P_l^m(x)
/// using the standard recurrence relation. Requires |m| <= l and x in [-1, 1].
inline double assoc_legendre(int l, int m, double x) {
    // Start with P_m^m(x)
    double Pmm = 1.0;
    if (m > 0) {
        double somx2 = std::sqrt((1.0 - x) * (1.0 + x));
        double fact = 1.0;
        for (int j = 1; j <= m; ++j) {
            Pmm *= -fact * somx2;
            fact += 2.0;
        }
    }

    if (l == m) return Pmm;

    // P_{m+1}^m(x)
    double Pm1m = x * (2 * m + 1) * Pmm;
    if (l == m + 1) return Pm1m;

    // Recurrence for l >= m+2
    double Plm = Pm1m;
    for (int ll = m + 2; ll <= l; ++ll) {
        Plm = ((2 * ll - 1) * x * Pm1m - (ll + m - 1) * Pmm) / (ll - m);
        Pmm = Pm1m;
        Pm1m = Plm;
    }
    return Plm;
}

// ========================== Wavefunction Components ==========================

/// Squared radial wavefunction |R_{nl}(r)|² (without the r² volume factor).
///   R_{nl}(r) = N · exp(-ρ/2) · ρ^l · L_{n-l-1}^{2l+1}(ρ)
/// where ρ = 2r / (n·a0) and N is the normalization constant.
inline double radial_wavefunction_sq(double r, int n, int l, double a0 = 1.0) {
    double rho = 2.0 * r / (n * a0);
    int k = n - l - 1;
    int alpha = 2 * l + 1;

    double L = assoc_laguerre(k, alpha, rho);

    double norm = std::pow(2.0 / (n * a0), 3)
                * std::tgamma(n - l)
                / (2.0 * n * std::tgamma(n + l + 1));

    double R = std::sqrt(norm) * std::exp(-rho / 2.0) * std::pow(rho, l) * L;
    return R * R;
}

/// Angular probability |P_l^m(cos θ)|² (the θ-dependent part of |Y_l^m|²).
inline double angular_probability(double theta, int l, int m) {
    double x = std::cos(theta);
    double Plm = assoc_legendre(l, m, x);
    return Plm * Plm;
}

/// Full probability density |ψ_{n,l,m}|² = r² · |R_{nl}(r)|² · |P_l^m(cos θ)|²
/// (the r² comes from the spherical volume element; φ part is uniform).
inline double probability_density(double r, double theta, int n, int l, int m,
                                  double a0 = 1.0) {
    return r * r * radial_wavefunction_sq(r, n, l, a0) * angular_probability(theta, l, m);
}

// ========================== CDF-Inversion Samplers ===========================

/// Radial CDF-inversion sampler.
/// Draws a random radius r from the distribution P(r) = r² |R_{nl}(r)|².
///
/// Uses a precomputed CDF that is rebuilt when (n, l) change — this fixes
/// the original static-cache bug where changing quantum numbers at runtime
/// would silently return samples from the old distribution.
struct RadialSampler {
    int cached_n = -1, cached_l = -1;
    std::vector<double> cdf;
    double r_max = 0.0;
    double dr = 0.0;
    static constexpr int NUM_BINS = 4096;

    double sample(int n, int l, std::mt19937& gen, double a0 = 1.0) {
        if (n != cached_n || l != cached_l) {
            rebuild(n, l, a0);
        }

        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double u = dist(gen);
        int idx = static_cast<int>(
            std::lower_bound(cdf.begin(), cdf.end(), u) - cdf.begin());
        return idx * dr;
    }

private:
    void rebuild(int n, int l, double a0) {
        cached_n = n;
        cached_l = l;
        r_max = 10.0 * n * n * a0;
        dr = r_max / (NUM_BINS - 1);

        cdf.resize(NUM_BINS);
        double sum = 0.0;
        for (int i = 0; i < NUM_BINS; ++i) {
            double r = i * dr;
            double pdf = r * r * radial_wavefunction_sq(r, n, l, a0);
            sum += pdf;
            cdf[i] = sum;
        }
        for (double& v : cdf) v /= sum;
    }
};

/// Polar-angle CDF-inversion sampler.
/// Draws a random θ from the distribution P(θ) = sin(θ) |P_l^m(cos θ)|².
struct ThetaSampler {
    int cached_l = -1, cached_m = -1;
    std::vector<double> cdf;
    double dtheta = 0.0;
    static constexpr int NUM_BINS = 2048;

    double sample(int l, int m, std::mt19937& gen) {
        if (l != cached_l || m != cached_m) {
            rebuild(l, m);
        }

        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double u = dist(gen);
        int idx = static_cast<int>(
            std::lower_bound(cdf.begin(), cdf.end(), u) - cdf.begin());
        return idx * dtheta;
    }

private:
    void rebuild(int l, int m) {
        cached_l = l;
        cached_m = m;
        dtheta = M_PI / (NUM_BINS - 1);

        cdf.resize(NUM_BINS);
        double sum = 0.0;
        for (int i = 0; i < NUM_BINS; ++i) {
            double theta = i * dtheta;
            double Plm = assoc_legendre(l, m, std::cos(theta));
            double pdf = std::sin(theta) * Plm * Plm;
            sum += pdf;
            cdf[i] = sum;
        }
        for (double& v : cdf) v /= sum;
    }
};

/// Sample φ uniformly from [0, 2π).
inline double sample_phi(std::mt19937& gen) {
    std::uniform_real_distribution<double> dist(0.0, 2.0 * M_PI);
    return dist(gen);
}

// ========================== Coordinate Conversion ============================

struct Vec3 {
    float x, y, z;
};

/// Convert spherical coordinates (r, θ, φ) → Cartesian (x, y, z).
/// Convention: y-up (θ measured from +y axis).
inline Vec3 spherical_to_cartesian(float r, float theta, float phi) {
    return {
        r * std::sin(theta) * std::cos(phi),
        r * std::cos(theta),
        r * std::sin(theta) * std::sin(phi)
    };
}

// ========================== Color Mapping ====================================

struct Color4 {
    float r, g, b, a;
};

/// Fire/heat color ramp: black → purple → red → orange → yellow → white.
/// Input value should be in [0, 1]; it is clamped internally.
inline Color4 heatmap_fire(float value) {
    value = std::max(0.0f, std::min(1.0f, value));

    constexpr int NUM_STOPS = 6;
    const Color4 colors[NUM_STOPS] = {
        {0.0f, 0.0f, 0.0f, 1.0f},  // 0.0: Black
        {0.3f, 0.0f, 0.6f, 1.0f},  // 0.2: Dark Purple
        {0.8f, 0.0f, 0.0f, 1.0f},  // 0.4: Deep Red
        {1.0f, 0.5f, 0.0f, 1.0f},  // 0.6: Orange
        {1.0f, 1.0f, 0.0f, 1.0f},  // 0.8: Yellow
        {1.0f, 1.0f, 1.0f, 1.0f}   // 1.0: White
    };

    float scaled = value * (NUM_STOPS - 1);
    int i = static_cast<int>(scaled);
    int next = std::min(i + 1, NUM_STOPS - 1);
    float t = scaled - i;

    return {
        colors[i].r + t * (colors[next].r - colors[i].r),
        colors[i].g + t * (colors[next].g - colors[i].g),
        colors[i].b + t * (colors[next].b - colors[i].b),
        1.0f
    };
}

/// Compute intensity-based color for a point in an orbital.
/// Maps |ψ|² through the heatmap with a per-n scaling factor.
inline Color4 orbital_color(double r, double theta, int n, int l, int m,
                            float intensity_scale, double a0 = 1.0) {
    double radial = radial_wavefunction_sq(r, n, l, a0);
    double angular = angular_probability(theta, l, m);
    double intensity = radial * angular;
    return heatmap_fire(static_cast<float>(intensity * intensity_scale));
}

// ========================== Simulation State =================================

/// Encapsulates all mutable simulation state — quantum numbers, particles,
/// and RNG — eliminating global mutable state.
struct SimulationState {
    int n = 2;
    int l = 1;
    int m = 0;
    int particle_count = 100000;
    std::mt19937 rng{std::random_device{}()};
    RadialSampler radial_sampler;
    ThetaSampler theta_sampler;

    /// Clamp quantum numbers to valid ranges:
    ///   0 < n,  0 <= l < n,  -l <= m <= l
    void clamp_quantum_numbers() {
        if (n < 1) n = 1;
        if (l > n - 1) l = n - 1;
        if (l < 0) l = 0;
        if (m > l) m = l;
        if (m < -l) m = -l;
    }
};

// ========================== Probability Flow ==================================

/// Calculate the probability current velocity for a particle at position (x,y,z).
/// For a hydrogen eigenstate with quantum number m, the azimuthal flow is:
///   v_φ = ℏm / (m_e · r · sin θ)
/// converted to Cartesian components. Uses atomic units (ℏ = m_e = 1).
inline Vec3 probability_flow(float px, float py, float pz, int m_quantum) {
    double r = std::sqrt(px * px + py * py + pz * pz);
    if (r < 1e-6) return {0.0f, 0.0f, 0.0f};

    double theta = std::acos(py / r);
    double phi = std::atan2(pz, px);

    double sin_theta = std::sin(theta);
    if (std::abs(sin_theta) < 1e-4) sin_theta = 1e-4;

    // ℏ = m_e = 1 in atomic units
    double v_mag = static_cast<double>(m_quantum) / (r * sin_theta);

    return {
        static_cast<float>(-v_mag * std::sin(phi)),
        0.0f,
        static_cast<float>(v_mag * std::cos(phi))
    };
}

} // namespace orbital

#endif // ORBITAL_MATH_H
