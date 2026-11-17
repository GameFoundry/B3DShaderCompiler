===========
Ray Tracing
===========

XShaderCompiler accepts the ray tracing shaders of DirectX Raytracing (DXR),
and adds two built-in functions, ``RayTraceClosest`` and ``RayTraceAny``,
that trace a ray from inside any shader. Select HLSL 6 as the input language.
The output targets ``HLSL6`` and ``HLSL6VK`` support everything on this page.
Both produce HLSL for DXC, the DirectX Shader Compiler: ``HLSL6`` for its
DXIL output and ``HLSL6VK`` for its SPIR-V output.

A ray is traced against an *acceleration structure*: the scene's geometry
stored so that the GPU can search it quickly. It holds *instances*, each of
which places one mesh in the scene. The application builds it, and a shader
only reads it. There are two ways to trace:

Pipeline
    A shader calls ``TraceRay``, and the GPU runs other shaders for what the
    ray meets, such as a closest hit shader or a miss shader. There are six
    kinds of shader in a pipeline, the *ray tracing stages*. Each shader is
    an entry point marked with the ``shader`` attribute.

Inline
    The shader calls ``RayTraceClosest`` or ``RayTraceAny`` and gets the
    result back as a value. This works in every stage, including vertex,
    fragment and compute shaders.

.. contents::
   :local:
   :depth: 2

Built-in names
==============

The structs and constants in this section are declared by the compiler when
the input language is HLSL 6. A source that declares one of them itself gets
the error ``identifier 'RayDesc' already declared in this scope``.

Types
-----

