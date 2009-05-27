/* ------------------------------------------------------------------
 * Copyright (C) 2008 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
/****************************************************************************************
Portions of this file are derived from the following 3GPP standard:

    3GPP TS 26.073
    ANSI-C code for the Adaptive Multi-Rate (AMR) speech codec
    Available from http://www.3gpp.org

(C) 2004, 3GPP Organizational Partners (ARIB, ATIS, CCSA, ETSI, TTA, TTC)
Permission to distribute, modify and use this file under the standard license
terms listed above has been obtained from the copyright holder.
****************************************************************************************/
/*



 Filename: /audio/gsm_amr/c/src/qgain475_tab.c

     Date: 12/09/2002

------------------------------------------------------------------------------
 REVISION HISTORY

 Description: Created this file from the reference, qgain475.tab.

 Description: Added #ifdef __cplusplus and removed "extern" from table
              definition.

 Description: Put "extern" back.

 Description:

------------------------------------------------------------------------------
 MODULE DESCRIPTION

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; INCLUDES
----------------------------------------------------------------------------*/
#include "typedef.h"

/*--------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif

    /*----------------------------------------------------------------------------
    ; MACROS
    ; [Define module specific macros here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; DEFINES
    ; [Include all pre-processor statements here. Include conditional
    ; compile variables also.]
    ----------------------------------------------------------------------------*/
