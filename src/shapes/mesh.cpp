#include <lightwave.hpp>

#include "../core/plyparser.hpp"
#include "accel.hpp"

namespace lightwave {

/**
 * @brief A shape consisting of many (potentially millions) of triangles, which
 * share an index and vertex buffer. Since individual triangles are rarely
 * needed (and would pose an excessive amount of overhead), collections of
 * triangles are combined in a single shape.
 */
class TriangleMesh : public AccelerationStructure {
    /**
     * @brief The index buffer of the triangles.
     * The n-th element corresponds to the n-th triangle, and each component of
     * the element corresponds to one vertex index (into @c m_vertices ) of the
     * triangle. This list will always contain as many elements as there are
     * triangles.
     */
    std::vector<Vector3i> m_triangles;
    /**
     * @brief The vertex buffer of the triangles, indexed by m_triangles.
     * Note that multiple triangles can share vertices, hence there can also be
     * fewer than @code 3 * numTriangles @endcode vertices.
     */
    std::vector<Vertex> m_vertices;
    /// @brief The file this mesh was loaded from, for logging and debugging
    /// purposes.
    std::filesystem::path m_originalPath;
    /// @brief Whether to interpolate the normals from m_vertices, or report the
    /// geometric normal instead.
    bool m_smoothNormals;

protected:
    int numberOfPrimitives() const override { return int(m_triangles.size()); }

    bool intersect(int primitiveIndex, const Ray &ray, Intersection &its,
                   Sampler &rng) const override {

        // hints:
        // * use m_triangles[primitiveIndex] to get the vertex indices of the
        // triangle that should be intersected
        // * if m_smoothNormals is true, interpolate the vertex normals from
        // m_vertices
        //   * make sure that your shading frame stays orthonormal!
        // * if m_smoothNormals is false, use the geometrical normal (can be
        // computed from the vertex positions)
        const Vector3i tri = m_triangles[primitiveIndex];
        const Vertex c1    = m_vertices[tri[0]];
        const Vertex c2    = m_vertices[tri[1]];
        const Vertex c3    = m_vertices[tri[2]];

        const Vector d = ray.direction;
        const Vector o = Vector(ray.origin);

        const Vector edge1 = c2.position - c1.position;
        const Vector edge2 = c3.position - c1.position;
        const float detM   = edge1.dot(d.cross(edge2));

        if (fabs(detM) < 1.0e-6) {
            return false;
        }
        const float invDetM = 1 / detM;

        const float detMu = (o - c1.position).dot(d.cross(edge2));
        const float u     = detMu * invDetM;
        if (u < 0 || u > 1) {
            return false;
        }

        // Copied calculations from
        // https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html
        // float detMv = edge1.dot((o - c1.position).cross(edge2));
        // TODO: Optimize these, some double calculations
        const float detMv = d.dot((o - c1.position).cross(edge1));
        const float v     = detMv * invDetM;
        if (v < 0 || u + v > 1) {
            return false;
        }

        // Copied calculations from
        // https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html
        // float detMt = edge1.dot(d.cross(o - c1.position));
        // TODO: Optimize these, some double calculations
        const float detMt = edge2.dot((o - c1.position).cross(edge1));

        const float t = detMt * invDetM;

        // note that we never report an intersection closer than Epsilon (to
        // avoid self-intersections)! we also do not update the intersection if
        // a closer intersection already exists (i.e., its.t is lower than our
        // own t)
        if (t < Epsilon || t > its.t)
            return false;

        // compute the hitpoint
        const Point position = ray(t);

        // we have determined there was an intersection! we are now free to
        // change the intersection object and return true.
        its.t = t;

        // TODO: Maybe move this into inline populate function?

        // After line ~111 where you set its.position:
        its.position = position;

        // **ADD THIS CRITICAL LINE**
        its.uv = (1 - u - v) * c1.uv + u * c2.uv + v * c3.uv;

        // Then continue with the rest as before:
        its.geometryNormal = (c2.position - c1.position)
                                 .cross(c3.position - c1.position)
                                 .normalized();

        if (m_smoothNormals) {
            its.shadingNormal =
                Vertex::interpolate({ u, v }, c1, c2, c3).normal.normalized();
        } else {
            its.shadingNormal = its.geometryNormal;
        }

        // Copied from Khalil's implementation
        // TODO: Come back to this and implement properly
        // TODO: Doesn't seem to be covered by tests so not sure if correct
        Vector dpAB  = c2.position - c1.position;
        Vector dpAC  = c3.position - c1.position;
        Vector2 uvAB = c2.uv - c1.uv;
        Vector2 uvAC = c3.uv - c1.uv;

        float uvDet = uvAB.x() * uvAC.y() - uvAB.y() * uvAC.x();

        if (fabs(uvDet) > 1e-10f) {
            float inv = 1.0f / uvDet;

            // Tangent points along +u direction on the surface
            its.tangent = (dpAB * uvAC.y() - dpAC * uvAB.y()) * inv;

            // Make it a proper tangent for your shading frame
            its.tangent = its.tangent.normalized();
            its.tangent = (its.tangent - its.shadingNormal *
                                             its.tangent.dot(its.shadingNormal))
                              .normalized();
        } else {
            // UVs are degenerate -> build any stable tangent from the normal
            Vector bitangent;
            buildOrthonormalBasis(its.shadingNormal, its.tangent, bitangent);
        }

        // TODO: not implemented
        its.pdf = 1.0f;

        return true;
    }