``RaytracingAccelerationStructure``
    An acceleration structure. It is a read-only resource and is declared
    like a texture: as a global with an optional ``register(t#)``, as an
    array, as a function parameter, or as a local variable that is read from
    ``ResourceDescriptorHeap`` (see `Acceleration structures from the
    descriptor heap`_). Automatic binding assigns it a ``t`` register.

``RayDesc``
    A ray. The members are, in this order, ``float3 Origin``, ``float TMin``,
    ``float3 Direction`` and ``float TMax``. Hits closer than ``TMin`` or
    further than ``TMax`` are not reported.

``BuiltInTriangleIntersectionAttributes``
    What a pipeline reports about a hit on a triangle. Its only member is
    ``float2 barycentrics``, the weights of the triangle's second and third
    vertex at the hit.

``RayCandidate`` and ``RayHit``
    The input of the callbacks and the result of the inline built-ins. See
    `RayCandidate and RayHit`_.

Constants
---------

Ray flags change how a ray is traced. They are ``uint`` constants that can be
combined with ``|``:

.. list-table::
   :header-rows: 1
   :widths: 50 10 40

   * - Name
     - Value
     - Effect
   * - ``RAY_FLAG_NONE``
     - 0
     - None.
   * - ``RAY_FLAG_FORCE_OPAQUE``
     - 1
     - Treat all geometry as opaque.
   * - ``RAY_FLAG_FORCE_NON_OPAQUE``
     - 2
     - Treat all geometry as non-opaque.
   * - ``RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH``
     - 4
     - Stop at the first hit instead of looking for the closest one.
   * - ``RAY_FLAG_SKIP_CLOSEST_HIT_SHADER``
     - 8
     - Don't run the closest hit shader.
   * - ``RAY_FLAG_CULL_BACK_FACING_TRIANGLES``
     - 16
     - Ignore triangles that face away from the ray.
   * - ``RAY_FLAG_CULL_FRONT_FACING_TRIANGLES``
     - 32
     - Ignore triangles that face the ray.
   * - ``RAY_FLAG_CULL_OPAQUE``
     - 64
     - Ignore opaque geometry.
   * - ``RAY_FLAG_CULL_NON_OPAQUE``
     - 128
     - Ignore non-opaque geometry.
   * - ``RAY_FLAG_SKIP_TRIANGLES``
     - 256
     - Ignore triangles.
   * - ``RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES``
     - 512
     - Ignore procedural primitives.

Geometry is *opaque* when a ray that meets it always hits it. For
*non-opaque* geometry the shader decides, for example with an alpha test. A
*procedural primitive* is a box that stands for a custom shape. The shader
computes where the ray hits the shape inside it.

``HitKind()`` returns one of two more ``uint`` constants for a triangle:
``HIT_KIND_TRIANGLE_FRONT_FACE`` (254) or ``HIT_KIND_TRIANGLE_BACK_FACE``
(255).

Ray tracing pipelines
=====================

The shader attribute
--------------------

A function becomes the entry point of a ray tracing stage with the attribute
``[shader("<stage>")]``. A source can hold any number of them, and each one is
compiled on its own by selecting it with ``-E`` and its stage with ``-T``:

.. list-table::
   :header-rows: 1
   :widths: 14 8 20 28 30

   * - Attribute argument
     - ``-T``
     - ``ShaderTarget``
     - Runs
     - Parameters
   * - ``raygeneration``
     - ``rgen``
     - ``RayGenerationShader``
     - Once for each pixel the application dispatches. Tracing starts here.
     - None.
   * - ``intersection``
     - ``rint``
     - ``IntersectionShader``
     - When a ray enters the box of a procedural primitive.
     - None.
   * - ``anyhit``
     - ``rahit``
     - ``AnyHitShader``
     - For each non-opaque hit the search finds.
     - An ``inout`` payload struct, then an ``in`` attribute struct.
   * - ``closesthit``
     - ``rchit``
     - ``ClosestHitShader``
     - Once, for the closest hit.
     - An ``inout`` payload struct, then an ``in`` attribute struct.
   * - ``miss``
     - ``rmiss``
     - ``MissShader``
     - Once, if the ray hit nothing.
     - One ``inout`` payload struct.
   * - ``callable``
     - ``rcall``
     - ``CallableShader``
     - When another shader calls it with ``CallShader``.
     - One ``inout`` parameter struct.

The *payload* is a struct of the shader's own that travels with the ray. The
caller of ``TraceRay`` fills it in, and the hit and miss shaders read and
change it. The *attribute struct* describes the hit. It is
``BuiltInTriangleIntersectionAttributes`` for triangles, or the struct that an
intersection shader passes to ``ReportHit``.

Every function with the attribute is checked, not only the selected entry
point:

- The function must return ``void`` and take the parameters in the table:
  ``ray tracing entry point 'Entry' must return void`` and, for example,
  ``miss entry point 'Entry' must take one 'inout' payload struct``.
- Payloads, callable parameters and attributes must be structs that hold
  only numeric data: ``payload 'ResourcePayload' must be a struct that holds
  only numeric data``.
- An attribute struct can't be larger than 32 bytes, as a parameter and as an
  argument of ``ReportHit``: ``attribute struct 'LargeAttributes' is 36
  bytes; ray tracing attributes are limited to 32 bytes``. `Reflection`_
  describes how the size is computed.
- The argument must be one of the six names: ``the 'shader' attribute takes
  one of "raygeneration", "intersection", "anyhit", "closesthit", "miss" or
  "callable"``.
- The attribute can appear once, and only on a global function:
  ``function 'Entry' has more than one 'shader' attribute`` and ``the
  'shader' attribute is only allowed on global functions``.

The selected entry point must have the attribute exactly when ``-T`` names a
ray tracing stage, and the two must name the same stage:

- ``entry point 'Entry' needs the attribute [shader("raygeneration")] for
  target 'Ray Generation Shader'``
- ``entry point 'Entry' is marked [shader("miss")], which doesn't match
  target 'Closest-Hit Shader'``
- ``entry point 'Entry' is marked [shader("miss")], but target 'Compute
  Shader' isn't a ray tracing stage (use rgen, rint, rahit, rchit, rmiss or
  rcall)``

A source with a ray generation, a miss and a closest hit shader::

    RaytracingAccelerationStructure gScene : register(t0);
    RWTexture2D<float4> gOutput : register(u0);

    struct Payload
    {
        float3 color;
    };

    [shader("raygeneration")]
    void RayGen()
    {
        uint2 pixel = DispatchRaysIndex().xy;
        float2 uv = (float2(pixel) + 0.5f) / float2(DispatchRaysDimensions().xy);

        RayDesc ray;
        ray.Origin = float3(0, 0, -5);
        ray.TMin = 0.001f;
        ray.Direction = normalize(float3(uv * 2.0f - 1.0f, 1.0f));
        ray.TMax = 1000.0f;

        Payload payload;
        payload.color = 0;
        TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);

        gOutput[pixel] = float4(payload.color, 1.0f);
    }

    [shader("miss")]
    void Miss(inout Payload payload)
    {
        payload.color = float3(0.3f, 0.5f, 0.9f);
    }

    [shader("closesthit")]
    void ClosestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attributes)
    {
        float2 b = attributes.barycentrics;
        payload.color = float3(1.0f - b.x - b.y, b.x, b.y);
    }

Intrinsics
----------

The functions of the pipeline, and the stages that can call each one:

