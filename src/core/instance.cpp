#include <lightwave/core.hpp>
#include <lightwave/instance.hpp>
#include <lightwave/registry.hpp>
#include <lightwave/sampler.hpp>
#include <lightwave/texture.hpp> // Required for m_alpha

namespace lightwave {

void Instance::applyNormalMap(Intersection &cand) const {
    if (!m_normal)
        return;

    Color c = m_normal->evaluate(cand.uv); // [0,1]

    Vector n_tan(2.0f * c.r() - 1.0f, 2.0f * c.g() - 1.0f, 2.0f * c.b() - 1.0f);

    n_tan = Vector(n_tan.x() * m_normalStrength,
                   n_tan.y() * m_normalStrength,
                   n_tan.z())
                .normalized();

    Frame f            = cand.shadingFrame();
    cand.shadingNormal = f.toWorld(n_tan).normalized();
}

void Instance::transformFrame(SurfaceEvent &surf, const Vector &wo) const {
    // Normals: inverse-transpose (needed for non-uniform scaling)
    surf.geometryNormal =
        surf.instance->m_transform->applyNormal(surf.geometryNormal)
            .normalized();
    surf.shadingNormal =
        surf.instance->m_transform->applyNormal(surf.shadingNormal)
            .normalized();

    // Tangent: regular vector transform (w=0), then re-orthonormalize vs N
    surf.tangent = surf.instance->m_transform->apply(surf.tangent).normalized();

    // Gram–Schmidt: make tangent perpendicular to shadingNormal
    surf.tangent = (surf.tangent -
                    surf.shadingNormal * surf.tangent.dot(surf.shadingNormal))
                       .normalized();

    // Point: regular point transform (w=1)
    surf.position = surf.instance->m_transform->apply(surf.position);
}

inline void validateIntersection(const Intersection &its) {
    assert_finite(its.t, {
        logger(
            EError,
            "  your intersection produced a non-finite intersection distance");
        logger(EError, "  offending shape: %s", its.instance->shape());
    });
    assert_condition(its.t >= Epsilon, {
        logger(EError,
               "  your intersection is susceptible to self-intersections");
        logger(EError, "  offending shape: %s", its.instance->shape());
        logger(EError,
               "  returned t: %.3g (smaller than Epsilon = %.3g)",
               its.t,
               Epsilon);
    });
}

bool Instance::intersect(const Ray &worldRay, Intersection &its,
                         Sampler &rng) const {
    const Intersection itsBackup = its;
    const float tMaxWorld        = itsBackup.t;

    auto acceptByAlpha = [&](const Intersection &cand) -> bool {
        if (!m_alpha)
            return true;
        float a = std::clamp(m_alpha->scalar(cand.uv), 0.0f, 1.0f);
        return rng.next() <= a;
    };

    // ----------------------------
    // Path 1: No transform
    // ----------------------------
    if (!m_transform) {
        Ray r             = worldRay;
        float tAccumWorld = 0.0f;

        for (int guard = 0; guard < 256; ++guard) {
            Intersection cand = itsBackup;
            cand.t            = tMaxWorld - tAccumWorld;
            if (cand.t <= Epsilon) {
                its = itsBackup;
                return false;
            }

            if (!m_shape->intersect(r, cand, rng)) {
                its = itsBackup;
                return false;
            }

            cand.instance = this;
            validateIntersection(cand);

            if (acceptByAlpha(cand)) {
                cand.t += tAccumWorld; // convert to worldRay parameter
                applyNormalMap(cand);  // apply once
                its = cand;
                return true;
            }

            // rejected: step forward and continue
            tAccumWorld += cand.t + Epsilon;
            r.origin = worldRay.origin + tAccumWorld * worldRay.direction;
        }

        its = itsBackup;
        return false;
    }

    // ----------------------------
    // Path 2: With transform
    // ----------------------------
    Ray localRay = m_transform->inverse(worldRay).normalized();

    float tMaxLocal = std::numeric_limits<float>::infinity();
    if (itsBackup) {
        const Point pLocalMax = m_transform->inverse(itsBackup.position);
        tMaxLocal             = (pLocalMax - localRay.origin).length();
    }

    Ray r             = localRay;
    float tAccumLocal = 0.0f;

    for (int guard = 0; guard < 256; ++guard) {
        Intersection cand = itsBackup;
        cand.t            = tMaxLocal - tAccumLocal;
        if (cand.t <= Epsilon) {
            its = itsBackup;
            return false;
        }

        if (!m_shape->intersect(r, cand, rng)) {
            its = itsBackup;
            return false;
        }

        cand.instance = this;
        validateIntersection(cand);

        if (acceptByAlpha(cand)) {
            // Transform hit info to world
            transformFrame(cand, -r.direction);

            // Compute world t (distance from original world ray origin)
            cand.t = (cand.position - worldRay.origin).length();

            // Enforce global closest-hit constraint
            if (cand.t >= tMaxWorld) {
                its = itsBackup;
                return false;
            }

            applyNormalMap(cand); // apply once (after transformFrame)
            its = cand;
            return true;
        }

        // rejected: step forward and continue
        tAccumLocal += cand.t + Epsilon;
        r.origin = localRay.origin + tAccumLocal * localRay.direction;
    }

    its = itsBackup;
    return false;
}

float Instance::transmittance(const Ray &worldRay, float tMax,
                              Sampler &rng) const {
    // If alpha mask exists, we must use the full intersection test
    // to determine if the specific UV coordinate is transparent.
    if (m_alpha) {
        Intersection its;
        if (this->intersect(worldRay, its, rng)) {
            if (its.t < tMax) {
                return 0.0f; // Blocked by opaque part
            }
        }
        return 1.0f; // Transparent or missed
    }

    if (!m_transform) {
        return m_shape->transmittance(worldRay, tMax, rng);
    }

    Ray localRay = m_transform->inverse(worldRay);

    const float dLength = localRay.direction.length();
    if (dLength == 0)
        return 0;
    localRay.direction /= dLength;
    tMax *= dLength;

    return m_shape->transmittance(localRay, tMax, rng);
}

Bounds Instance::getBoundingBox() const {
    if (!m_transform) {
        return m_shape->getBoundingBox();
    }

    const Bounds untransformedAABB = m_shape->getBoundingBox();
    if (untransformedAABB.isUnbounded()) {
        return Bounds::full();
    }

    Bounds result;
    for (int point = 0; point < 8; point++) {
        Point p = untransformedAABB.min();
        for (int dim = 0; dim < p.Dimension; dim++) {
            if ((point >> dim) & 1) {
                p[dim] = untransformedAABB.max()[dim];
            }
        }
        p = m_transform->apply(p);
        result.extend(p);
    }
    return result;
}

Point Instance::getCentroid() const {
    if (!m_transform) {
        return m_shape->getCentroid();
    }
    return m_transform->apply(m_shape->getCentroid());
}

AreaSample Instance::sampleArea(Sampler &rng) const {
    AreaSample sample = m_shape->sampleArea(rng);
    transformFrame(sample, Vector());
    return sample;
}

} // namespace lightwave

REGISTER_CLASS(Instance, "instance", "default")