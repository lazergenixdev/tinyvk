// Header-only library, not designed to be useful to anyone, see license below.
//
// Functions:
//    tric_memory_requirements
//    tric_triangulate
//
#ifndef TRIANGULATE_CIRCLE_H
#define TRIANGULATE_CIRCLE_H

#include <stdint.h>

#ifndef TRIC_ASSERT
#define TRIC_ASSERT assert
#include <assert.h>
#endif

#ifndef TRIC_SINCOS
#define TRIC_SINCOS tric_sincos
#include <math.h>
static inline void tric_sincos(float x, float *sin, float *cos)
{
    *cos = cosf(x);
    *sin = sinf(x);
}
#endif

typedef enum {
    tric_Method_Naive,
    tric_Method_Fan,
    tric_Method_Strip,
    tric_Method_Quad,
    tric_Method_Max_Area,
    tric_Method_COUNT,
} tric_Method;

typedef struct {
    void     *Vertices;
    uint32_t  VertexCount;
    uint32_t  VertexStride;
    uint32_t *Indices;
    uint32_t  IndexCount;
} tric_Buffers;

typedef struct {
    float x, y;
} tric_Point;

// Get `VertexCount` and `IndexCount` required for method and level of detail (LOD).
// NOTE: `VertexStride` defaults to `sizeof(float) * 2` 
static inline void tric_memory_requirements(tric_Method method, int lod, tric_Buffers *buffers)
{
    TRIC_ASSERT(lod >= 0 && lod < 32);
    TRIC_ASSERT(buffers);

    buffers->VertexCount = 3 * (1 << lod);
    buffers->VertexStride = sizeof(float) * 2;

    switch (method)
    {
        case tric_Method_Fan:
        {
            // Every outer vertex is the edge of a unique triangle.
            buffers->IndexCount = 3 * buffers->VertexCount;
            // Add center vertex
            buffers->VertexCount += 1;
        }
        break;

        case tric_Method_Naive:
        case tric_Method_Strip:
        case tric_Method_Max_Area:
        {
            buffers->IndexCount = 3 * (buffers->VertexCount - 2);
        }
        break;

        case tric_Method_Quad:
        {
            // Needs to be multiple of 4, hard-coded for 0 and 1
            switch (lod) {
                case 0:
                    buffers->VertexCount = 4;
                break;
                case 1:
                    buffers->VertexCount = 8;
                break;
                default:
                break;
            }

            buffers->IndexCount = 3 * (buffers->VertexCount - 2);
        }
        break;

        default:
        case tric_Method_COUNT:
        {
            TRIC_ASSERT(0 && "Method does not exist");
        }
    }
}