.. list-table::
   :header-rows: 1
   :widths: 46 14 40

   * - Function
     - Returns
     - Stages
   * - ``TraceRay(scene, rayFlags, instanceMask,
       rayContributionToHitGroupIndex,
       multiplierForGeometryContributionToHitGroupIndex, missShaderIndex,
       ray, payload)``
     - ``void``
     - ``raygeneration``, ``closesthit``, ``miss``
   * - ``CallShader(shaderIndex, parameter)``
     - ``void``
     - ``raygeneration``, ``closesthit``, ``miss``, ``callable``
   * - ``ReportHit(t, hitKind, attributes)``
     - ``bool``
     - ``intersection``
   * - ``IgnoreHit()``, ``AcceptHitAndEndSearch()``
     - ``void``
     - ``anyhit``
   * - ``DispatchRaysIndex()``, ``DispatchRaysDimensions()``
     - ``uint3``
     - All six
   * - ``WorldRayOrigin()``, ``WorldRayDirection()``
     - ``float3``
     - ``intersection``, ``anyhit``, ``closesthit``, ``miss``
   * - ``RayTMin()``, ``RayTCurrent()``
     - ``float``
     - ``intersection``, ``anyhit``, ``closesthit``, ``miss``
   * - ``RayFlags()``
     - ``uint``
     - ``intersection``, ``anyhit``, ``closesthit``, ``miss``
   * - ``InstanceIndex()``, ``InstanceID()``, ``GeometryIndex()``,
       ``PrimitiveIndex()``
     - ``uint``
     - ``intersection``, ``anyhit``, ``closesthit``
   * - ``ObjectRayOrigin()``, ``ObjectRayDirection()``
     - ``float3``
     - ``intersection``, ``anyhit``, ``closesthit``
   * - ``ObjectToWorld3x4()``, ``WorldToObject3x4()``
     - ``float3x4``
     - ``intersection``, ``anyhit``, ``closesthit``
   * - ``ObjectToWorld4x3()``, ``WorldToObject4x3()``
     - ``float4x3``
     - ``intersection``, ``anyhit``, ``closesthit``
   * - ``HitKind()``
     - ``uint``
     - ``anyhit``, ``closesthit``

The ``scene`` argument is a ``RaytracingAccelerationStructure`` and ``ray`` is
a ``RayDesc``. The payload of ``TraceRay`` and the parameter of ``CallShader``
are structs passed ``inout``, and ``attributes`` is a struct. ``t`` is a
``float``, and the remaining arguments are ``uint``.

A call to one of these names always means the intrinsic, also when the source
declares a function with the same name. Members and methods of structs can
use the names freely: ``hit.InstanceID`` is the member.

``ObjectToWorld()`` and ``WorldToObject()`` without a suffix are rejected:
``'ObjectToWorld' isn't supported, because DXC can't compile it for SPIR-V;
use 'ObjectToWorld3x4' or 'ObjectToWorld4x3'``.

Stage checks
------------

The compiler follows every call from the selected entry point, through helper
functions and callback methods, and reports each intrinsic that the entry
point's stage doesn't allow. A call whose result is unused is checked too.
Functions the entry point doesn't reach are not checked, so one source can
hold entry points of different stages that share helpers.

Besides the table above, these rules apply:

- Vertex, fragment, compute and the other stages that aren't ray tracing
  stages can call none of the functions in the table. They trace with
  ``RayTraceClosest`` and ``RayTraceAny``, which every stage can call.
- The six ray tracing stages can't use anything that needs screen-space
  derivatives or a thread group:

  - ``Sample``, ``SampleBias``, ``SampleCmp``, ``CalculateLevelOfDetail``
    and ``CalculateLevelOfDetailUnclamped``. Use ``SampleLevel``,
    ``SampleGrad`` or ``SampleCmpLevelZero``.
  - ``ddx``, ``ddy``, their ``_coarse`` and ``_fine`` forms, and ``fwidth``.
  - The ``tex1D``, ``tex2D``, ``tex3D`` and ``texCUBE`` functions without
    gradient arguments, and their ``bias`` and ``proj`` forms.
  - ``discard`` and ``clip``.
  - ``GroupMemoryBarrier``, ``AllMemoryBarrier`` and every barrier that ends
    in ``WithGroupSync``. ``DeviceMemoryBarrier`` is allowed.

The error names the intrinsic and the stage, and lists the calls that lead to
it when a helper is involved. A helper that calls ``TraceRay``, reached from
an any hit shader, reports::

    context error (shader.hlsl:13:5) : 'TraceRay' isn't available in stage 'Any-Hit Shader'
        TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 1, 0, ray, payload);
        ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    call stack:
      'void TraceShadow(inout struct Payload)' (shader.hlsl:19:5)

The ray tracing stages are printed as ``Ray Generation Shader``,
``Intersection Shader``, ``Any-Hit Shader``, ``Closest-Hit Shader``, ``Miss
Shader`` and ``Callable Shader``. Other stages are printed, for example, as
``Fragment Shader`` and ``Compute Shader``.