    float transmittance(int primitiveIndex, const Ray &ray, float tMax,
                        Sampler &rng) const override {
        Intersection its(-ray.direction, tMax);
        return intersect(ray, its, rng) ? 0.f : 1.f;
    }

    Bounds getBoundingBox(int primitiveIndex) const override {
        const Vector3i tri = m_triangles[primitiveIndex];
        const Vertex c1    = m_vertices[tri[0]];
        const Vertex c2    = m_vertices[tri[1]];
        const Vertex c3    = m_vertices[tri[2]];

        return Bounds(
            Point{
                std::min({ c1.position.x(), c2.position.x(), c3.position.x() }),
                std::min({ c1.position.y(), c2.position.y(), c3.position.y() }),
                std::min(
                    { c1.position.z(), c2.position.z(), c3.position.z() }) },
            Point{
                std::max({ c1.position.x(), c2.position.x(), c3.position.x() }),
                std::max({ c1.position.y(), c2.position.y(), c3.position.y() }),
                std::max(
                    { c1.position.z(), c2.position.z(), c3.position.z() }) });
    }

    Point getCentroid(int primitiveIndex) const override {
        const Vector3i tri = m_triangles[primitiveIndex];
        const Vertex c1    = m_vertices[tri[0]];
        const Vertex c2    = m_vertices[tri[1]];
        const Vertex c3    = m_vertices[tri[2]];
        return (Vector(c1.position) + Vector(c2.position) +
                Vector(c3.position)) /
               3;
    }

public:
    TriangleMesh(const Properties &properties) {
        m_originalPath  = properties.get<std::filesystem::path>("filename");
        m_smoothNormals = properties.get<bool>("smooth", true);
        readPLY(m_originalPath, m_triangles, m_vertices);
        logger(EInfo,
               "loaded ply with %d triangles, %d vertices",
               m_triangles.size(),
               m_vertices.size());
        buildAccelerationStructure();
    }

    bool intersect(const Ray &ray, Intersection &its,
                   Sampler &rng) const override {
        PROFILE("Triangle mesh")
        return AccelerationStructure::intersect(ray, its, rng);
    }

    AreaSample sampleArea(Sampler &rng) const override{
        // only implement this if you need triangle mesh
        // area light sampling for
        // your rendering competition
        NOT_IMPLEMENTED
    }

    std::string toString() const override {
        return tfm::format(
            "Mesh[\n"
            "  vertices = %d,\n"
            "  triangles = %d,\n"
            "  filename = \"%s\"\n"
            "]",
            m_vertices.size(),
            m_triangles.size(),
            m_originalPath.generic_string());
    }
};

} // namespace lightwave

REGISTER_SHAPE(TriangleMesh, "mesh")
