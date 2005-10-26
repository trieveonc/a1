#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/common.h"
#include "common/cpu.h"
#ifdef HAVE_MMXEXT
#include "common/i386/pixel.h"
#include "common/i386/dct.h"
#include "common/i386/mc.h"
#endif
#ifdef ARCH_PPC
#include "common/ppc/pixel.h"
#include "common/ppc/mc.h"
#endif

/* buf1, buf2: initialised to randome data and shouldn't write into them */
uint8_t * buf1, * buf2;
/* buf3, buf4: used to store output */
uint8_t * buf3, * buf4;
/* buf5: temp */
uint8_t * buf5;

#define report( name ) { \
    if( used_asm ) \
        fprintf( stderr, " - %-21s [%s]\n", name, ok ? "OK" : "FAILED" ); \
    if( !ok ) ret = -1; \
}

static int check_pixel( int cpu_ref, int cpu_new )
{
    x264_pixel_function_t pixel_c;
    x264_pixel_function_t pixel_ref;
    x264_pixel_function_t pixel_asm;
    int ret = 0, ok, used_asm;
    int i;

    x264_pixel_init( 0, &pixel_c );
    x264_pixel_init( cpu_ref, &pixel_ref );
    x264_pixel_init( cpu_new, &pixel_asm );

#define TEST_PIXEL( name ) \
    for( i = 0, ok = 1, used_asm = 0; i < 7; i++ ) \
    { \
        int res_c, res_asm; \
        if( pixel_asm.name[i] != pixel_ref.name[i] ) \
        { \
            used_asm = 1; \
            res_c   = pixel_c.name[i]( buf1, 32, buf2, 24 ); \
            res_asm = pixel_asm.name[i]( buf1, 32, buf2, 24 ); \
            if( res_c != res_asm ) \
            { \
                ok = 0; \
                fprintf( stderr, #name "[%d]: %d != %d [FAILED]\n", i, res_c, res_asm ); \
            } \
        } \
    } \
    report( "pixel " #name " :" );

    TEST_PIXEL( sad );
    TEST_PIXEL( ssd );
    TEST_PIXEL( satd );

    return ret;
}

static int check_dct( int cpu_ref, int cpu_new )
{
    x264_dct_function_t dct_c;
    x264_dct_function_t dct_ref;
    x264_dct_function_t dct_asm;
    int ret = 0, ok, used_asm;
    int16_t dct1[16][4][4] __attribute((aligned(16)));
    int16_t dct2[16][4][4] __attribute((aligned(16)));

    x264_dct_init( 0, &dct_c );
    x264_dct_init( cpu_ref, &dct_ref);
    x264_dct_init( cpu_new, &dct_asm );
#define TEST_DCT( name, t1, t2, size ) \
    if( dct_asm.name != dct_ref.name ) \
    { \
        used_asm = 1; \
        dct_c.name( t1, buf1, 32, buf2, 24 ); \
        dct_asm.name( t2, buf1, 32, buf2, 24 ); \
        if( memcmp( t1, t2, size ) ) \
        { \
            ok = 0; \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
    }
    ok = 1; used_asm = 0;
    TEST_DCT( sub4x4_dct, dct1[0], dct2[0], 16*2 );
    TEST_DCT( sub8x8_dct, dct1, dct2, 16*2*4 );
    TEST_DCT( sub16x16_dct, dct1, dct2, 16*2*16 );
    report( "sub_dct4 :" );

    ok = 1; used_asm = 0;
    TEST_DCT( sub8x8_dct8, (void*)dct1[0], (void*)dct2[0], 64*2 );
    TEST_DCT( sub16x16_dct8, (void*)dct1, (void*)dct2, 64*2*4 );
    report( "sub_dct8 :" );
#undef TEST_DCT

    /* copy coefs because idct8 modifies them in place */
    memcpy( buf5, dct1, 512 );

#define TEST_IDCT( name ) \
    if( dct_asm.name != dct_ref.name ) \
    { \
        used_asm = 1; \
        memcpy( buf3, buf1, 32*32 ); \
        memcpy( buf4, buf1, 32*32 ); \
        memcpy( dct1, buf5, 512 ); \
        memcpy( dct2, buf5, 512 ); \
        dct_c.name( buf3, 32, (void*)dct1 ); \
        dct_asm.name( buf4, 32, (void*)dct2 ); \
        if( memcmp( buf3, buf4, 32*32 ) ) \
        { \
            ok = 0; \
            fprintf( stderr, #name " [FAILED]\n" ); \
        } \
    }
    ok = 1; used_asm = 0;
    TEST_IDCT( add4x4_idct );
    TEST_IDCT( add8x8_idct );
    TEST_IDCT( add16x16_idct );
    report( "add_idct4 :" );

    ok = 1; used_asm = 0;
    TEST_IDCT( add8x8_idct8 );
    TEST_IDCT( add16x16_idct8 );
    report( "add_idct8 :" );
#undef TEST_IDCT

    ok = 1; used_asm = 0;
    if( dct_asm.dct4x4dc != dct_ref.dct4x4dc )
    {
        int16_t dct1[4][4] __attribute((aligned(16))) = { {-12, 42, 23, 67},{2, 90, 89,56}, {67,43,-76,91},{56,-78,-54,1}};
        int16_t dct2[4][4] __attribute((aligned(16))) = { {-12, 42, 23, 67},{2, 90, 89,56}, {67,43,-76,91},{56,-78,-54,1}};
        used_asm = 1;
        dct_c.dct4x4dc( dct1 );
        dct_asm.dct4x4dc( dct2 );
        if( memcmp( dct1, dct2, 32 ) )
        {
            ok = 0;
            fprintf( stderr, " - dct4x4dc :        [FAILED]\n" );
        }
    }
    if( dct_asm.dct4x4dc != dct_ref.dct4x4dc )
    {
        int16_t dct1[4][4] __attribute((aligned(16))) = { {-12, 42, 23, 67},{2, 90, 89,56}, {67,43,-76,91},{56,-78,-54,1}};
        int16_t dct2[4][4] __attribute((aligned(16))) = { {-12, 42, 23, 67},{2, 90, 89,56}, {67,43,-76,91},{56,-78,-54,1}};
        used_asm = 1;
        dct_c.idct4x4dc( dct1 );
        dct_asm.idct4x4dc( dct2 );
        if( memcmp( dct1, dct2, 32 ) )
        {
            ok = 0;
            fprintf( stderr, " - idct4x4dc :        [FAILED]\n" );
        }
    }
    report( "(i)dct4x4dc :" );

    ok = 1; used_asm = 0;
    if( dct_asm.dct2x2dc != dct_ref.dct2x2dc )
    {
        int16_t dct1[2][2] __attribute((aligned(16))) = { {-12, 42},{2, 90}};
        int16_t dct2[2][2] __attribute((aligned(16))) = { {-12, 42},{2, 90}};
        used_asm = 1;
        dct_c.dct2x2dc( dct1 );
        dct_asm.dct2x2dc( dct2 );
        if( memcmp( dct1, dct2, 4*2 ) )
        {
            ok = 0;
            fprintf( stderr, " - dct2x2dc :        [FAILED]\n" );
        }
    }
    if( dct_asm.idct2x2dc != dct_ref.idct2x2dc )
    {
        int16_t dct1[2][2] __attribute((aligned(16))) = { {-12, 42},{2, 90}};
        int16_t dct2[2][2] __attribute((aligned(16))) = { {-12, 42},{2, 90}};
        used_asm = 1;
        dct_c.idct2x2dc( dct1 );
        dct_asm.idct2x2dc( dct2 );
        if( memcmp( dct1, dct2, 4*2 ) )
        {
            ok = 0;
            fprintf( stderr, " - idct2x2dc :       [FAILED]\n" );
        }
    }
    report( "(i)dct2x2dc :" );

    return ret;
}

static int check_mc( int cpu_ref, int cpu_new )
{
    x264_mc_functions_t mc_c;
    x264_mc_functions_t mc_ref;
    x264_mc_functions_t mc_a;

    uint8_t *src     = &buf1[2*32+2];
    uint8_t *src2[4] = { &buf1[2*32+2],  &buf1[7*32+2],
                         &buf1[12*32+2], &buf1[17*32+2] };
    uint8_t *dst1    = &buf3[2*32+2];
    uint8_t *dst2    = &buf4[2*32+2];

    int dx, dy, i, w;
    int ret = 0, ok, used_asm;

    x264_mc_init( 0, &mc_c );
    x264_mc_init( cpu_ref, &mc_ref );
    x264_mc_init( cpu_new, &mc_a );

#define MC_TEST_LUMA( w, h ) \
        if( mc_a.mc_luma != mc_ref.mc_luma ) \
        { \
            used_asm = 1; \
            memset(buf3, 0xCD, 1024); \
            memset(buf4, 0xCD, 1024); \
            mc_c.mc_luma( src2, 32, dst1, 16, dx, dy, w, h );     \
            mc_a.mc_luma( src2, 32, dst2, 16, dx, dy, w, h );   \
            if( memcmp( buf3, buf4, 1024 ) )               \
            { \
                fprintf( stderr, "mc_luma[mv(%d,%d) %2dx%-2d]     [FAILED]\n", dx, dy, w, h );   \
                ok = 0; \
            } \
        }

#define MC_TEST_CHROMA( w, h ) \
        if( mc_a.mc_chroma != mc_ref.mc_chroma ) \
        { \
            used_asm = 1; \
            memset(buf3, 0xCD, 1024); \
            memset(buf4, 0xCD, 1024); \
            mc_c.mc_chroma( src, 32, dst1, 16, dx, dy, w, h );     \
            mc_a.mc_chroma( src, 32, dst2, 16, dx, dy, w, h );   \
            if( memcmp( buf3, buf4, 1024 ) )               \
            { \
                fprintf( stderr, "mc_chroma[mv(%d,%d) %2dx%-2d]     [FAILED]\n", dx, dy, w, h );   \
                ok = 0; \
            } \
        }
    ok = 1; used_asm = 0;
    for( dy = 0; dy < 4; dy++ )
        for( dx = 0; dx < 4; dx++ )
        {
            MC_TEST_LUMA( 16, 16 );
            MC_TEST_LUMA( 16, 8 );
            MC_TEST_LUMA( 8, 16 );
            MC_TEST_LUMA( 8, 8 );
            MC_TEST_LUMA( 8, 4 );
            MC_TEST_LUMA( 4, 8 );
            MC_TEST_LUMA( 4, 4 );
        }
    report( "mc luma :" );

    ok = 1; used_asm = 0;
    for( dy = 0; dy < 9; dy++ )
        for( dx = 0; dx < 9; dx++ )
        {
            MC_TEST_CHROMA( 8, 8 );
            MC_TEST_CHROMA( 8, 4 );
            MC_TEST_CHROMA( 4, 8 );
            MC_TEST_CHROMA( 4, 4 );
            MC_TEST_CHROMA( 4, 2 );
            MC_TEST_CHROMA( 2, 4 );
            MC_TEST_CHROMA( 2, 2 );
        }
    report( "mc chroma :" );
#undef MC_TEST_LUMA
#undef MC_TEST_CHROMA

#define MC_TEST_AVG( name, ... ) \
    for( i = 0, ok = 1, used_asm = 0; i < 10; i++ ) \
    { \
        memcpy( buf3, buf1, 1024 ); \
        memcpy( buf4, buf1, 1024 ); \
        if( mc_a.name[i] != mc_ref.name[i] ) \
        { \
            used_asm = 1; \
            mc_c.name[i]( buf3, 32, buf2, 24, ##__VA_ARGS__ ); \
            mc_a.name[i]( buf4, 32, buf2, 24, ##__VA_ARGS__ ); \
            if( memcmp( buf3, buf4, 1024 ) )               \
            { \
                ok = 0; \
                fprintf( stderr, #name "[%d]: [FAILED]\n", i ); \
            } \
        } \
    }
    MC_TEST_AVG( avg );
    report( "mc avg :" );
    for( w = -64; w <= 128 && ok; w++ )
        MC_TEST_AVG( avg_weight, w );
    report( "mc wpredb :" );

    return ret;
}

static int check_quant( int cpu_ref, int cpu_new )
{
    x264_quant_function_t qf_c;
    x264_quant_function_t qf_ref;
    x264_quant_function_t qf_a;
    int16_t dct1[64], dct2[64];
    uint8_t cqm_buf[64];
    int ret = 0, ok = 1, used_asm = 0;
    int i, i_cqm;
    x264_t h_buf;
    x264_t *h = &h_buf;
    h->pps = h->pps_array;
    x264_param_default( &h->param );

    for( i_cqm = 0; i_cqm < 4; i_cqm++ )
    {
        if( i_cqm == 0 )
            for( i = 0; i < 6; i++ )
                h->pps->scaling_list[i] = x264_cqm_flat16;
        else if( i_cqm == 1 )
            for( i = 0; i < 6; i++ )
                h->pps->scaling_list[i] = x264_cqm_jvt[i];
        else
        {
            if( i_cqm == 2 )
                for( i = 0; i < 64; i++ )
                    cqm_buf[i] = 10 + rand() % 246;
            else
                for( i = 0; i < 64; i++ )
                    cqm_buf[i] = 1;
            for( i = 0; i < 6; i++ )
                h->pps->scaling_list[i] = cqm_buf;
        }

        x264_cqm_init( h );
        x264_quant_init( h, 0, &qf_c );
        x264_quant_init( h, cpu_ref, &qf_ref );
        x264_quant_init( h, cpu_new, &qf_a );

#define TEST_QUANT( name, cqm ) \
        if( qf_a.name != qf_ref.name ) \
        { \
            used_asm = 1; \
            for( i = 0; i < 64; i++ ) \
                dct1[i] = dct2[i] = rand() & 0xfff; \
            qf_c.name( (void*)dct1, cqm, 20, (1<<20)/6 ); \
            qf_a.name( (void*)dct2, cqm, 20, (1<<20)/6 ); \
            if( memcmp( dct1, dct2, 64*2 ) )       \
            { \
                ok = 0; \
                fprintf( stderr, #name "(cqm=%d): [FAILED]\n", i_cqm ); \
            } \
        }

        TEST_QUANT( quant_8x8_core, *h->quant8_mf[CQM_8IY] );
        TEST_QUANT( quant_8x8_core, *h->quant8_mf[CQM_8PY] );
        TEST_QUANT( quant_4x4_core, *h->quant4_mf[CQM_4IY] );
        TEST_QUANT( quant_4x4_core, *h->quant4_mf[CQM_4PY] );
        TEST_QUANT( quant_4x4_dc_core, ***h->quant4_mf[CQM_4IY] );
        TEST_QUANT( quant_2x2_dc_core, ***h->quant4_mf[CQM_4IC] );
    }

    report( "quant :" );

    return ret;
}

int check_all( int cpu_ref, int cpu_new )
{
    return check_pixel( cpu_ref, cpu_new )
         + check_dct( cpu_ref, cpu_new )
         + check_mc( cpu_ref, cpu_new )
         + check_quant( cpu_ref, cpu_new );
}

int main()
{
    int ret = 0;
    int i;

    buf1 = x264_malloc( 1024 ); /* 32 x 32 */
    buf2 = x264_malloc( 1024 );
    buf3 = x264_malloc( 1024 );
    buf4 = x264_malloc( 1024 );
    buf5 = x264_malloc( 1024 );

    srand( x264_mdate() );

    for( i = 0; i < 1024; i++ )
    {
        buf1[i] = rand() & 0xFF;
        buf2[i] = rand() & 0xFF;
        buf3[i] = buf4[i] = 0;
    }

#ifdef HAVE_MMXEXT
    fprintf( stderr, "x264: MMXEXT against C\n" );
    ret = check_all( 0, X264_CPU_MMX | X264_CPU_MMXEXT );
#ifdef HAVE_SSE2
    if( x264_cpu_detect() & X264_CPU_SSE2 )
    {
        fprintf( stderr, "\nx264: SSE2 against C\n" );
        ret |= check_all( X264_CPU_MMX | X264_CPU_MMXEXT,
                          X264_CPU_MMX | X264_CPU_MMXEXT | X264_CPU_SSE | X264_CPU_SSE2 );
    }
#endif
#elif ARCH_PPC
    fprintf( stderr, "x264: ALTIVEC against C\n" );
    ret = check_all( 0, X264_CPU_ALTIVEC );
#endif

    if( ret == 0 )
    {
        fprintf( stderr, "x264: All tests passed Yeah :)\n" );
        return 0;
    }
    fprintf( stderr, "x264: at least one test has failed. Go and fix that Right Now!\n" );
    return -1;
}