Inline ray tracing
==================

The built-ins
-------------

::

    RayHit RayTraceClosest(RaytracingAccelerationStructure scene, uint rayFlags, uint instanceMask, RayDesc ray);
    RayHit RayTraceClosest(RaytracingAccelerationStructure scene, uint rayFlags, uint instanceMask, RayDesc ray, inout Callbacks callbacks);
    RayHit RayTraceAny(RaytracingAccelerationStructure scene, uint rayFlags, uint instanceMask, RayDesc ray);
    RayHit RayTraceAny(RaytracingAccelerationStructure scene, uint rayFlags, uint instanceMask, RayDesc ray, inout Callbacks callbacks);

``RayTraceClosest`` returns the closest hit along the ray.

``RayTraceAny`` stops at the first hit it accepts, which need not be the
closest. Use it when only the existence of a hit matters, as for shadows.

An instance is tested only if its mask and ``instanceMask`` have a bit in
common. ``rayFlags`` and ``instanceMask`` can be any scalar integer.
``Callbacks`` stands for a struct of the shader's own, described in
`Callbacks`_.

Without callbacks, every triangle the ray meets counts as a hit and
procedural primitives are skipped::

    RaytracingAccelerationStructure gScene : register(t0);

    float4 PS(float3 worldPosition : TEXCOORD0) : SV_Target0
    {
        RayDesc ray;
        ray.Origin = worldPosition;
        ray.TMin = 0.001f;
        ray.Direction = float3(0, 1, 0);
        ray.TMax = 100.0f;

        RayHit hit = RayTraceAny(gScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, ray);
        return hit.IsHit ? 0.0f : 1.0f;
    }

Callbacks
---------

While it searches, the GPU finds *candidates*: places where the ray may hit.
Two kinds of candidate need a decision from the shader, and the built-ins ask
for it by calling a method of the callbacks struct:

``bool AnyHit(RayCandidate candidate)``
    Called for each non-opaque triangle the ray meets. Return ``true`` to
    accept the candidate as a hit, or ``false`` to ignore it and go on.
    Without this method, non-opaque triangles are accepted.

``bool Intersection(RayCandidate candidate, out float t)``
    Called for each procedural primitive whose box the ray enters. Return
    ``true`` and the distance ``t`` along the ray to report a hit. A ``t``
    that is closer than the ray's ``TMin``, or further than the closest hit
    so far, is ignored. Without this method, procedural primitives are
    skipped.

The struct needs at least one of the two. Its other members and methods are
free to use: members are how a callback gets extra inputs and keeps results.
The struct is passed ``inout``, so what a callback writes to a member can be
read after the call::

    RaytracingAccelerationStructure gScene : register(t0);
    RWTexture2D<float4> gOutput : register(u0);
    StructuredBuffer<float> gOpacity : register(t1);   // One value per instance

    struct SceneCallbacks
    {
        float Cutoff;     // Set by the caller
        uint Rejected;    // Counted by AnyHit, read by the caller after the trace

        // Alpha test: accepts a non-opaque triangle if its instance is opaque enough
        bool AnyHit(RayCandidate candidate)
        {
            if (gOpacity[candidate.InstanceID] >= Cutoff)
                return true;

            Rejected++;
            return false;
        }

        // Custom shape: a unit sphere at the origin of the instance's object space
        bool Intersection(RayCandidate candidate, out float t)
        {
            float3 origin = candidate.ObjectRayOrigin;
            float3 direction = candidate.ObjectRayDirection;

            float a = dot(direction, direction);
            float b = dot(origin, direction);
            float h = b * b - a * (dot(origin, origin) - 1.0f);

            t = (-b - sqrt(max(h, 0.0f))) / a;
            return h >= 0.0f;
        }
    };

    [numthreads(8, 8, 1)]
    void CS(uint3 threadId : SV_DispatchThreadID)
    {
        RayDesc ray;
        ray.Origin = float3(threadId.xy, -10.0f);
        ray.TMin = 0.0f;
        ray.Direction = float3(0, 0, 1);
        ray.TMax = 1000.0f;

        SceneCallbacks callbacks;
        callbacks.Cutoff = 0.5f;
        callbacks.Rejected = 0;

        RayHit hit = RayTraceClosest(gScene, RAY_FLAG_NONE, 0xFF, ray, callbacks);

        gOutput[threadId.xy] = hit.IsHit ? float4(hit.T, hit.IsProcedural, callbacks.Rejected, 1.0f) : 0;
    }

The rules for callbacks, with the error each one reports:

- The callbacks argument must name a local variable or a parameter. A global,
  a ``static`` local, a struct member, an array element or a temporary is
  rejected: ``the callbacks argument of 'RayTraceClosest' must be a local
  variable or a parameter``.