// Fill `Vertices` and `Indices` buffers with mesh data.
static inline void tric_triangulate(tric_Method method, int lod, const tric_Buffers *buffers)
{
    TRIC_ASSERT(lod >= 0 && lod < 32);
    TRIC_ASSERT(buffers->VertexCount > 0);
    TRIC_ASSERT(buffers->VertexStride > 0);
    TRIC_ASSERT(buffers->IndexCount > 0);
    TRIC_ASSERT(buffers->Vertices);
    TRIC_ASSERT(buffers->Indices);

    uint8_t *vertex_bytes = (uint8_t*)(buffers->Vertices);

    switch (method)
    {
        case tric_Method_Naive:
        {
            uint32_t n = buffers->VertexCount;

            // Outer points
            float sint, cost;
            for (uint32_t i = 0; i < n; ++i)
            {
                tric_Point *position = (tric_Point*)(vertex_bytes + buffers->VertexStride * i);

                float theta = (6.28318530718f) * (float)(i) / (float)(n);
                TRIC_SINCOS(theta, &sint, &cost);
                position->x = cost;
                position->y = sint;
            }

            uint32_t c = 0;

            // Triangles
            for (uint32_t i = 1; i < n - 1; ++i)
            {
                buffers->Indices[c++] = (i + 0) % n;
                buffers->Indices[c++] = (i + 1) % n;
                buffers->Indices[c++] = 0;
            }
        }
        break;

        case tric_Method_Fan:
        {
            uint32_t n = buffers->VertexCount - 1;

            // Outer points
            float sint, cost;
            for (uint32_t i = 0; i < n; ++i)
            {
                tric_Point *position = (tric_Point*)(vertex_bytes + buffers->VertexStride * i);

                float theta = (6.28318530718f) * (float)(i) / (float)(n);
                TRIC_SINCOS(theta, &sint, &cost);
                position->x = cost;
                position->y = sint;
            }

            // Center point
            *(tric_Point*)(vertex_bytes + buffers->VertexStride * n) = (tric_Point){0,0};

            // Triangles
            for (uint32_t i = 0; i < n; ++i)
            {
                buffers->Indices[i*3 + 0] = (i + 0) % n;
                buffers->Indices[i*3 + 1] = (i + 1) % n;
                buffers->Indices[i*3 + 2] = n;
            }
        }
        break;

        case tric_Method_Strip:
        {
            uint32_t n = buffers->VertexCount;

            // Outer points
            float sint, cost;
            *(tric_Point*)(vertex_bytes) = (tric_Point){1.0f, 0.0f};
            for (uint32_t i = 1; i < n; ++i)
            {
                tric_Point *position = (tric_Point*)(vertex_bytes + buffers->VertexStride * i);
                uint32_t j = (i % 2 == 0) ? (i + 1)/2 : n - (i + 1)/2;

                float theta = (6.28318530718f) * (float)(j) / (float)(n);
                TRIC_SINCOS(theta, &sint, &cost);
                position->x = cost;
                position->y = sint;
            }

            // Triangles
            for (uint32_t i = 0; i < n - 2; ++i)
            {
                buffers->Indices[i*3 + 0] = (i + 0) % n;
                buffers->Indices[i*3 + 1] = (i + 1) % n;
                buffers->Indices[i*3 + 2] = (i + 2) % n;
            }
        }
        break;

        case tric_Method_Max_Area:
        {
            uint32_t n = buffers->VertexCount;

            // Outer points
            float sint, cost;
            for (uint32_t i = 0; i < n; ++i)
            {
                tric_Point *position = (tric_Point*)(vertex_bytes + buffers->VertexStride * i);

                float theta = (6.28318530718f) * (float)(i) / (float)(n);
                TRIC_SINCOS(theta, &sint, &cost);
                position->x = cost;
                position->y = sint;
            }

            // Triangles
            uint32_t i = 0;
            for (uint32_t skip = (1 << lod); skip > 0; skip /= 2)
            {
                for (uint32_t base = 0; base + skip * 2 <= n; base += skip * 2)
                {
                    buffers->Indices[i++] = (base + skip * 0);
                    buffers->Indices[i++] = (base + skip * 1);
                    buffers->Indices[i++] = (base + skip * 2) % n;
                }
                // 1 +3 +6 +12 + 24
                // 1 4 10 
            }
        }
        break;

        case tric_Method_Quad:
        {
            uint32_t n = buffers->VertexCount;

            // Outer points
            float sint, cost;
            for (uint32_t i = 0; i < n; ++i)
            {
                tric_Point *position = (tric_Point*)(vertex_bytes + buffers->VertexStride * i);

                float theta = (6.28318530718f) * (float)(i) / (float)(n);
                TRIC_SINCOS(theta, &sint, &cost);
                position->x = cost;
                position->y = sint;
            }

            uint32_t i = 0;

            // Center Quad (LOD=0)
            buffers->Indices[i++] = (n/4) * 0;
            buffers->Indices[i++] = (n/4) * 1;
            buffers->Indices[i++] = (n/4) * 2;
            buffers->Indices[i++] = (n/4) * 2;
            buffers->Indices[i++] = (n/4) * 3;
            buffers->Indices[i++] = (n/4) * 0;

            // Triangles
            if (lod > 0) {
                uint32_t c = (n - 4) / 4; // Triangles per quad side
                for (uint32_t s = 0; s < 4; ++s)
                {
                    for (uint32_t k = 0; k < c; ++k)
                    {
                        buffers->Indices[i++] = s*(n/4) + k + 1;
                        buffers->Indices[i++] = (s*(n/4) + k + 2) % n;
                        buffers->Indices[i++] = s*(n/4);
                    }
                }
            }
        }
        break;

        default:
        case tric_Method_COUNT:
        {
            TRIC_ASSERT(0 && "Method does not exist");
        }
    }
}

#endif // TRIANGULATE_CIRCLE_H

/*---------------------------------------------------------------------------------
    MIT License

    Copyright (c) 2026 Mitchell Bizzigotti

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
---------------------------------------------------------------------------------*/