#define MR475_VQ_SIZE 256

    /*----------------------------------------------------------------------------
    ; LOCAL FUNCTION DEFINITIONS
    ; [List function prototypes here]
    ----------------------------------------------------------------------------*/

    /*----------------------------------------------------------------------------
    ; LOCAL VARIABLE DEFINITIONS
    ; [Variable declaration - defined here and used outside this module]
    ----------------------------------------------------------------------------*/

    /* The table contains the following data:
     *
     *    g_pitch(0)        (Q14) // for sub-
     *    g_fac(0)          (Q12) // frame 0 and 2
     *    g_pitch(1)        (Q14) // for sub-
     *    g_fac(2)          (Q12) // frame 1 and 3
     *
     */
    extern const Word16 table_gain_MR475[MR475_VQ_SIZE*4] =
    {
        /*g_pit(0), g_fac(0),      g_pit(1), g_fac(1) */
        812,          128,           542,      140,
        2873,         1135,          2266,     3402,
        2067,          563,         12677,      647,
        4132,         1798,          5601,     5285,
        7689,          374,          3735,      441,
        10912,         2638,         11807,     2494,
        20490,          797,          5218,      675,
        6724,         8354,          5282,     1696,
        1488,          428,          5882,      452,
        5332,         4072,          3583,     1268,
        2469,          901,         15894,     1005,
        14982,         3271,         10331,     4858,
        3635,         2021,          2596,      835,
        12360,         4892,         12206,     1704,
        13432,         1604,          9118,     2341,
        3968,         1538,          5479,     9936,
        3795,          417,          1359,      414,
        3640,         1569,          7995,     3541,
        11405,          645,          8552,      635,
        4056,         1377,         16608,     6124,
        11420,          700,          2007,      607,
        12415,         1578,         11119,     4654,
        13680,         1708,         11990,     1229,
        7996,         7297,         13231,     5715,
        2428,         1159,          2073,     1941,
        6218,         6121,          3546,     1804,
        8925,         1802,          8679,     1580,
        13935,         3576,         13313,     6237,
        6142,         1130,          5994,     1734,
        14141,         4662,         11271,     3321,
        12226,         1551,         13931,     3015,
        5081,        10464,          9444,     6706,
        1689,          683,          1436,     1306,
        7212,         3933,          4082,     2713,
        7793,          704,         15070,      802,
        6299,         5212,          4337,     5357,
        6676,          541,          6062,      626,
        13651,         3700,         11498,     2408,
        16156,          716,         12177,      751,
        8065,        11489,          6314,     2256,
        4466,          496,          7293,      523,
        10213,         3833,          8394,     3037,
        8403,          966,         14228,     1880,
        8703,         5409,         16395,     4863,
        7420,         1979,          6089,     1230,
        9371,         4398,         14558,     3363,
        13559,         2873,         13163,     1465,
        5534,         1678,         13138,    14771,
        7338,          600,          1318,      548,
        4252,         3539,         10044,     2364,
        10587,          622,         13088,      669,
        14126,         3526,          5039,     9784,
        15338,          619,          3115,      590,
        16442,         3013,         15542,     4168,
        15537,         1611,         15405,     1228,
        16023,         9299,          7534,     4976,
        1990,         1213,         11447,     1157,
        12512,         5519,          9475,     2644,
        7716,         2034,         13280,     2239,
        16011,         5093,          8066,     6761,
        10083,         1413,          5002,     2347,
        12523,         5975,         15126,     2899,
        18264,         2289,         15827,     2527,
        16265,        10254,         14651,    11319,
        1797,          337,          3115,      397,
        3510,         2928,          4592,     2670,
        7519,          628,         11415,      656,
        5946,         2435,          6544,     7367,
        8238,          829,          4000,      863,
        10032,         2492,         16057,     3551,
        18204,         1054,          6103,     1454,
        5884,         7900,         18752,     3468,
        1864,          544,          9198,      683,
        11623,         4160,          4594,     1644,
        3158,         1157,         15953,     2560,
        12349,         3733,         17420,     5260,
        6106,         2004,          2917,     1742,
        16467,         5257,         16787,     1680,
        17205,         1759,          4773,     3231,
        7386,         6035,         14342,    10012,
        4035,          442,          4194,      458,
        9214,         2242,          7427,     4217,
        12860,          801,         11186,      825,
        12648,         2084,         12956,     6554,
        9505,          996,          6629,      985,
        10537,         2502,         15289,     5006,
        12602,         2055,         15484,     1653,
        16194,         6921,         14231,     5790,
        2626,          828,          5615,     1686,
        13663,         5778,          3668,     1554,
        11313,         2633,          9770,     1459,
        14003,         4733,         15897,     6291,
        6278,         1870,          7910,     2285,
        16978,         4571,         16576,     3849,
        15248,         2311,         16023,     3244,
        14459,        17808,         11847,     2763,
        1981,         1407,          1400,      876,
        4335,         3547,          4391,     4210,
        5405,          680,         17461,      781,
        6501,         5118,          8091,     7677,
        7355,          794,          8333,     1182,
        15041,         3160,         14928,     3039,
        20421,          880,         14545,      852,
        12337,        14708,          6904,     1920,
        4225,          933,          8218,     1087,
        10659,         4084,         10082,     4533,
        2735,          840,         20657,     1081,
        16711,         5966,         15873,     4578,
        10871,         2574,          3773,     1166,
        14519,         4044,         20699,     2627,
        15219,         2734,         15274,     2186,
        6257,         3226,         13125,    19480,
        7196,          930,          2462,     1618,
        4515,         3092,         13852,     4277,
        10460,          833,         17339,      810,
        16891,         2289,         15546,     8217,
        13603,         1684,          3197,     1834,
        15948,         2820,         15812,     5327,
        17006,         2438,         16788,     1326,
        15671,         8156,         11726,     8556,
        3762,         2053,          9563,     1317,
        13561,         6790,         12227,     1936,
        8180,         3550,         13287,     1778,
        16299,         6599,         16291,     7758,
        8521,         2551,          7225,     2645,
        18269,         7489,         16885,     2248,
        17882,         2884,         17265,     3328,
        9417,        20162,         11042,     8320,
        1286,          620,          1431,      583,
        5993,         2289,          3978,     3626,
        5144,          752,         13409,      830,
        5553,         2860,         11764,     5908,
        10737,          560,          5446,      564,
        13321,         3008,         11946,     3683,
        19887,          798,          9825,      728,
        13663,         8748,          7391,     3053,
        2515,          778,          6050,      833,
        6469,         5074,          8305,     2463,
        6141,         1865,         15308,     1262,
        14408,         4547,         13663,     4515,
        3137,         2983,          2479,     1259,
        15088,         4647,         15382,     2607,
        14492,         2392,         12462,     2537,
        7539,         2949,         12909,    12060,
        5468,          684,          3141,      722,
        5081,         1274,         12732,     4200,
        15302,          681,          7819,      592,
        6534,         2021,         16478,     8737,
        13364,          882,          5397,      899,
        14656,         2178,         14741,     4227,
        14270,         1298,         13929,     2029,
        15477,         7482,         15815,     4572,
        2521,         2013,          5062,     1804,
        5159,         6582,          7130,     3597,
        10920,         1611,         11729,     1708,
        16903,         3455,         16268,     6640,
        9306,         1007,          9369,     2106,
        19182,         5037,         12441,     4269,
        15919,         1332,         15357,     3512,
        11898,        14141,         16101,     6854,
        2010,          737,          3779,      861,
        11454,         2880,          3564,     3540,
        9057,         1241,         12391,      896,
        8546,         4629,         11561,     5776,
        8129,          589,          8218,      588,
        18728,         3755,         12973,     3149,
        15729,          758,         16634,      754,
        15222,        11138,         15871,     2208,
        4673,          610,         10218,      678,
        15257,         4146,          5729,     3327,
        8377,         1670,         19862,     2321,
        15450,         5511,         14054,     5481,
        5728,         2888,          7580,     1346,
        14384,         5325,         16236,     3950,
        15118,         3744,         15306,     1435,
        14597,         4070,         12301,    15696,
        7617,         1699,          2170,      884,
        4459,         4567,         18094,     3306,
        12742,          815,         14926,      907,
        15016,         4281,         15518,     8368,
        17994,         1087,          2358,      865,
        16281,         3787,         15679,     4596,
        16356,         1534,         16584,     2210,
        16833,         9697,         15929,     4513,
        3277,         1085,          9643,     2187,
        11973,         6068,          9199,     4462,
        8955,         1629,         10289,     3062,
        16481,         5155,         15466,     7066,
        13678,         2543,          5273,     2277,
        16746,         6213,         16655,     3408,
        20304,         3363,         18688,     1985,
        14172,        12867,         15154,    15703,
        4473,         1020,          1681,      886,
        4311,         4301,          8952,     3657,
        5893,         1147,         11647,     1452,
        15886,         2227,          4582,     6644,
        6929,         1205,          6220,      799,
        12415,         3409,         15968,     3877,
        19859,         2109,          9689,     2141,
        14742,         8830,         14480,     2599,
        1817,         1238,          7771,      813,
        19079,         4410,          5554,     2064,
        3687,         2844,         17435,     2256,
        16697,         4486,         16199,     5388,
        8028,         2763,          3405,     2119,
        17426,         5477,         13698,     2786,
        19879,         2720,          9098,     3880,
        18172,         4833,         17336,    12207,
        5116,          996,          4935,      988,
        9888,         3081,          6014,     5371,
        15881,         1667,          8405,     1183,
        15087,         2366,         19777,     7002,
        11963,         1562,          7279,     1128,
        16859,         1532,         15762,     5381,
        14708,         2065,         20105,     2155,
        17158,         8245,         17911,     6318,
        5467,         1504,          4100,     2574,
        17421,         6810,          5673,     2888,
        16636,         3382,          8975,     1831,
        20159,         4737,         19550,     7294,
        6658,         2781,         11472,     3321,
        19397,         5054,         18878,     4722,
        16439,         2373,         20430,     4386,
        11353,        26526,         11593,     3068,
        2866,         1566,          5108,     1070,
        9614,         4915,          4939,     3536,
        7541,          878,         20717,      851,
        6938,         4395,         16799,     7733,
        10137,         1019,          9845,      964,
        15494,         3955,         15459,     3430,
        18863,          982,         20120,      963,
        16876,        12887,         14334,     4200,
        6599,         1220,          9222,      814,
        16942,         5134,          5661,     4898,
        5488,         1798,         20258,     3962,
        17005,         6178,         17929,     5929,
        9365,         3420,          7474,     1971,
        19537,         5177,         19003,     3006,
        16454,         3788,         16070,     2367,
        8664,         2743,          9445,    26358,
        10856,         1287,          3555,     1009,
        5606,         3622,         19453,     5512,
        12453,          797,         20634,      911,
        15427,         3066,         17037,    10275,
        18883,         2633,          3913,     1268,
        19519,         3371,         18052,     5230,
        19291,         1678,         19508,     3172,
        18072,        10754,         16625,     6845,
        3134,         2298,         10869,     2437,
        15580,         6913,         12597,     3381,
        11116,         3297,         16762,     2424,
        18853,         6715,         17171,     9887,
        12743,         2605,          8937,     3140,
        19033,         7764,         18347,     3880,
        20475,         3682,         19602,     3380,
        13044,        19373,         10526,    23124
    };


    /*--------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

/*
------------------------------------------------------------------------------
 FUNCTION NAME:
------------------------------------------------------------------------------
 INPUT AND OUTPUT DEFINITIONS

 Inputs:
	None

 Outputs:
	None

 Returns:
	None

 Global Variables Used:
    None

 Local Variables Needed:
    None

------------------------------------------------------------------------------
 FUNCTION DESCRIPTION

 None

------------------------------------------------------------------------------
 REQUIREMENTS

 None

------------------------------------------------------------------------------
 REFERENCES

 [1] qua_gain.tab,  UMTS GSM AMR speech codec,
                    R99 - Version 3.2.0, March 2, 2001

------------------------------------------------------------------------------
 PSEUDO-CODE


------------------------------------------------------------------------------
 RESOURCES USED [optional]

 When the code is written for a specific target processor the
 the resources used should be documented below.

 HEAP MEMORY USED: x bytes

 STACK MEMORY USED: x bytes

 CLOCK CYCLES: (cycle count equation for this function) + (variable
                used to represent cycle count for each subroutine
                called)
     where: (cycle count variable) = cycle count for [subroutine
                                     name]

------------------------------------------------------------------------------
 CAUTION [optional]
 [State any special notes, constraints or cautions for users of this function]

------------------------------------------------------------------------------
*/

/*----------------------------------------------------------------------------
; FUNCTION CODE
----------------------------------------------------------------------------*/