- Its type must be a struct declared at global scope that isn't a template:
  ``the callbacks argument of 'RayTraceClosest' must have a struct type
  declared at global scope, but it is 'float'``.
- The struct must have ``AnyHit``, ``Intersection`` or both: ``callbacks
  struct 'Typo' has neither an 'AnyHit' nor an 'Intersection' method``. This
  catches a misspelled method name.
- Each of the two methods must have exactly the signature above, with no
  overloads, and can't be ``static``: ``'BadAnyHit::AnyHit' must be declared
  as 'bool AnyHit(RayCandidate candidate)'`` and
  ``'BadIntersection::Intersection' must be declared as 'bool
  Intersection(RayCandidate candidate, out float t)'``.
- A callback can't trace another ray, call a callable shader or discard the
  pixel, directly or through a function it calls. ``RayTraceClosest``,
  ``RayTraceAny``, ``TraceRay``, ``CallShader``, ``discard`` and ``clip``
  report, for example, ``'RayTraceAny' can't be used inside the ray tracing
  callback 'Callbacks::AnyHit', because inline traversal can't nest``.

A callback is checked as part of the entry point that traces, so the `Stage
checks`_ apply to it as well. It can read and write global variables and
resources.

The built-ins themselves report ``'RayTraceClosest' takes 4 or 5 arguments
(scene, rayFlags, instanceMask, ray, and optional callbacks), but got 3`` for
a wrong argument count, and for a wrong argument type, for example,
``argument 4 of 'RayTraceAny' must be 'RayDesc', but it is 'float3'``.

RayCandidate and RayHit
-----------------------

``RayCandidate`` is what a callback receives:

.. list-table::
   :header-rows: 1
   :widths: 14 40 46

   * - Type
     - Member
     - Meaning
   * - ``float``
     - ``T``
     - Distance from the ray's origin to the candidate, in units of the
       ray's direction.
   * - ``float2``
     - ``Barycentrics``
     - Weights of the triangle's second and third vertex at the candidate.
   * - ``bool``
     - ``FrontFace``
     - True if the ray meets the front of the triangle.
   * - ``uint``
     - ``InstanceIndex``
     - Position of the instance in the acceleration structure.
   * - ``uint``
     - ``InstanceID``
     - The ID the application gave the instance.
   * - ``uint``
     - ``InstanceContributionToHitGroupIndex``
     - The hit group offset the application gave the instance.
   * - ``uint``
     - ``GeometryIndex``
     - Index of the geometry within the instance's mesh.
   * - ``uint``
     - ``PrimitiveIndex``
     - Index of the triangle or procedural primitive within the geometry.
   * - ``float3``
     - ``ObjectRayOrigin``
     - Origin of the ray in the instance's object space.
   * - ``float3``
     - ``ObjectRayDirection``
     - Direction of the ray in the instance's object space.

``T``, ``Barycentrics`` and ``FrontFace`` are only set in ``AnyHit``. They
are zero in ``Intersection``.

``RayHit`` is what the built-ins return:

.. list-table::
   :header-rows: 1
   :widths: 14 40 46

   * - Type
     - Member
     - Meaning
   * - ``bool``
     - ``IsHit``
     - True if the ray hit something. Every other member is zero if not.
   * - ``bool``
     - ``IsProcedural``
     - True if the hit is on a procedural primitive, false for a triangle.
   * - ``float``
     - ``T``
     - Distance from the ray's origin to the hit, in units of the ray's
       direction.
   * - ``float2``
     - ``Barycentrics``
     - Weights of the triangle's second and third vertex at the hit. Zero
       for a procedural primitive.
   * - ``bool``
     - ``FrontFace``
     - True if the ray hit the front of the triangle. False for a procedural
       primitive.
   * - ``uint``
     - ``InstanceIndex``
     - Position of the instance in the acceleration structure.
   * - ``uint``
     - ``InstanceID``
     - The ID the application gave the instance.
   * - ``uint``
     - ``InstanceContributionToHitGroupIndex``
     - The hit group offset the application gave the instance.
   * - ``uint``
     - ``GeometryIndex``
     - Index of the geometry within the instance's mesh.
   * - ``uint``
     - ``PrimitiveIndex``
     - Index of the triangle or procedural primitive within the geometry.
   * - ``float3x4``
     - ``ObjectToWorld``
     - Transform from the instance's object space to world space.
   * - ``float3x4``
     - ``WorldToObject``
     - Transform from world space to the instance's object space.

Ray flags
---------

