# lightwave

Welcome to _lightwave_, an educational framework for writing ray tracers that can render photo-realistic images!
Lightwave provides the boring boilerplate, so you can focus on writing the insightful parts.
It aims to be minimal enough to remain comprehensible, yet flexible enough to provide a solid foundation even for sophisticated rendering algorithms.

## What's Included
Out of the box, lightwave is unable to produce any images, as it lacks all necessary rendering functionality to do so.
It is your job to write the various components that make this possible: You will write camera models, intersect shapes, program their appearance, and orchestrate how rays are traced throughout the virtual scene.
Lightwave supports you in this endeavour by supplying tedious to implement boilerplate, including:

* Modularity
  * Modern APIs flexible enough for sophisticated algorithms
  * Shapes, materials, etc are implemented as plugins
  * Whatever you can think of, you can make a plugin for it
* Basic math library
  * Vector operations
  * Matrix operations
  * Transforms
* File I/O
  * An XML parser for the lightwave scene format
  * Reading and writing various image formats
  * Reading and representing triangle meshes
  * Streaming images to the [tev](https://github.com/Tom94/tev) image viewer
* Multi-threading
  * Rendering is parallelized across all available cores
  * Parallelized scene loading (image loading, BVH building, etc)
* BVH acceleration structure
  * Data-structure and traversal is supplied by us
  * Split-in-the-middle build is supplied as well
  * It's your job to implement more sophisticated building
* Useful utilities
  * Thread-safe logger functionality
  * Assert statements that provide extra context
  * An embedded profiler to identify bottlenecks of your code
  * Random number generators
* A Blender exporter
  * You can easily build and render your own scenes

## My Contributions

### Core Rendering Architecture
* **Camera & Primitives:** Implemented a perspective camera model with ray generation and transformations. Built intersection logic for fundamental shapes (spheres, triangle meshes via Möller-Trumbore) and arbitrary output variable (AOV) integrators to visualize shading normals.
* **Acceleration Structures:** Upgraded the naive bounding volume hierarchy (BVH) to use the Surface Area Heuristic (SAH) with binning, drastically reducing ray-intersection testing times for complex meshes.
* **Direct Lighting & Integrators:** Built a direct lighting integrator supporting point lights, directional lights, and emissive shapes using Lambertian emission. Extended this into a full Monte Carlo Unidirectional Path Tracer supporting recursive multi-bounce indirect illumination and Next-Event Estimation (NEE).

### Materials & Textures (BSDFs)
* **Basic Materials:** Implemented Diffuse (Lambertian) BSDFs with cosine-weighted hemisphere sampling, Dielectrics (glass) with reflection/refraction based on Fresnel terms, and smooth Conductors (mirrors).
* **Microfacet Models:** Implemented rough conductors utilizing the Trowbridge-Reitz (GGX) microfacet distribution with visible normal sampling and Smith masking-shadowing. Combined these into a Principled BSDF supporting weighted diffuse and metallic lobes.
* **Texturing:** Built robust 2D texture mapping with UV coordinate clamping/repeating and Bilinear/Nearest-Neighbor filtering. Implemented Image-Based Lighting (IBL) by mapping 3D light directions to environment map spherical coordinates.

### Advanced Rendering Features
* **Volume Rendering:** Implemented homogeneous volume rendering using distance sampling and a Henyey-Greenstein phase function. Adapted the path tracer to account for volumetric transmittance and out-scattering along shadow rays, enabling ambient fog and volumetric shadows within bounded shapes.
* **Normal Mapping:** Extended the Instance system to perturb shading normals via normal maps. Applied maps within the intersection transform path, handling tangent-space to world-space conversions and re-orthonormalization to support non-uniform scaling seamlessly.
* **Alpha Masking:** Added probabilistic ray-intersection discarding based on alpha textures to efficiently render cut-out details (e.g., leaves, fences) without adding complex geometric overhead.
* **Post-Processing Pipeline:** Built a custom post-processing stage for Bloom to mimic physical camera scattering (bright-pass extraction, blurring, and compositing), and chained it with a Reinhard Tone Mapping implementation to safely compress high dynamic range (HDR) radiance into the displayable [0,1] sRGB gamut.

## Contributors
Lightwave was written by [Alexander Rath](https://graphics.cg.uni-saarland.de/people/rath.html), with contributions from [Ömercan Yazici](https://graphics.cg.uni-saarland.de/people/yazici.html) and [Philippe Weier](https://graphics.cg.uni-saarland.de/people/weier.html).
Many of our design decisions were heavily inspired by [Nori](https://wjakob.github.io/nori/), a great educational renderer developed by Wenzel Jakob.
We would also like to thank the teams behind our dependencies: [ctpl](https://github.com/vit-vit/CTPL), [miniz](https://github.com/richgel999/miniz), [stb](https://github.com/nothings/stb), [tinyexr](https://github.com/syoyo/tinyexr), [tinyformat](https://github.com/c42f/tinyformat), [pcg32](https://github.com/wjakob/pcg32), and [catch2](https://github.com/catchorg/Catch2).
