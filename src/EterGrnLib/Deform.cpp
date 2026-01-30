#include "Deform.h"
#include <xmmintrin.h>
#include <emmintrin.h>

void DeformPWNT3432toGrannyPNGBT33332D(granny_int32x Count, void const* SourceInit, void* DestInit,
    granny_matrix_4x4 const* Transforms,
    granny_int32x CopySize, granny_int32x SourceStride, granny_int32x DestStride)
{
    const float inv255 = 1.0f / 255.0f;

    const granny_pwnt3432_vertex* src = (const granny_pwnt3432_vertex*)SourceInit;
    granny_pnt332_vertex* dst = (granny_pnt332_vertex*)DestInit;

    while (Count--) {
        const __m128 srcPos = _mm_set_ps(1.0f, src->Position[2], src->Position[1], src->Position[0]);
        const __m128 srcNrm = _mm_set_ps(0.0f, src->Normal[2], src->Normal[1], src->Normal[0]);

        const __m128 px = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 py = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 pz = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 pw = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(3, 3, 3, 3));

        const __m128 nx = _mm_shuffle_ps(srcNrm, srcNrm, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 ny = _mm_shuffle_ps(srcNrm, srcNrm, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 nz = _mm_shuffle_ps(srcNrm, srcNrm, _MM_SHUFFLE(2, 2, 2, 2));

        __m128 P = _mm_setzero_ps();
        __m128 N = _mm_setzero_ps();

        for (int i = 0; i < 4; ++i) {
            const int   bi = src->BoneIndices[i];
            const float wS = (float)src->BoneWeights[i] * inv255;
            if (wS <= 0.0f) continue;

            const float* m = (const float*)(&Transforms[bi]);

            const __m128 r0 = _mm_loadu_ps(m + 0);
            const __m128 r1 = _mm_loadu_ps(m + 4);
            const __m128 r2 = _mm_loadu_ps(m + 8);
            const __m128 r3 = _mm_loadu_ps(m + 12);

            __m128 p = _mm_add_ps(_mm_mul_ps(r0, px), _mm_mul_ps(r1, py));
            p = _mm_add_ps(p, _mm_mul_ps(r2, pz));
            p = _mm_add_ps(p, _mm_mul_ps(r3, pw));

            const __m128 w = _mm_set1_ps(wS);
            P = _mm_add_ps(P, _mm_mul_ps(p, w));

            __m128 n = _mm_add_ps(_mm_mul_ps(r0, nx), _mm_mul_ps(r1, ny));
            n = _mm_add_ps(n, _mm_mul_ps(r2, nz));
            N = _mm_add_ps(N, _mm_mul_ps(n, w));
        }

        float pOut[4], nOut[4];
        _mm_storeu_ps(pOut, P);
        _mm_storeu_ps(nOut, N);

        dst->Position[0] = pOut[0];
        dst->Position[1] = pOut[1];
        dst->Position[2] = pOut[2];

        dst->Normal[0] = nOut[0];
        dst->Normal[1] = nOut[1];
        dst->Normal[2] = nOut[2];

        dst->UV[0] = src->UV[0];
        dst->UV[1] = src->UV[1];

        src = (const granny_pwnt3432_vertex*)((const granny_uint8*)src + SourceStride);
        dst = (granny_pnt332_vertex*)((granny_uint8*)dst + DestStride);
    }
}

void DeformPWNT3432toGrannyPNGBT33332I(granny_int32x Count, void const* SourceInit, void* DestInit,
    granny_int32x const* TransformTable, granny_matrix_4x4 const* Transforms,
    granny_int32x CopySize, granny_int32x SourceStride, granny_int32x DestStride)
{
    const float inv255 = 1.0f / 255.0f;

    const granny_pwnt3432_vertex* src = (const granny_pwnt3432_vertex*)SourceInit;
    granny_pnt332_vertex* dst = (granny_pnt332_vertex*)DestInit;

    while (Count--) {
        const __m128 srcPos = _mm_set_ps(1.0f, src->Position[2], src->Position[1], src->Position[0]);
        const __m128 srcNrm = _mm_set_ps(0.0f, src->Normal[2], src->Normal[1], src->Normal[0]);

        const __m128 px = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 py = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 pz = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(2, 2, 2, 2));
        const __m128 pw = _mm_shuffle_ps(srcPos, srcPos, _MM_SHUFFLE(3, 3, 3, 3));

        const __m128 nx = _mm_shuffle_ps(srcNrm, srcNrm, _MM_SHUFFLE(0, 0, 0, 0));
        const __m128 ny = _mm_shuffle_ps(srcNrm, srcNrm, _MM_SHUFFLE(1, 1, 1, 1));
        const __m128 nz = _mm_shuffle_ps(srcNrm, srcNrm, _MM_SHUFFLE(2, 2, 2, 2));

        __m128 P = _mm_setzero_ps();
        __m128 N = _mm_setzero_ps();

        for (int i = 0; i < 4; ++i) {
            const int   bi = TransformTable[src->BoneIndices[i]];
            const float wS = (float)src->BoneWeights[i] * inv255;
            if (wS <= 0.0f) continue;

            const float* m = (const float*)(&Transforms[bi]);

            const __m128 r0 = _mm_loadu_ps(m + 0);
            const __m128 r1 = _mm_loadu_ps(m + 4);
            const __m128 r2 = _mm_loadu_ps(m + 8);
            const __m128 r3 = _mm_loadu_ps(m + 12);

            __m128 p = _mm_add_ps(_mm_mul_ps(r0, px), _mm_mul_ps(r1, py));
            p = _mm_add_ps(p, _mm_mul_ps(r2, pz));
            p = _mm_add_ps(p, _mm_mul_ps(r3, pw));

            const __m128 w = _mm_set1_ps(wS);
            P = _mm_add_ps(P, _mm_mul_ps(p, w));

            __m128 n = _mm_add_ps(_mm_mul_ps(r0, nx), _mm_mul_ps(r1, ny));
            n = _mm_add_ps(n, _mm_mul_ps(r2, nz));
            N = _mm_add_ps(N, _mm_mul_ps(n, w));
        }

        float pOut[4], nOut[4];
        _mm_storeu_ps(pOut, P);
        _mm_storeu_ps(nOut, N);

        dst->Position[0] = pOut[0];
        dst->Position[1] = pOut[1];
        dst->Position[2] = pOut[2];

        dst->Normal[0] = nOut[0];
        dst->Normal[1] = nOut[1];
        dst->Normal[2] = nOut[2];

        dst->UV[0] = src->UV[0];
        dst->UV[1] = src->UV[1];

        src = (const granny_pwnt3432_vertex*)((const granny_uint8*)src + SourceStride);
        dst = (granny_pnt332_vertex*)((granny_uint8*)dst + DestStride);
    }
}

void DeformPWNT3432toGrannyPNGBT33332(granny_int32x Count, void const* SourceInit, void* DestInit,
	granny_int32x const* TransformTable, granny_matrix_4x4 const* Transforms,
	granny_int32x CopySize, granny_int32x SourceStride, granny_int32x DestStride)
{
	if (TransformTable) [[likely]] {
		DeformPWNT3432toGrannyPNGBT33332I(Count, SourceInit, DestInit, TransformTable, Transforms, CopySize, SourceStride, DestStride);
	}
	else [[unlikely]] {
		DeformPWNT3432toGrannyPNGBT33332D(Count, SourceInit, DestInit, Transforms, CopySize, SourceStride, DestStride);
	}
}