The inline built-ins accept ``RAY_FLAG_FORCE_OPAQUE``,
``RAY_FLAG_FORCE_NON_OPAQUE``, ``RAY_FLAG_CULL_BACK_FACING_TRIANGLES``,
``RAY_FLAG_CULL_FRONT_FACING_TRIANGLES``, ``RAY_FLAG_CULL_OPAQUE``,
``RAY_FLAG_CULL_NON_OPAQUE`` and ``RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES``.

When the ``rayFlags`` argument is a constant, any other bit is an error. A
constant is a literal, a ``const`` variable with a constant initializer, or
an expression of those:

- ``ray flag RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH can't be used with
  'RayTraceClosest'; use RayTraceAny to stop at the first hit``
- ``ray flag RAY_FLAG_SKIP_CLOSEST_HIT_SHADER can't be used with
  'RayTraceAny'; inline ray tracing runs no closest-hit shader``
- ``ray flag RAY_FLAG_SKIP_TRIANGLES can't be used with 'RayTraceClosest';
  not supported on all backends``
- ``ray flag 0x400 can't be used with 'RayTraceClosest'; it isn't one of the
  RAY_FLAG_* values the inline built-ins accept``

Flags that are computed at run time can't be checked. The bits that aren't
accepted are removed from them before the ray is traced.

These limits apply to the inline built-ins only. ``TraceRay`` takes every
flag.

RayQuery
--------

DXR's own inline interface, the ``RayQuery`` object, is not part of the input
language. Using the name reports ``'RayQuery' isn't supported; use
RayTraceClosest or RayTraceAny``.

Acceleration structures from the descriptor heap
================================================

With the ``BindlessResources`` language extension (``-Xbindless``, see
``BindlessResources.rst``), an acceleration structure can be read from
``ResourceDescriptorHeap`` into a local variable and then used like any
other. The index must be a scalar ``uint``::

    RWTexture2D<float4> gOutput : register(u0);

    cbuffer SceneParams : register(b0)
    {
        uint gSceneIndex;
    };

    float TraceDistance(RaytracingAccelerationStructure scene, RayDesc ray)
    {
        return RayTraceClosest(scene, RAY_FLAG_NONE, 0xFF, ray).T;
    }

    [numthreads(8, 8, 1)]
    void CS(uint3 threadId : SV_DispatchThreadID)
    {
        RaytracingAccelerationStructure scene = ResourceDescriptorHeap[gSceneIndex];

        RayDesc ray;
        ray.Origin = float3(threadId.xy, 0.0f);
        ray.TMin = 0.0f;
        ray.Direction = float3(0, 0, 1);
        ray.TMax = 1000.0f;

        gOutput[threadId.xy] = TraceDistance(scene, ray);
    }

The same works with ``TraceRay`` in a ray tracing stage. Reflection reports
the resource heap as used. An acceleration structure read from the heap has
no binding of its own, so it isn't listed with the acceleration structure
bindings.

HLSL output
===========

``HLSL6`` and ``HLSL6VK`` write the ray tracing constructs in the same way.

Pipeline constructs
-------------------

The acceleration structure type, ``RayDesc``,
``BuiltInTriangleIntersectionAttributes``, the constants and the pipeline
intrinsics are written as they appear in the source. Their declarations are
not written, because DXC declares them itself.

Only the functions that the selected entry point reaches are written, along
with the functions that initializers of global variables call. Other entry
points of the same source are therefore left out, and the selected entry
point is the only function with a ``shader`` attribute. Global variables,
resources and structs are all written. The output of a ray tracing stage can
be compiled by DXC as a library, for example with ``-T lib_6_6``, and holds
exactly one shader.

Inline built-ins
----------------

``RayCandidate`` and ``RayHit`` are not known to DXC, so their declarations
are written at the top of the output of any source that uses one of them.

Each call is replaced by a call to a generated helper function with the same
arguments. There is one helper for each pair of a built-in and a callbacks
struct:

- ``xsc_RayTraceClosest`` and ``xsc_RayTraceAny`` for calls without
  callbacks. They are written after the two struct declarations, when the
  source contains such a call.
- ``xsc_RayTraceClosest_<Struct>`` and ``xsc_RayTraceAny_<Struct>`` for calls
  with a callbacks struct. They are written right after the declaration of
  the struct, when the selected entry point reaches such a call.

A helper traces the ray with a ``RayQuery`` loop, calls the callbacks for the
candidates and fills in the result. This is the helper for the example in
`Callbacks`_, whose call becomes
``xsc_RayTraceClosest_SceneCallbacks(gScene, RAY_FLAG_NONE, 0xFF, ray,
callbacks)``::

    RayHit xsc_RayTraceClosest_SceneCallbacks(RaytracingAccelerationStructure scene, uint rayFlags, uint instanceMask, RayDesc ray, inout SceneCallbacks callbacks)
    {
        RayQuery<RAY_FLAG_NONE> query;
        query.TraceRayInline(scene, rayFlags & 0x2F3, instanceMask, ray);
        while (query.Proceed())
        {
            if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
            {
                RayCandidate candidate = (RayCandidate)0;
                candidate.InstanceID = query.CandidateInstanceID();
                if (callbacks.AnyHit(candidate))
                    query.CommitNonOpaqueTriangleHit();
            }
            else
            {
                RayCandidate candidate = (RayCandidate)0;
                candidate.ObjectRayOrigin = query.CandidateObjectRayOrigin();
                candidate.ObjectRayDirection = query.CandidateObjectRayDirection();
                float t;
                if (callbacks.Intersection(candidate, t) && t >= query.RayTMin() && t <= query.CommittedRayT())
                    query.CommitProceduralPrimitiveHit(t);
            }
        }

        RayHit hit = (RayHit)0;
        hit.IsHit = query.CommittedStatus() != COMMITTED_NOTHING;
        if (hit.IsHit)
        {
            hit.IsProcedural = query.CommittedStatus() == COMMITTED_PROCEDURAL_PRIMITIVE_HIT;
            hit.T = query.CommittedRayT();
        }
        return hit;
    }

The parts that depend on the call:

Template flags of ``RayQuery``
    ``RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH`` for ``RayTraceAny``, and
    ``RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES`` when there is no ``Intersection``
    method. ``RAY_FLAG_NONE`` if neither applies.

Ray flags
    ``rayFlags & 0x2F3`` keeps the flags that the inline built-ins accept.

Candidates
    Without an ``AnyHit`` method, a non-opaque triangle is committed directly.
    Without an ``Intersection`` method, the ``else`` branch is left out.

Fields
    A helper fills in only the fields that are read, so that no query is made
    for a value nobody uses. In the example, ``AnyHit`` reads only
    ``InstanceID``, ``Intersection`` reads only the object space ray, and the
    shader reads only ``IsHit``, ``IsProcedural`` and ``T`` of the result.

The fields of a ``RayCandidate`` are collected for each callback method,
from the method and the functions it calls. The fields of a ``RayHit`` are
collected once, from every function the entry point reaches, and apply to all
helpers. A field counts as read when it is accessed as a member. A value of
either struct can also be copied to a local variable or a parameter, passed
to a function, returned from a function that isn't an entry point, and used
in parentheses or as a result of ``?:`` without reading more fields. Any
other use fills in every field. Examples are storing the value in a global,
in a struct member or in a buffer, casting it, and returning it from an entry
point.

Compiling the helpers needs Shader Model 6.5 or later, which introduced
``RayQuery``. Other output targets may implement the two built-ins
differently.

Reflection
==========

``Reflection::ReflectionData`` reports what an application needs to bind
resources and to create a ray tracing pipeline:

``accelerationStructures``
    One binding slot, with identifier, location and set, for each global
    ``RaytracingAccelerationStructure``. An array is one entry. They also
    appear in ``uniforms`` as buffers of type
    ``BufferType::RaytracingAccelerationStructure``, but not in ``textures``.

``usesInlineRayTracing``
    True if the selected entry point reaches a call to ``RayTraceClosest`` or
    ``RayTraceAny``.

``functions``
    Each ``Reflection::Function`` has four more members. The first three are
    set for every function of the source that has a ``shader`` attribute, not
    only for the selected entry point:

    ``shaderTarget``
        The stage named by the attribute, or ``ShaderTarget::Undefined`` for a
        function without one.

    ``rayPayloadSize``
        Size in bytes of the payload parameter of a miss, closest hit or any
        hit function. 0 for other functions.

    ``hitAttributeSize``
        Size in bytes of the attribute parameter of a closest hit or any hit
        function, which is 8 for triangles. For an intersection function, the
        size of the largest struct that it, or a function it calls, passes to
        ``ReportHit``. 0 for other functions.

    ``rayTracingSystemValues``
        The system values the selected entry point and the functions it calls
        read, as ``Reflection::RayTracingSystemValues`` bits named after the
        intrinsics that return them (``ObjectToWorld`` stands for both
        ``ObjectToWorld3x4`` and ``ObjectToWorld4x3``, likewise
        ``WorldToObject``). ``HitAttributes`` is set when a closest hit or any
        hit function reads its attribute parameter. Only set for the selected
        entry point, and only if it is a ray tracing stage; 0 for other
        functions.

Sizes follow the layout DXC uses for payloads and attributes. ``bool``,
``int``, ``uint``, ``half`` and ``float`` take 4 bytes and ``double`` takes 8.
A vector or matrix is that many scalars without padding. Each member is
aligned to the size of its scalar type, and the size of a struct is rounded
up to its largest alignment. For example, ``float3, float`` is 16 bytes,
``float, float2, float, float`` is 20 and ``float, double, float`` is 24.

``--reflect`` prints the new data after the existing sections, and only for
sources that use ray tracing. For the pipeline example above, compiled for
any of its entry points::

    Acceleration Structures:
      0: gScene, set 0, usesDynamicOffset false
    Ray Tracing:
      Inline ray tracing: unused
      RayGen: Ray Generation Shader, payload 0 bytes, attributes 0 bytes
      Miss: Miss Shader, payload 12 bytes, attributes 0 bytes
      ClosestHit: Closest-Hit Shader, payload 12 bytes, attributes 8 bytes
      System values of RayGen: DispatchRaysIndex DispatchRaysDimensions

The last line lists the system values of the selected entry point, here
``RayGen``.

Command line
============

A ray tracing stage is selected like any other, by its entry point and
target. The inline built-ins need no option::

    xsc -Vin HLSL6 -Vout HLSL6 -E RayGen -T rgen shader.hlsl
    xsc -Vin HLSL6 -Vout HLSL6VK -E ClosestHit -T rchit shader.hlsl
    xsc -Vin HLSL6 -Vout HLSL6 -E Miss -T rmiss --reflect shader.hlsl

The equivalent library settings are ``ShaderInput::shaderVersion``,
``ShaderInput::entryPoint`` and ``ShaderInput::shaderTarget``, with the
``ShaderTarget`` values from the table in `The shader attribute`_.

Limits
======

Input and output
    The built-in structs and constants exist only for HLSL 6 input. The
    output targets ``HLSL6``, ``HLSL6VK`` and ``HLSL`` support ray tracing.
    The GLSL, ESSL and VKSL targets and ``HLSL5`` reject a source that uses
    any of it, also in code the entry point doesn't reach: ``ray tracing is
    not supported by output target 'HLSL5'``.

Targets with less
    Output targets registered by other backends may support only part of ray
    tracing. Each part a target lacks is rejected when the selected entry
    point reaches it, so other entries of the same source still compile:

    - ray tracing pipelines as a whole, leaving only ``RayTraceClosest`` and
      ``RayTraceAny``: ``ray tracing pipelines (...) are not supported by
      output target '...'``;
    - callable shaders and ``CallShader``: ``callable programs (...) are not
      supported by output target '...'``;
    - intersection shaders and ``ReportHit``: ``intersection programs (...)
      are not supported by output target '...'``;
    - ``TraceRay`` outside ray generation shaders: ``TraceRay can only be
      called from ray generation programs on output target '...', which
      doesn't support recursion``;
    - ``RAY_FLAG_SKIP_TRIANGLES`` in a constant ``TraceRay`` flags argument:
      ``ray flag RAY_FLAG_SKIP_TRIANGLES can't be used with 'TraceRay' on
      output target '...'``. Such a target ignores the flag when it is not a
      constant.

    A target whose traversal can't end from an any hit shader implements
    ``AcceptHitAndEndSearch`` as accepting the hit, and traversal continues:
    a closer hit can still replace it, so the closest hit shader may see a
    nearer hit than on ``HLSL6``.

Not supported
    - ``RayQuery`` in the source. A callback can only accept or ignore a
      candidate, so there is no counterpart to ``RayQuery::Abort``.
    - ``ObjectToWorld()`` and ``WorldToObject()`` without the ``3x4`` or
      ``4x3`` suffix.
    - What later shader models added to ray tracing, such as the ray flags
      introduced after Shader Model 6.6. The names are undeclared
      identifiers.
    - ``RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH``,
      ``RAY_FLAG_SKIP_CLOSEST_HIT_SHADER`` and ``RAY_FLAG_SKIP_TRIANGLES``
      with the inline built-ins.
    - Attribute structs larger than 32 bytes.
    - Payload access qualifiers: ``[raypayload]`` on a struct, and ``read``
      and ``write`` on its members.
    - Pipeline setup in HLSL, such as ``TriangleHitGroup`` and
      ``RaytracingShaderConfig``. The application composes the pipeline from
      separately compiled entry points.

Not checked
    - That the payload passed to ``TraceRay`` is the struct that the hit and
      miss shaders of the pipeline take, and that the parameter passed to
      ``CallShader`` matches the callable shader. The application pairs the
      shaders, so the compiler can't see it.
    - How deep ``TraceRay`` calls nest at run time.
    - Ray flags that are not constants.

Reflection
    The size of a callable shader's parameter struct is not reported. The C
    and C# wrappers have the six ``ShaderTarget`` values, but not the
    reflection members described above.
