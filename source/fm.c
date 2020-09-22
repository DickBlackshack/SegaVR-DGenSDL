#define YM2610B_WARNING

/*
**
** File: fm.c -- software implementation of Yamaha FM sound generator
**
** Copyright (C) 2001, 2002, 2003 Jarek Burczynski (bujar at mame dot net)
** Copyright (C) 1998 Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 1.4 (final beta)
**
*/

/*
** History:
**
** 03-08-2003 Jarek Burczynski:
**  - fixed YM2608 initial values (after the reset)
**  - fixed flag and irqmask handling (YM2608)
**  - fixed BUFRDY flag handling (YM2608)
**
** 14-06-2003 Jarek Burczynski:
**  - implemented all of the YM2608 status register flags
**  - implemented support for external memory read/write via YM2608
**  - implemented support for deltat memory limit register in YM2608 emulation
**
** 22-05-2003 Jarek Burczynski:
**  - fixed LFO PM calculations (copy&paste bugfix)
**
** 08-05-2003 Jarek Burczynski:
**  - fixed SSG support
**
** 22-04-2003 Jarek Burczynski:
**  - implemented 100% correct LFO generator (verified on real YM2610 and YM2608)
**
** 15-04-2003 Jarek Burczynski:
**  - added support for YM2608's register 0x110 - status mask
**
** 01-12-2002 Jarek Burczynski:
**  - fixed register addressing in YM2608, YM2610, YM2610B chips. (verified on real YM2608)
**    The addressing patch used for early Neo-Geo games can be removed now.
**
** 26-11-2002 Jarek Burczynski, Nicola Salmoria:
**  - recreated YM2608 ADPCM ROM using data from real YM2608's output which leads to:
**  - added emulation of YM2608 drums.
**  - output of YM2608 is two times lower now - same as YM2610 (verified on real YM2608)
**
** 16-08-2002 Jarek Burczynski:
**  - binary exact Envelope Generator (verified on real YM2203);
**    identical to YM2151
**  - corrected 'off by one' error in feedback calculations (when feedback is off)
**  - corrected connection (algorithm) calculation (verified on real YM2203 and YM2610)
**
** 18-12-2001 Jarek Burczynski:
**  - added SSG-EG support (verified on real YM2203)
**
** 12-08-2001 Jarek Burczynski:
**  - corrected sin_tab and tl_tab data (verified on real chip)
**  - corrected feedback calculations (verified on real chip)
**  - corrected phase generator calculations (verified on real chip)
**  - corrected envelope generator calculations (verified on real chip)
**  - corrected FM volume level (YM2610 and YM2610B).
**  - changed YMxxxUpdateOne() functions (YM2203, YM2608, YM2610, YM2610B, YM2612) :
**    this was needed to calculate YM2610 FM channels output correctly.
**    (Each FM channel is calculated as in other chips, but the output of the channel
**    gets shifted right by one *before* sending to accumulator. That was impossible to do
**    with previous implementation).
**
** 23-07-2001 Jarek Burczynski, Nicola Salmoria:
**  - corrected YM2610 ADPCM type A algorithm and tables (verified on real chip)
**
** 11-06-2001 Jarek Burczynski:
**  - corrected end of sample bug in ADPCMA_calc_cha().
**    Real YM2610 checks for equality between current and end addresses (only 20 LSB bits).
**
** 08-12-98 hiro-shi:
** rename ADPCMA -> ADPCMB, ADPCMB -> ADPCMA
** move ROM limit check.(CALC_CH? -> 2610Write1/2)
** test program (ADPCMB_TEST)
** move ADPCM A/B end check.
** ADPCMB repeat flag(no check)
** change ADPCM volume rate (8->16) (32->48).
**
** 09-12-98 hiro-shi:
** change ADPCM volume. (8->16, 48->64)
** replace ym2610 ch0/3 (YM-2610B)
** init cur_chip (restart bug fix)
** change ADPCM_SHIFT (10->8) missing bank change 0x4000-0xffff.
** add ADPCM_SHIFT_MASK
** change ADPCMA_DECODE_MIN/MAX.
*/




/************************************************************************/
/*    comment of hiro-shi(Hiromitsu Shioya)                             */
/*    YM2610(B) = OPN-B                                                 */
/*    YM2610  : PSG:3ch FM:4ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/*    YM2610B : PSG:3ch FM:6ch ADPCM(18.5KHz):6ch DeltaT ADPCM:1ch      */
/************************************************************************/

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "fm.h"


#ifndef PI
#define PI 3.14159265358979323846
#endif


/* include external DELTA-T unit (when needed) */
#if (BUILD_YM2608||BUILD_YM2610||BUILD_YM2610B)
	#include "ymdeltat.h"
#endif

/* shared function building option */
#define BUILD_OPN (BUILD_YM2203||BUILD_YM2608||BUILD_YM2610||BUILD_YM2610B||BUILD_YM2612)
#define BUILD_OPN_PRESCALER (BUILD_YM2203||BUILD_YM2608)


/* globals */
#define TYPE_SSG    0x01    /* SSG support          */
#define TYPE_LFOPAN 0x02    /* OPN type LFO and PAN */
#define TYPE_6CH    0x04    /* FM 6CH / 3CH         */
#define TYPE_DAC    0x08    /* YM2612's DAC device  */
#define TYPE_ADPCM  0x10    /* two ADPCM units      */
#define TYPE_2610   0x20    /* bogus flag to differentiate 2608 from 2610 */

#define TYPE_YM2203 (TYPE_SSG)
#define TYPE_YM2608 (TYPE_SSG |TYPE_LFOPAN |TYPE_6CH |TYPE_ADPCM)
#define TYPE_YM2610 (TYPE_SSG |TYPE_LFOPAN |TYPE_6CH |TYPE_ADPCM |TYPE_2610)
#define TYPE_YM2612 (TYPE_DAC |TYPE_LFOPAN |TYPE_6CH)



#define FREQ_SH			16  /* 16.16 fixed point (frequency calculations) */
#define EG_SH			16  /* 16.16 fixed point (envelope generator timing) */
#define LFO_SH			24  /*  8.24 fixed point (LFO calculations)       */
#define TIMER_SH		16  /* 16.16 fixed point (timers calculations)    */

#define FREQ_MASK		((1<<FREQ_SH)-1)

#define ENV_BITS		10
#define ENV_LEN			(1<<ENV_BITS)
#define ENV_STEP		(128.0/ENV_LEN)

#define MAX_ATT_INDEX	(ENV_LEN-1) /* 1023 */
#define MIN_ATT_INDEX	(0)			/* 0 */

#define EG_ATT			4
#define EG_DEC			3
#define EG_SUS			2
#define EG_REL			1
#define EG_OFF			0

#define SIN_BITS		10
#define SIN_LEN			(1<<SIN_BITS)
#define SIN_MASK		(SIN_LEN-1)

#define TL_RES_LEN		(256) /* 8 bits addressing (real chip) */


#if (FM_SAMPLE_BITS==16)
	#define FINAL_SH	(0)
	#define MAXOUT		(+32767)
	#define MINOUT		(-32768)
#else
	#define FINAL_SH	(8)
	#define MAXOUT		(+127)
	#define MINOUT		(-128)
#endif


/*	TL_TAB_LEN is calculated as:
*	13 - sinus amplitude bits     (Y axis)
*	2  - sinus sign bit           (Y axis)
*	TL_RES_LEN - sinus resolution (X axis)
*/
#define TL_TAB_LEN (13*2*TL_RES_LEN)
static signed int tl_tab[TL_TAB_LEN];

#define ENV_QUIET		(TL_TAB_LEN>>3)

/* sin waveform table in 'decibel' scale */
static unsigned int sin_tab[SIN_LEN];

/* sustain level table (3dB per step) */
/* bit0, bit1, bit2, bit3, bit4, bit5, bit6 */
/* 1,    2,    4,    8,    16,   32,   64   (value)*/
/* 0.75, 1.5,  3,    6,    12,   24,   48   (dB)*/

/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
#define SC(db) (UINT32) ( db * (4.0/ENV_STEP) )
static const UINT32 sl_table[16]={
 SC( 0),SC( 1),SC( 2),SC(3 ),SC(4 ),SC(5 ),SC(6 ),SC( 7),
 SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)
};
#undef SC


#define RATE_STEPS (8)
static const UINT8 eg_inc[19*RATE_STEPS]={

/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..11 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..11 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..11 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..11 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 12 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 12 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 12 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 12 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 13 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 13 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 13 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 13 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rate 14 0 (increment by 4) */
/*13 */ 4,4, 4,8, 4,4, 4,8, /* rate 14 1 */
/*14 */ 4,8, 4,8, 4,8, 4,8, /* rate 14 2 */
/*15 */ 4,8, 8,8, 4,8, 8,8, /* rate 14 3 */

/*16 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 8) */
/*17 */ 16,16,16,16,16,16,16,16, /* rates 15 2, 15 3 for attack */
/*18 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};


#define O(a) (a*RATE_STEPS)

/*note that there is no O(17) in this table - it's directly in the code */
static const UINT8 eg_rate_select[32+64+32]={	/* Envelope Generator rates (32 + 64 rates + 32 RKS) */
/* 32 infinite time rates */
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),

/* rates 00-11 */
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 12 */
O( 4),O( 5),O( 6),O( 7),

/* rate 13 */
O( 8),O( 9),O(10),O(11),

/* rate 14 */
O(12),O(13),O(14),O(15),

/* rate 15 */
O(16),O(16),O(16),O(16),

/* 32 dummy rates (same as 15 3) */
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16)

};

static const UINT8 eg_rate_select2612[32+64+32]={    /* Envelope Generator rates (32 + 64 rates + 32 RKS) */
/* 32 infinite time rates */
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),
O(18),O(18),O(18),O(18),O(18),O(18),O(18),O(18),

/* rates 00-11 */
O( 18),O( 18),O( 0),O( 0),
O( 0),O( 0),O( 2),O( 2),  // Nemesis's tests

O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 12 */
O( 4),O( 5),O( 6),O( 7),

/* rate 13 */
O( 8),O( 9),O(10),O(11),

/* rate 14 */
O(12),O(13),O(14),O(15),

/* rate 15 */
O(16),O(16),O(16),O(16),

/* 32 dummy rates (same as 15 3) */
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16),
O(16),O(16),O(16),O(16),O(16),O(16),O(16),O(16)

};
#undef O

/*rate  0,    1,    2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15*/
/*shift 11,  10,  9,  8,  7,  6,  5,  4,  3,  2, 1,  0,  0,  0,  0,  0 */
/*mask  2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3, 1,  0,  0,  0,  0,  0 */

#define O(a) (a*1)
static const UINT8 eg_rate_shift[32+64+32]={	/* Envelope Generator counter shifts (32 + 64 rates + 32 RKS) */
/* 32 infinite time rates */
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),

/* rates 00-11 */
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),
O( 0),O( 0),O( 0),O( 0),

/* rate 12 */
O( 0),O( 0),O( 0),O( 0),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 32 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0)

};
#undef O

static const UINT8 dt_tab[4 * 32]={
/* this is YM2151 and YM2612 phase increment data (in 10.10 fixed point format)*/
/* FD=0 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* FD=1 */
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
	2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8,
/* FD=2 */
	1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5,
	5, 6, 6, 7, 8, 8, 9,10,11,12,13,14,16,16,16,16,
/* FD=3 */
	2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7,
	8 , 8, 9,10,11,12,13,14,16,17,19,20,22,22,22,22
};


/* OPN key frequency number -> key code follow table */
/* fnum higher 4bit -> keycode lower 2bit */
static const UINT8 opn_fktable[16] = {0,0,0,0,0,0,0,1,2,3,3,3,3,3,3,3};


/* 8 LFO speed parameters */
/* each value represents number of samples that one LFO level will last for */
static const UINT32 lfo_samples_per_step[8] = {108, 77, 71, 67, 62, 44, 8, 5};



/*There are 4 different LFO AM depths available, they are:
  0 dB,	1.4 dB,	5.9 dB, 11.8 dB
  Here is how it is generated (in EG steps):

  11.8 dB =	0, 2, 4, 6, 8, 10,12,14,16...126,126,124,122,120,118,....4,2,0
   5.9 dB =	0, 1, 2, 3, 4, 5, 6, 7, 8....63, 63, 62, 61, 60, 59,.....2,1,0
   1.4 dB =	0, 0, 0, 0, 1, 1, 1, 1, 2,...15, 15, 15, 15, 14, 14,.....0,0,0

  (1.4 dB is loosing precision as you can see)

  It's implemented as generator from 0..126 with step 2 then a shift
  right N times, where N is:
	8 for 0 dB
	3 for 1.4 dB
	1 for 5.9 dB
	0 for 11.8 dB
*/
static const UINT8 lfo_ams_depth_shift[4] = {8, 3, 1, 0};



/*There are 8 different LFO PM depths available, they are:
  0, 3.4, 6.7, 10, 14, 20, 40, 80 (cents)

  Modulation level at each depth depends on F-NUMBER bits: 4,5,6,7,8,9,10
  (bits 8,9,10 = FNUM MSB from OCT/FNUM register)

  Here we store only first quarter (positive one) of full waveform.
  Full table (lfo_pm_table) containing all 128 waveforms is build
  at run (init) time.

  One value in table below represents 4 (four) basic LFO steps
  (1 PM step = 4 AM steps).

  For example:
   at LFO SPEED=0 (which is 108 samples per basic LFO step)
   one value from "lfo_pm_output" table lasts for 432 consecutive
   samples (4*108=432) and one full LFO waveform cycle lasts for 13824
   samples (32*432=13824; 32 because we store only a quarter of whole
            waveform in the table below)
*/
static const UINT8 lfo_pm_output[7*8][8]={ /* 7 bits meaningful (of F-NUMBER), 8 LFO output levels per one depth (out of 32), 8 LFO depths */
/* FNUM BIT 4: 000 0001xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 5 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 6 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 7 */ {0,   0,   0,   0,   1,   1,   1,   1},

/* FNUM BIT 5: 000 0010xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 5 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 6 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 7 */ {0,   0,   1,   1,   2,   2,   2,   3},

/* FNUM BIT 6: 000 0100xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 3 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 4 */ {0,   0,   0,   0,   0,   0,   0,   1},
/* DEPTH 5 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 6 */ {0,   0,   1,   1,   2,   2,   2,   3},
/* DEPTH 7 */ {0,   0,   2,   3,   4,   4,   5,   6},

/* FNUM BIT 7: 000 1000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 2 */ {0,   0,   0,   0,   0,   0,   1,   1},
/* DEPTH 3 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 4 */ {0,   0,   0,   1,   1,   1,   1,   2},
/* DEPTH 5 */ {0,   0,   1,   1,   2,   2,   2,   3},
/* DEPTH 6 */ {0,   0,   2,   3,   4,   4,   5,   6},
/* DEPTH 7 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},

/* FNUM BIT 8: 001 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   1,   1,   1,   1},
/* DEPTH 2 */ {0,   0,   0,   1,   1,   1,   2,   2},
/* DEPTH 3 */ {0,   0,   1,   1,   2,   2,   3,   3},
/* DEPTH 4 */ {0,   0,   1,   2,   2,   2,   3,   4},
/* DEPTH 5 */ {0,   0,   2,   3,   4,   4,   5,   6},
/* DEPTH 6 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},
/* DEPTH 7 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},

/* FNUM BIT 9: 010 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   2,   2,   2,   2},
/* DEPTH 2 */ {0,   0,   0,   2,   2,   2,   4,   4},
/* DEPTH 3 */ {0,   0,   2,   2,   4,   4,   6,   6},
/* DEPTH 4 */ {0,   0,   2,   4,   4,   4,   6,   8},
/* DEPTH 5 */ {0,   0,   4,   6,   8,   8, 0xa, 0xc},
/* DEPTH 6 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},
/* DEPTH 7 */ {0,   0,0x10,0x18,0x20,0x20,0x28,0x30},

/* FNUM BIT10: 100 0000xxxx */
/* DEPTH 0 */ {0,   0,   0,   0,   0,   0,   0,   0},
/* DEPTH 1 */ {0,   0,   0,   0,   4,   4,   4,   4},
/* DEPTH 2 */ {0,   0,   0,   4,   4,   4,   8,   8},
/* DEPTH 3 */ {0,   0,   4,   4,   8,   8, 0xc, 0xc},
/* DEPTH 4 */ {0,   0,   4,   8,   8,   8, 0xc,0x10},
/* DEPTH 5 */ {0,   0,   8, 0xc,0x10,0x10,0x14,0x18},
/* DEPTH 6 */ {0,   0,0x10,0x18,0x20,0x20,0x28,0x30},
/* DEPTH 7 */ {0,   0,0x20,0x30,0x40,0x40,0x50,0x60},

};

/* all 128 LFO PM waveforms */
static INT32 lfo_pm_table[128*8*32]; /* 128 combinations of 7 bits meaningful (of F-NUMBER), 8 LFO depths, 32 LFO output levels per one depth */





/* register number to channel number , slot offset */
#define OPN_CHAN(N) (N&3)
#define OPN_SLOT(N) ((N>>2)&3)

/* slot number */
#define SLOT1 0
#define SLOT2 2
#define SLOT3 1
#define SLOT4 3

/* bit0 = Right enable , bit1 = Left enable */
#define OUTD_RIGHT  1
#define OUTD_LEFT   2
#define OUTD_CENTER 3


/* save output as raw 16-bit sample */
/* #define SAVE_SAMPLE */

#ifdef SAVE_SAMPLE
static FILE *sample[1];
	#if 1	/*save to MONO file */
		#define SAVE_ALL_CHANNELS \
		{	signed int pom = lt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#else	/*save to STEREO file */
		#define SAVE_ALL_CHANNELS \
		{	signed int pom = lt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
			pom = rt; \
			fputc((unsigned short)pom&0xff,sample[0]); \
			fputc(((unsigned short)pom>>8)&0xff,sample[0]); \
		}
	#endif
#endif


/* struct describing a single operator (SLOT) */
typedef struct
{
	INT32	*DT;		/* detune          :dt_tab[DT] */
	UINT8	KSR;		/* key scale rate  :3-KSR */
	UINT32	ar;			/* attack rate  */
	UINT32	d1r;		/* decay rate   */
	UINT32	d2r;		/* sustain rate */
	UINT32	rr;			/* release rate */
	UINT8	ksr;		/* key scale rate  :kcode>>(3-KSR) */
	UINT32	mul;		/* multiple        :ML_TABLE[ML] */

	/* Phase Generator */
	UINT32	phase;		/* phase counter */
	INT32	Incr;		/* phase step */

	/* Envelope Generator */
	UINT8	state;		/* phase type */
	UINT32	tl;			/* total level: TL << 3	*/
	INT32	volume;		/* envelope counter	*/
	UINT32	sl;			/* sustain level:sl_table[SL] */
	UINT32	vol_out;	/* current output from EG circuit (without AM from LFO) */
	
	UINT8	eg_sh_ar;	/*  (attack state) */
	UINT8	eg_sel_ar;	/*  (attack state) */
	UINT8	eg_sh_d1r;	/*  (decay state) */
	UINT8	eg_sel_d1r;	/*  (decay state) */
	UINT8	eg_sh_d2r;	/*  (sustain state) */
	UINT8	eg_sel_d2r;	/*  (sustain state) */
	UINT8	eg_sh_rr;	/*  (release state) */
	UINT8	eg_sel_rr;	/*  (release state) */

	UINT8	ssg;		/* SSG-EG waveform */
	UINT8	ssgn;		/* SSG-EG negated output */

	UINT32	key;		/* 0=last key was KEY OFF, 1=KEY ON	*/

	/* LFO */
	UINT32	AMmask;		/* AM enable flag */

} FM_SLOT;

typedef struct
{
	FM_SLOT	SLOT[4];	/* four SLOTs (operators) */

	UINT8	ALGO;		/* algorithm */
	UINT8	FB;			/* feedback shift */
	INT32	op1_out[2];	/* op1 output for feedback */

	INT32	*connect1;	/* SLOT1 output	pointer */
	INT32	*connect3;	/* SLOT3 output	pointer */
	INT32	*connect2;	/* SLOT2 output	pointer */
	INT32	*connect4;	/* SLOT4 output	pointer */

	INT32	*mem_connect;/* where to put the delayed sample (MEM) */
	INT32	mem_value;	/* delayed sample (MEM) value */

	INT32	pms;		/* channel PMS */
	UINT8	ams;		/* channel AMS */

	UINT32	fc;			/* fnum,blk:adjusted to sample rate	*/
	UINT8	kcode;		/* key code:						*/
	UINT32	block_fnum;	/* current blk/fnum value for this slot (can be different betweeen slots of one channel in 3slot mode) */
} FM_CH;


typedef struct
{
	UINT8	index;		/* this chip index (number of chip)	*/
	int		clock;		/* master clock  (Hz)	*/
	int		rate;		/* sampling rate (Hz)	*/
	double	freqbase;	/* frequency base		*/
	double	TimerBase;	/* Timer base time		*/
#if FM_BUSY_FLAG_SUPPORT
	double	BusyExpire;	/* ExpireTime of Busy clear */
#endif
	UINT8	address;	/* address register		*/
	UINT8	irq;		/* interrupt level		*/
	UINT8	irqmask;	/* irq mask				*/
	UINT8	status;		/* status flag			*/
	UINT32	mode;		/* mode  CSM / 3SLOT	*/
	UINT8	prescaler_sel;/* prescaler selector	*/
	UINT8	fn_h;		/* freq latch			*/
	int		TA;			/* timer a				*/
	int		TAC;		/* timer a counter		*/
	UINT8	TB;			/* timer b				*/
	int		TBC;		/* timer b counter		*/
	/* local time tables */
	INT32	dt_tab[8][32];/* DeTune table		*/
	/* Extention Timer and IRQ handler */
	FM_TIMERHANDLER	Timer_Handler;
	FM_IRQHANDLER	IRQ_Handler;
} FM_ST;



/***********************************************************/
/* OPN unit                                                */
/***********************************************************/

/* OPN 3slot struct */
typedef struct
{
	UINT32  fc[3];			/* fnum3,blk3: calculated */
	UINT8	fn_h;			/* freq3 latch */
	UINT8	kcode[3];		/* key code */
	UINT32	block_fnum[3];	/* current fnum value for this slot (can be different betweeen slots of one channel in 3slot mode) */
} FM_3SLOT;

/* OPN/A/B common state */
typedef struct
{
	UINT8	type;			/* chip type */
	FM_ST	ST;				/* general state */
	FM_3SLOT SL3;			/* 3 slot mode state */
	FM_CH	*P_CH;			/* pointer of CH */
	unsigned int pan[6*2];	/* fm channels output masks (0xffffffff = enable) */

	UINT32	eg_cnt;			/* global envelope generator counter */
	UINT32	eg_timer;		/* global envelope generator counter works at frequency = chipclock/64/3 */
	UINT32	eg_timer_add;	/* step of eg_timer */
	UINT32	eg_timer_overflow;/* envelope generator timer overlfows every 3 samples (on real chip) */


	/* there are 2048 FNUMs that can be generated using FNUM/BLK registers
		but LFO works with one more bit of a precision so we really need 4096 elements */

	UINT32	fn_table[4096];	/* fnumber->increment counter */
	UINT32 fn_max;

	/* LFO */
	UINT32	lfo_cnt;
	UINT32	lfo_inc;

	UINT32	lfo_freq[8];	/* LFO FREQ table */
} FM_OPN;



/* current chip state */
static void		*cur_chip = 0;	/* pointer of current chip struct */
static FM_ST	*State;			/* basic status */
static FM_CH	*cch[8];		/* pointer of FM channels */


static INT32	m2,c1,c2;		/* Phase Modulation input for operators 2,3,4 */
static INT32	mem;			/* one sample delay memory */

static INT32	out_fm[8];		/* outputs of working channels */

#if (BUILD_YM2608||BUILD_YM2610||BUILD_YM2610B)
static INT32	out_adpcm[4];	/* channel output NONE,LEFT,RIGHT or CENTER for YM2608/YM2610 ADPCM */
static INT32	out_delta[4];	/* channel output NONE,LEFT,RIGHT or CENTER for YM2608/YM2610 DELTAT*/
#endif

static UINT32	LFO_AM;			/* runtime LFO calculations helper */
static INT32	LFO_PM;			/* runtime LFO calculations helper */

/* log output level */
#define LOG_ERR  3      /* ERROR       */
#define LOG_WAR  2      /* WARNING     */
#define LOG_INF  1      /* INFORMATION */
#define LOG_LEVEL LOG_INF

#define LOG(n,x) (void)0

/* limitter */
#define Limit(val, max,min) { \
	if ( val > max )      val = max; \
	else if ( val < min ) val = min; \
}


/* status set and IRQ handling */
INLINE void FM_STATUS_SET(FM_ST *ST,int flag)
{
	/* set status flag */
	ST->status |= flag;
	if ( !(ST->irq) && (ST->status & ST->irqmask) )
	{
		ST->irq = 1;
		/* callback user interrupt handler (IRQ is OFF to ON) */
		if(ST->IRQ_Handler) (ST->IRQ_Handler)(ST->index,1);
	}
}

/* status reset and IRQ handling */
INLINE void FM_STATUS_RESET(FM_ST *ST,int flag)
{
	/* reset status flag */
	ST->status &=~flag;
	if ( (ST->irq) && !(ST->status & ST->irqmask) )
	{
		ST->irq = 0;
		/* callback user interrupt handler (IRQ is ON to OFF) */
		if(ST->IRQ_Handler) (ST->IRQ_Handler)(ST->index,0);
	}
}

/* IRQ mask set */
INLINE void FM_IRQMASK_SET(FM_ST *ST,int flag)
{
	ST->irqmask = flag;
	/* IRQ handling check */
	FM_STATUS_SET(ST,0);
	FM_STATUS_RESET(ST,0);
}

/* OPN Mode Register Write */
INLINE void set_timers( FM_ST *ST, int n, int v )
{
	/* b7 = CSM MODE */
	/* b6 = 3 slot mode */
	/* b5 = reset b */
	/* b4 = reset a */
	/* b3 = timer enable b */
	/* b2 = timer enable a */
	/* b1 = load b */
	/* b0 = load a */
	ST->mode = v;

	/* reset Timer b flag */
	if( v & 0x20 )
		FM_STATUS_RESET(ST,0x02);
	/* reset Timer a flag */
	if( v & 0x10 )
		FM_STATUS_RESET(ST,0x01);
	/* load b */
	if( v & 0x02 )
	{
		if( ST->TBC == 0 )
		{
			ST->TBC = ( 256-ST->TB)<<4;
			/* External timer handler */
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,1,ST->TBC,ST->TimerBase);
		}
	}
	else
	{	/* stop timer b */
		if( ST->TBC != 0 )
		{
			ST->TBC = 0;
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,1,0,ST->TimerBase);
		}
	}
	/* load a */
	if( v & 0x01 )
	{
		if( ST->TAC == 0 )
		{
			ST->TAC = (1024-ST->TA);
			/* External timer handler */
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,0,ST->TAC,ST->TimerBase);
		}
	}
	else
	{	/* stop timer a */
		if( ST->TAC != 0 )
		{
			ST->TAC = 0;
			if (ST->Timer_Handler) (ST->Timer_Handler)(n,0,0,ST->TimerBase);
		}
	}
}


/* Timer A Overflow */
INLINE void TimerAOver(FM_ST *ST)
{
	/* set status (if enabled) */
	if(ST->mode & 0x04) FM_STATUS_SET(ST,0x01);
	/* clear or reload the counter */
	ST->TAC = (1024-ST->TA);
	if (ST->Timer_Handler) (ST->Timer_Handler)(ST->index,0,ST->TAC,ST->TimerBase);
}
/* Timer B Overflow */
INLINE void TimerBOver(FM_ST *ST)
{
	/* set status (if enabled) */
	if(ST->mode & 0x08) FM_STATUS_SET(ST,0x02);
	/* clear or reload the counter */
	ST->TBC = ( 256-ST->TB)<<4;
	if (ST->Timer_Handler) (ST->Timer_Handler)(ST->index,1,ST->TBC,ST->TimerBase);
}


#if FM_INTERNAL_TIMER
/* ----- internal timer mode , update timer */

/* ---------- calculate timer A ---------- */
	#define INTERNAL_TIMER_A(type, ST, CSM_CH)				\
	{													\
		if( ST->TAC &&  (ST->Timer_Handler==0) )		\
			if( (ST->TAC -= (int)(ST->freqbase*4096)) <= 0 )	\
			{											\
				TimerAOver( ST );						\
				/* CSM mode total level latch and auto key on */	\
				if( ST->mode & 0x80 )					\
					CSMKeyControll(type, CSM_CH );	\
			}											\
	}
/* ---------- calculate timer B ---------- */
	#define INTERNAL_TIMER_B(ST,step)						\
	{														\
		if( ST->TBC && (ST->Timer_Handler==0) )				\
			if( (ST->TBC -= (int)(ST->freqbase*4096*step)) <= 0 )	\
				TimerBOver( ST );							\
	}
#else /* FM_INTERNAL_TIMER */
/* external timer mode */
#define INTERNAL_TIMER_A(type, ST, CSM_CH)
#define INTERNAL_TIMER_B(ST,step)
#endif /* FM_INTERNAL_TIMER */



#if FM_BUSY_FLAG_SUPPORT
INLINE UINT8 FM_STATUS_FLAG(FM_ST *ST)
{
	if( ST->BusyExpire )
	{
		if( (ST->BusyExpire - FM_GET_TIME_NOW()) > 0)
			return ST->status | 0x80;	/* with busy */
		/* expire */
		ST->BusyExpire = 0;
	}
	return ST->status;
}
INLINE void FM_BUSY_SET(FM_ST *ST,int busyclock )
{
	ST->BusyExpire = FM_GET_TIME_NOW() + (ST->TimerBase * busyclock);
}
#define FM_BUSY_CLEAR(ST) ((ST)->BusyExpire = 0)
#else
#define FM_STATUS_FLAG(ST) ((ST)->status)
#define FM_BUSY_SET(ST,bclock) {}
#define FM_BUSY_CLEAR(ST) {}
#endif




INLINE void FM_KEYON(UINT8 type, FM_CH *CH , int s )
{
	FM_SLOT *SLOT = &CH->SLOT[s];
	if( !SLOT->key )
	{
		SLOT->key = 1;
		SLOT->phase = 0;		/* restart Phase Generator */
		SLOT->ssgn = (SLOT->ssg & 0x04) >> 1;

		if ((type == TYPE_YM2612) || (type == TYPE_YM2608))
		{
		        if( (SLOT->ar + SLOT->ksr) < 32+62 )
		        {
			        SLOT->state = EG_ATT;    /* phase -> Attack */
			}
			else
			{
			        /* directly switch to Decay */
			        SLOT->volume = MIN_ATT_INDEX;
			        SLOT->state = EG_DEC;
			}
		}
		else
		{
			SLOT->state = EG_ATT;
		}
	}
}

INLINE void FM_KEYOFF(FM_CH *CH , int s )
{
	FM_SLOT *SLOT = &CH->SLOT[s];
	if( SLOT->key )
	{
		SLOT->key = 0;
		if (SLOT->state>EG_REL)
			SLOT->state = EG_REL;/* phase -> Release */
	}
}

/* set algorithm connection */
static void setup_connection( FM_CH *CH, int ch )
{
	INT32 *carrier = &out_fm[ch];

	INT32 **om1 = &CH->connect1;
	INT32 **om2 = &CH->connect3;
	INT32 **oc1 = &CH->connect2;

	INT32 **memc = &CH->mem_connect;

	switch( CH->ALGO ){
	case 0:
		/* M1---C1---MEM---M2---C2---OUT */
		*om1 = &c1;
		*oc1 = &mem;
		*om2 = &c2;
		*memc= &m2;
		break;
	case 1:
		/* M1------+-MEM---M2---C2---OUT */
		/*      C1-+                     */
		*om1 = &mem;
		*oc1 = &mem;
		*om2 = &c2;
		*memc= &m2;
		break;
	case 2:
		/* M1-----------------+-C2---OUT */
		/*      C1---MEM---M2-+          */
		*om1 = &c2;
		*oc1 = &mem;
		*om2 = &c2;
		*memc= &m2;
		break;
	case 3:
		/* M1---C1---MEM------+-C2---OUT */
		/*                 M2-+          */
		*om1 = &c1;
		*oc1 = &mem;
		*om2 = &c2;
		*memc= &c2;
		break;
	case 4:
		/* M1---C1-+-OUT */
		/* M2---C2-+     */
		/* MEM: not used */
		*om1 = &c1;
		*oc1 = carrier;
		*om2 = &c2;
		*memc= &mem;	/* store it anywhere where it will not be used */
		break;
	case 5:
		/*    +----C1----+     */
		/* M1-+-MEM---M2-+-OUT */
		/*    +----C2----+     */
		*om1 = 0;	/* special mark */
		*oc1 = carrier;
		*om2 = carrier;
		*memc= &m2;
		break;
	case 6:
		/* M1---C1-+     */
		/*      M2-+-OUT */
		/*      C2-+     */
		/* MEM: not used */
		*om1 = &c1;
		*oc1 = carrier;
		*om2 = carrier;
		*memc= &mem;	/* store it anywhere where it will not be used */
		break;
	case 7:
		/* M1-+     */
		/* C1-+-OUT */
		/* M2-+     */
		/* C2-+     */
		/* MEM: not used*/
		*om1 = carrier;
		*oc1 = carrier;
		*om2 = carrier;
		*memc= &mem;	/* store it anywhere where it will not be used */
		break;
	}

	CH->connect4 = carrier;
}

/* set detune & multiple */
INLINE void set_det_mul(FM_ST *ST,FM_CH *CH,FM_SLOT *SLOT,int v)
{
	SLOT->mul = (v&0x0f)? (v&0x0f)*2 : 1;
	SLOT->DT  = ST->dt_tab[(v>>4)&7];
	CH->SLOT[SLOT1].Incr=-1;
}

/* set total level */
INLINE void set_tl(FM_CH *CH,FM_SLOT *SLOT , int v)
{
	(void)CH;
	SLOT->tl = (v&0x7f)<<(ENV_BITS-7); /* 7bit TL */
}

/* set attack rate & key scale  */
INLINE void set_ar_ksr(UINT8 type, FM_CH *CH,FM_SLOT *SLOT,int v)
{
	UINT8 old_KSR = SLOT->KSR;

	SLOT->ar = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

	SLOT->KSR = 3-(v>>6);
	if (SLOT->KSR != old_KSR)
	{
		CH->SLOT[SLOT1].Incr=-1;
	}

	/* refresh Attack rate */
	if ((SLOT->ar + SLOT->ksr) < 32+62)
	{
		SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar  + SLOT->ksr ];
		if ((type == TYPE_YM2612) || (type == TYPE_YM2608))
		{
			SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar  + SLOT->ksr ];
			SLOT->eg_sel_ar = eg_rate_select2612[SLOT->ar  + SLOT->ksr ];
		}
		else
		{
			SLOT->eg_sel_ar = eg_rate_select[SLOT->ar  + SLOT->ksr ];
		}
	}
	else
	{
		SLOT->eg_sh_ar  = 0;
		SLOT->eg_sel_ar = 17*RATE_STEPS;
	}
}

/* set decay rate */
INLINE void set_dr(UINT8 type, FM_SLOT *SLOT,int v)
{
	SLOT->d1r = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

	SLOT->eg_sh_d1r = eg_rate_shift [SLOT->d1r + SLOT->ksr];
	if ((type == TYPE_YM2612) || (type == TYPE_YM2608))
	{
		SLOT->eg_sel_d1r= eg_rate_select2612[SLOT->d1r + SLOT->ksr];
	}
	else
	{
		SLOT->eg_sel_d1r= eg_rate_select[SLOT->d1r + SLOT->ksr];
	}

}

/* set sustain rate */
INLINE void set_sr(UINT8 type, FM_SLOT *SLOT,int v)
{
	SLOT->d2r = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

	SLOT->eg_sh_d2r = eg_rate_shift [SLOT->d2r + SLOT->ksr];
	if ((type == TYPE_YM2612) || (type == TYPE_YM2608))
	{
		SLOT->eg_sel_d2r= eg_rate_select2612[SLOT->d2r + SLOT->ksr];
	}
	else
	{
		SLOT->eg_sel_d2r= eg_rate_select[SLOT->d2r + SLOT->ksr];
	}
}

/* set release rate */
INLINE void set_sl_rr(UINT8 type, FM_SLOT *SLOT,int v)
{
	SLOT->sl = sl_table[ v>>4 ];

	SLOT->rr  = 34 + ((v&0x0f)<<2);

	SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr  + SLOT->ksr];
	if ((type == TYPE_YM2612) || (type == TYPE_YM2608))
	{
		SLOT->eg_sel_rr = eg_rate_select2612[SLOT->rr  + SLOT->ksr];
	}
	else
	{
		SLOT->eg_sel_rr = eg_rate_select[SLOT->rr  + SLOT->ksr];
	}
}



INLINE signed int op_calc(UINT32 phase, unsigned int env, signed int pm)
{
	UINT32 p;

	p = (env<<3) + sin_tab[ ( ((signed int)((phase & ~FREQ_MASK) + (pm<<15))) >> FREQ_SH ) & SIN_MASK ];

	if (p >= TL_TAB_LEN)
		return 0;
	return tl_tab[p];
}

INLINE signed int op_calc1(UINT32 phase, unsigned int env, signed int pm)
{
	UINT32 p;

	p = (env<<3) + sin_tab[ ( ((signed int)((phase & ~FREQ_MASK) + pm      )) >> FREQ_SH ) & SIN_MASK ];

	if (p >= TL_TAB_LEN)
		return 0;
	return tl_tab[p];
}

/* advance LFO to next sample */
INLINE void advance_lfo(FM_OPN *OPN)
{
	UINT8 pos;

	if (OPN->lfo_inc)	/* LFO enabled ? */
	{
		OPN->lfo_cnt += OPN->lfo_inc;

		pos = (OPN->lfo_cnt >> LFO_SH) & 127;


		/* update AM when LFO output changes */

		/* actually I can't optimize is this way without rewritting chan_calc()
		to use chip->lfo_am instead of global lfo_am */
		{

			/* triangle */
			/* AM: 0 to 126 step +2, 126 to 0 step -2 */
			if (pos<64)
				LFO_AM = (pos&63) * 2;
			else
				LFO_AM = 126 - ((pos&63) * 2);
		}

		/* PM works with 4 times slower clock */
		pos >>= 2;
		/* update PM when LFO output changes */
		/*if (prev_pos != pos)*/ /* can't use global lfo_pm for this optimization, must be chip->lfo_pm instead*/
		{
			LFO_PM = pos;
		}

	}
	else
	{
		LFO_AM = 0;
		LFO_PM = 0;
	}
}

INLINE void advance_eg_channel(FM_OPN *OPN, FM_SLOT *SLOT)
{
	unsigned int out;
	unsigned int swap_flag = 0;
	unsigned int i;


	i = 4; /* four operators per channel */
	do
	{
		/* reset SSG-EG swap flag */
		swap_flag = 0;
		
		switch(SLOT->state)
		{
		case EG_ATT:		/* attack phase */
			if ( !(OPN->eg_cnt & ((1<<SLOT->eg_sh_ar)-1) ) )
			{
				SLOT->volume += (~SLOT->volume *
                                  (eg_inc[SLOT->eg_sel_ar + ((OPN->eg_cnt>>SLOT->eg_sh_ar)&7)])
                                ) >>4;

				if (SLOT->volume <= MIN_ATT_INDEX)
				{
					SLOT->volume = MIN_ATT_INDEX;
					SLOT->state = EG_DEC;
				}
			}
		break;

		case EG_DEC:	/* decay phase */
			if ((OPN->type == TYPE_YM2612) || (OPN->type == TYPE_YM2608))
			{
				if ( !(OPN->eg_cnt & ((1<<SLOT->eg_sh_d1r)-1) ) )
                                {
					if (SLOT->ssg&0x08)   /* SSG EG type envelope selected */
					{
				       		SLOT->volume += 6 * eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r)&7)];
					}
					else
					{
                                                SLOT->volume += eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r)&7)];
					}
        			}
        			
        			/* check transition even if no volume update: this fixes the case when SL = MIN_ATT_INDEX */
				if ( SLOT->volume >= (INT32)(SLOT->sl) )
				{
					SLOT->volume = (INT32)(SLOT->sl);
					SLOT->state = EG_SUS;
 				}
			}
			else
			{
				if (SLOT->ssg&0x08)	/* SSG EG type envelope selected */
				{
					if ( !(OPN->eg_cnt & ((1<<SLOT->eg_sh_d1r)-1) ) )
					{
						SLOT->volume += 4 * eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r)&7)];

						if ( SLOT->volume >= (INT32)(SLOT->sl) )
							SLOT->state = EG_SUS;
					}
				}
				else
				{
					if ( !(OPN->eg_cnt & ((1<<SLOT->eg_sh_d1r)-1) ) )
					{
						SLOT->volume += eg_inc[SLOT->eg_sel_d1r + ((OPN->eg_cnt>>SLOT->eg_sh_d1r)&7)];

						if ( SLOT->volume >= (INT32)(SLOT->sl) )
							SLOT->state = EG_SUS;
					}
				}
			}
		break;

		case EG_SUS:	/* sustain phase */
			if (SLOT->ssg&0x08)	/* SSG EG type envelope selected */
			{
				if ( !(OPN->eg_cnt & ((1<<SLOT->eg_sh_d2r)-1) ) )
				{
					if ((OPN->type == TYPE_YM2612) || (OPN->type == TYPE_YM2608))
					{
						SLOT->volume += 6 * eg_inc[SLOT->eg_sel_d2r + ((OPN->eg_cnt>>SLOT->eg_sh_d2r)&7)];
					}
					else
					{
						SLOT->volume += 4 * eg_inc[SLOT->eg_sel_d2r + ((OPN->eg_cnt>>SLOT->eg_sh_d2r)&7)];
					}

					if ( SLOT->volume >= ENV_QUIET )
					{
						if ((OPN->type != TYPE_YM2612) && (OPN->type != TYPE_YM2608))
						{
							SLOT->volume = MAX_ATT_INDEX;
						}

						if (SLOT->ssg&0x01)	/* bit 0 = hold */
						{
							if (SLOT->ssgn&1)	/* have we swapped once ??? */
							{
								/* yes, so do nothing, just hold current level */
							}
							else
								swap_flag = (SLOT->ssg&0x02) | 1 ; /* bit 1 = alternate */

						}
						else
						{
							/* same as KEY-ON operation */

							/* restart of the Phase Generator should be here */
							SLOT->phase = 0;

							if ((OPN->type == TYPE_YM2612) || (OPN->type == TYPE_YM2608))
							{
								if ((SLOT->ar + SLOT->ksr) < 94 /*32+62*/)
								{
									SLOT->state = EG_ATT; /* phase -> Attack */
								}
								else
								{
									/* Attack Rate is maximal: directly switch to Decay (or Substain) */
									SLOT->volume = MIN_ATT_INDEX;
									SLOT->state = (SLOT->sl == MIN_ATT_INDEX) ? EG_SUS : EG_DEC;
								}
							}
							else
							{
								/* phase -> Attack */
								SLOT->volume = 511;
								SLOT->state = EG_ATT;
							}

							swap_flag = (SLOT->ssg&0x02); /* bit 1 = alternate */
						}
					}
				}
			}
			else
			{
				if ( !(OPN->eg_cnt & ((1<<SLOT->eg_sh_d2r)-1) ) )
				{
					SLOT->volume += eg_inc[SLOT->eg_sel_d2r + ((OPN->eg_cnt>>SLOT->eg_sh_d2r)&7)];

					if ( SLOT->volume >= MAX_ATT_INDEX )
					{
						SLOT->volume = MAX_ATT_INDEX;
						/* do not change SLOT->state (verified on real chip) */
					}
				}

			}
		break;

		case EG_REL:	/* release phase */
				if ( !(OPN->eg_cnt & ((1<<SLOT->eg_sh_rr)-1) ) )
				{
					/* SSG-EG affects Release phase also (Nemesis) */
					if ((SLOT->ssg&0x08) && ((OPN->type = TYPE_YM2612) || (OPN->type = TYPE_YM2608)))
					{
						SLOT->volume += 6 * eg_inc[SLOT->eg_sel_rr + ((OPN->eg_cnt>>SLOT->eg_sh_rr)&7)];
					}
					else
					{
						SLOT->volume += eg_inc[SLOT->eg_sel_rr + ((OPN->eg_cnt>>SLOT->eg_sh_rr)&7)];
					}

					if ( SLOT->volume >= MAX_ATT_INDEX )
					{
						SLOT->volume = MAX_ATT_INDEX;
						SLOT->state = EG_OFF;
					}
				}
		break;

		}

		out = ((UINT32)SLOT->volume);

                /* negate output (changes come from alternate bit, init comes from attack bit) */
		if ((SLOT->ssg&0x08) && (SLOT->ssgn&2) && (SLOT->state > EG_REL))
			out ^= MAX_ATT_INDEX;

		/* we need to store the result here because we are going to change ssgn
			in next instruction */
		SLOT->vol_out = out + SLOT->tl;

		/* reverse SLOT inversion flag */
		SLOT->ssgn ^= swap_flag;

		SLOT++;
		i--;
	}while (i);

}



#define volume_calc(OP) ((OP)->vol_out + (AM & (OP)->AMmask))

INLINE void update_phase_lfo_slot(FM_OPN *OPN, FM_SLOT *SLOT, INT32 pms, UINT32 block_fnum)
{
	UINT32 fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
	INT32  lfo_fn_table_index_offset = lfo_pm_table[ fnum_lfo + pms + LFO_PM ];

	if (lfo_fn_table_index_offset)    /* LFO phase modulation active */
	{
		UINT8 blk;
		UINT32 fn;
		int kc, fc;

		block_fnum = block_fnum*2 + lfo_fn_table_index_offset;

		blk = (block_fnum&0x7000) >> 12;
		fn  = block_fnum & 0xfff;

		/* keyscale code */
		kc = (blk<<2) | opn_fktable[fn >> 8];

		/* phase increment counter */
		fc = (OPN->fn_table[fn]>>(7-blk)) + SLOT->DT[kc];

		/* detects frequency overflow (credits to Nemesis) */
		if (fc < 0) fc += OPN->fn_max;

		/* update phase */
		SLOT->phase += (fc * SLOT->mul) >> 1;
	}
	else    /* LFO phase modulation  = zero */
	{
		SLOT->phase += SLOT->Incr;
	}
}

INLINE void update_phase_lfo_channel(FM_OPN *OPN, FM_CH *CH)
{
	UINT32 block_fnum = CH->block_fnum;

	UINT32 fnum_lfo  = ((block_fnum & 0x7f0) >> 4) * 32 * 8;
	INT32  lfo_fn_table_index_offset = lfo_pm_table[ fnum_lfo + CH->pms + LFO_PM ];

	if (lfo_fn_table_index_offset)    /* LFO phase modulation active */
	{
	        UINT8 blk;
	        UINT32 fn;
		int kc, fc, finc;

		block_fnum = block_fnum*2 + lfo_fn_table_index_offset;

	        blk = (block_fnum&0x7000) >> 12;
	        fn  = block_fnum & 0xfff;

		/* keyscale code */
	        kc = (blk<<2) | opn_fktable[fn >> 8];

	        /* phase increment counter */
		fc = (OPN->fn_table[fn]>>(7-blk));

		/* detects frequency overflow (credits to Nemesis) */
		finc = fc + CH->SLOT[SLOT1].DT[kc];

		if (finc < 0) finc += OPN->fn_max;
		CH->SLOT[SLOT1].phase += (finc*CH->SLOT[SLOT1].mul) >> 1;

		finc = fc + CH->SLOT[SLOT2].DT[kc];
		if (finc < 0) finc += OPN->fn_max;
		CH->SLOT[SLOT2].phase += (finc*CH->SLOT[SLOT2].mul) >> 1;

		finc = fc + CH->SLOT[SLOT3].DT[kc];
		if (finc < 0) finc += OPN->fn_max;
		CH->SLOT[SLOT3].phase += (finc*CH->SLOT[SLOT3].mul) >> 1;

		finc = fc + CH->SLOT[SLOT4].DT[kc];
		if (finc < 0) finc += OPN->fn_max;
		CH->SLOT[SLOT4].phase += (finc*CH->SLOT[SLOT4].mul) >> 1;
	}
	else    /* LFO phase modulation  = zero */
	{
	        CH->SLOT[SLOT1].phase += CH->SLOT[SLOT1].Incr;
	        CH->SLOT[SLOT2].phase += CH->SLOT[SLOT2].Incr;
	        CH->SLOT[SLOT3].phase += CH->SLOT[SLOT3].Incr;
	        CH->SLOT[SLOT4].phase += CH->SLOT[SLOT4].Incr;
	}
}

INLINE void chan_calc(FM_OPN *OPN, FM_CH *CH, int chnum)
{
	unsigned int eg_out;

	UINT32 AM = LFO_AM >> CH->ams;


	m2 = c1 = c2 = mem = 0;

	*CH->mem_connect = CH->mem_value;	/* restore delayed sample (MEM) value to m2 or c2 */

	eg_out = volume_calc(&CH->SLOT[SLOT1]);
	{
		INT32 out = CH->op1_out[0] + CH->op1_out[1];
		CH->op1_out[0] = CH->op1_out[1];

		if( !CH->connect1 ){
			/* algorithm 5  */
			mem = c1 = c2 = CH->op1_out[0];
		}
		else
		{
			/* other algorithms */
			*CH->connect1 += CH->op1_out[0];
		}

		CH->op1_out[1] = 0;
		if( eg_out < ENV_QUIET )	/* SLOT 1 */
		{
			if (!CH->FB)
				out=0;

			CH->op1_out[1] = op_calc1(CH->SLOT[SLOT1].phase, eg_out, (out<<CH->FB) );
		}
	}

	eg_out = volume_calc(&CH->SLOT[SLOT3]);
	if( eg_out < ENV_QUIET )		/* SLOT 3 */
		*CH->connect3 += op_calc(CH->SLOT[SLOT3].phase, eg_out, m2);

	eg_out = volume_calc(&CH->SLOT[SLOT2]);
	if( eg_out < ENV_QUIET )		/* SLOT 2 */
		*CH->connect2 += op_calc(CH->SLOT[SLOT2].phase, eg_out, c1);

	eg_out = volume_calc(&CH->SLOT[SLOT4]);
	if( eg_out < ENV_QUIET )		/* SLOT 4 */
		*CH->connect4 += op_calc(CH->SLOT[SLOT4].phase, eg_out, c2);


	/* store current MEM */
	CH->mem_value = mem;

	/* update phase counters AFTER output calculations */
	if(CH->pms)
	{
		/* add support for 3 slot mode */
		if ((OPN->ST.mode & 0xC0) && (chnum == 2))
		{
		        update_phase_lfo_slot(OPN, &CH->SLOT[SLOT1], CH->pms, OPN->SL3.block_fnum[1]);
		        update_phase_lfo_slot(OPN, &CH->SLOT[SLOT2], CH->pms, OPN->SL3.block_fnum[2]);
		        update_phase_lfo_slot(OPN, &CH->SLOT[SLOT3], CH->pms, OPN->SL3.block_fnum[0]);
		        update_phase_lfo_slot(OPN, &CH->SLOT[SLOT4], CH->pms, CH->block_fnum);
		}
		else update_phase_lfo_channel(OPN, CH);
	}
	else	/* no LFO phase modulation */
	{
		CH->SLOT[SLOT1].phase += CH->SLOT[SLOT1].Incr;
		CH->SLOT[SLOT2].phase += CH->SLOT[SLOT2].Incr;
		CH->SLOT[SLOT3].phase += CH->SLOT[SLOT3].Incr;
		CH->SLOT[SLOT4].phase += CH->SLOT[SLOT4].Incr;
	}
}

/* update phase increment and envelope generator */
INLINE void refresh_fc_eg_slot(FM_OPN *OPN, FM_SLOT *SLOT , int fc , int kc )
{
	int ksr = kc >> SLOT->KSR;

	fc += SLOT->DT[kc];

	/* detects frequency overflow (credits to Nemesis) */
	if (fc < 0) fc += OPN->fn_max;

	/* (frequency) phase increment counter */
	SLOT->Incr = (fc * SLOT->mul) >> 1;

	if( SLOT->ksr != ksr )
	{
		SLOT->ksr = ksr;

		/* calculate envelope generator rates */
		if ((SLOT->ar + SLOT->ksr) < 32+62)
		{
			SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar  + SLOT->ksr ];
			if ((OPN->type == TYPE_YM2612) || (OPN->type == TYPE_YM2608))
			{
				SLOT->eg_sel_ar = eg_rate_select2612[SLOT->ar  + SLOT->ksr ];
			}
			else
			{
				SLOT->eg_sel_ar = eg_rate_select[SLOT->ar  + SLOT->ksr ];
			}
		}
		else
		{
			SLOT->eg_sh_ar  = 0;
			SLOT->eg_sel_ar = 17*RATE_STEPS;
		}

		SLOT->eg_sh_d1r = eg_rate_shift [SLOT->d1r + SLOT->ksr];
		SLOT->eg_sh_d2r = eg_rate_shift [SLOT->d2r + SLOT->ksr];
		SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr  + SLOT->ksr];

		if ((OPN->type == TYPE_YM2612) || (OPN->type == TYPE_YM2608))
		{
			SLOT->eg_sel_d1r= eg_rate_select2612[SLOT->d1r + SLOT->ksr];
			SLOT->eg_sel_d2r= eg_rate_select2612[SLOT->d2r + SLOT->ksr];
			SLOT->eg_sel_rr = eg_rate_select2612[SLOT->rr  + SLOT->ksr];
		}
		else
		{
			SLOT->eg_sel_d1r= eg_rate_select[SLOT->d1r + SLOT->ksr];
			SLOT->eg_sel_d2r= eg_rate_select[SLOT->d2r + SLOT->ksr];
			SLOT->eg_sel_rr = eg_rate_select[SLOT->rr  + SLOT->ksr];
		}
	}
}

/* update phase increment counters */
/* Changed from INLINE to static to work around gcc 4.2.1 codegen bug */
static void refresh_fc_eg_chan(FM_OPN *OPN, FM_CH *CH )
{
	if( CH->SLOT[SLOT1].Incr==-1){
		int fc = CH->fc;
		int kc = CH->kcode;
		refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT1] , fc , kc );
		refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT2] , fc , kc );
		refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT3] , fc , kc );
		refresh_fc_eg_slot(OPN, &CH->SLOT[SLOT4] , fc , kc );
	}
}

/* initialize time tables */
static void init_timetables( FM_ST *ST , const UINT8 *dttable )
{
	int i,d;
	double rate;

#if 0
	logerror("FM.C: samplerate=%8i chip clock=%8i  freqbase=%f  \n",
			 ST->rate, ST->clock, ST->freqbase );
#endif

	/* DeTune table */
	for (d = 0;d <= 3;d++){
		for (i = 0;i <= 31;i++){
			rate = ((double)dttable[d*32 + i]) * SIN_LEN  * ST->freqbase  * (1<<FREQ_SH) / ((double)(1<<20));
			ST->dt_tab[d][i]   = (INT32) rate;
			ST->dt_tab[d+4][i] = -ST->dt_tab[d][i];
#if 0
			logerror("FM.C: DT [%2i %2i] = %8x  \n", d, i, ST->dt_tab[d][i] );
#endif
		}
	}

}


static void reset_channels( FM_ST *ST , FM_CH *CH , int num )
{
	int c,s;

	ST->mode   = 0;	/* normal mode */
	ST->TA     = 0;
	ST->TAC    = 0;
	ST->TB     = 0;
	ST->TBC    = 0;

	for( c = 0 ; c < num ; c++ )
	{
		CH[c].fc = 0;
		for(s = 0 ; s < 4 ; s++ )
		{
			CH[c].SLOT[s].ssg = 0;
			CH[c].SLOT[s].ssgn = 0;
			CH[c].SLOT[s].state= EG_OFF;
			CH[c].SLOT[s].volume = MAX_ATT_INDEX;
			CH[c].SLOT[s].vol_out= MAX_ATT_INDEX;
		}
	}
}

/* initialize generic tables */
static int init_tables(void)
{
	signed int i,x;
	signed int n;
	double o,m;

	for (x=0; x<TL_RES_LEN; x++)
	{
		m = (1<<16) / pow(2, (x+1) * (ENV_STEP/4.0) / 8.0);
		m = floor(m);

		/* we never reach (1<<16) here due to the (x+1) */
		/* result fits within 16 bits at maximum */

		n = (int)m;		/* 16 bits here */
		n >>= 4;		/* 12 bits here */
		if (n&1)		/* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;
						/* 11 bits here (rounded) */
		n <<= 2;		/* 13 bits here (as in real chip) */
		tl_tab[ x*2 + 0 ] = n;
		tl_tab[ x*2 + 1 ] = -tl_tab[ x*2 + 0 ];

		for (i=1; i<13; i++)
		{
			tl_tab[ x*2+0 + i*2*TL_RES_LEN ] =  tl_tab[ x*2+0 ]>>i;
			tl_tab[ x*2+1 + i*2*TL_RES_LEN ] = -tl_tab[ x*2+0 + i*2*TL_RES_LEN ];
		}
	#if 0
			logerror("tl %04i", x);
			for (i=0; i<13; i++)
				logerror(", [%02i] %4x", i*2, tl_tab[ x*2 /*+1*/ + i*2*TL_RES_LEN ]);
			logerror("\n");
		}
	#endif
	}
	/*logerror("FM.C: TL_TAB_LEN = %i elements (%i bytes)\n",TL_TAB_LEN, (int)sizeof(tl_tab));*/


	for (i=0; i<SIN_LEN; i++)
	{
		/* non-standard sinus */
		m = sin( ((i*2)+1) * PI / SIN_LEN ); /* checked against the real chip */

		/* we never reach zero here due to ((i*2)+1) */

		if (m>0.0)
			o = 8*log(1.0/m)/log(2);	/* convert to 'decibels' */
		else
			o = 8*log(-1.0/m)/log(2);	/* convert to 'decibels' */

		o = o / (ENV_STEP/4);

		n = (int)(2.0*o);
		if (n&1)						/* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;

		sin_tab[ i ] = n*2 + (m>=0.0? 0: 1 );
		/*logerror("FM.C: sin [%4i]= %4i (tl_tab value=%5i)\n", i, sin_tab[i],tl_tab[sin_tab[i]]);*/
	}

	/*logerror("FM.C: ENV_QUIET= %08x\n",ENV_QUIET );*/


	/* build LFO PM modulation table */
	for(i = 0; i < 8; i++) /* 8 PM depths */
	{
		UINT8 fnum;
		for (fnum=0; fnum<128; fnum++) /* 7 bits meaningful of F-NUMBER */
		{
			UINT8 value;
			UINT8 step;
			UINT32 offset_depth = i;
			UINT32 offset_fnum_bit;
			UINT32 bit_tmp;

			for (step=0; step<8; step++)
			{
				value = 0;
				for (bit_tmp=0; bit_tmp<7; bit_tmp++) /* 7 bits */
				{
					if (fnum & (1<<bit_tmp)) /* only if bit "bit_tmp" is set */
					{
						offset_fnum_bit = bit_tmp * 8;
						value += lfo_pm_output[offset_fnum_bit + offset_depth][step];
					}
				}
				lfo_pm_table[(fnum*32*8) + (i*32) + step   + 0] = value;
				lfo_pm_table[(fnum*32*8) + (i*32) +(step^7)+ 8] = value;
				lfo_pm_table[(fnum*32*8) + (i*32) + step   +16] = -value;
				lfo_pm_table[(fnum*32*8) + (i*32) +(step^7)+24] = -value;
			}
#if 0
			logerror("LFO depth=%1x FNUM=%04x (<<4=%4x): ", i, fnum, fnum<<4);
			for (step=0; step<16; step++) /* dump only positive part of waveforms */
				logerror("%02x ", lfo_pm_table[(fnum*32*8) + (i*32) + step] );
			logerror("\n");
#endif

		}
	}



#ifdef SAVE_SAMPLE
	sample[0]=fopen("sampsum.pcm","wb");
#endif

	return 1;

}



static void FMCloseTable( void )
{
#ifdef SAVE_SAMPLE
	fclose(sample[0]);
#endif
	return;
}


/* CSM Key Controll */
INLINE void CSMKeyControll(UINT8 type, FM_CH *CH)
{
	/* all key on then off (only for operators which were OFF!) */
	if (!CH->SLOT[SLOT1].key)
	{
		FM_KEYON(type, CH,SLOT1);
		FM_KEYOFF(CH, SLOT1);
	}
	if (!CH->SLOT[SLOT2].key)
	{
		FM_KEYON(type, CH,SLOT2);
		FM_KEYOFF(CH, SLOT2);
	}
	if (!CH->SLOT[SLOT3].key)
	{
		FM_KEYON(type, CH,SLOT3);
		FM_KEYOFF(CH, SLOT3);
	}
	if (!CH->SLOT[SLOT4].key)
	{
		FM_KEYON(type, CH,SLOT4);
		FM_KEYOFF(CH, SLOT4);
	}
}

#ifdef _STATE_H
/* FM channel save , internal state only */
static void FMsave_state_channel(const char *name,int num,FM_CH *CH,int num_ch)
{
	int slot , ch;
	char state_name[20];
	const char slot_array[4] = { 1 , 3 , 2 , 4 };

	for(ch=0;ch<num_ch;ch++,CH++)
	{
		/* channel */
		sprintf(state_name,"%s.CH%d",name,ch);
		state_save_register_INT32(state_name, num, "feedback" , CH->op1_out , 2);
		state_save_register_UINT32(state_name, num, "phasestep"   , &CH->fc , 1);
		/* slots */
		for(slot=0;slot<4;slot++)
		{
			FM_SLOT *SLOT = &CH->SLOT[slot];

			sprintf(state_name,"%s.CH%d.SLOT%d",name,ch,slot_array[slot]);
			state_save_register_UINT32(state_name, num, "phasecount" , &SLOT->phase, 1);
			state_save_register_UINT8 (state_name, num, "state"      , &SLOT->state, 1);
			state_save_register_INT32 (state_name, num, "volume"     , &SLOT->volume, 1);
		}
	}
}

static void FMsave_state_st(const char *state_name,int num,FM_ST *ST)
{
#if FM_BUSY_FLAG_SUPPORT
	state_save_register_double(state_name, num, "BusyExpire", &ST->BusyExpire , 1);
#endif
	state_save_register_UINT8 (state_name, num, "address"   , &ST->address , 1);
	state_save_register_UINT8 (state_name, num, "IRQ"       , &ST->irq     , 1);
	state_save_register_UINT8 (state_name, num, "IRQ MASK"  , &ST->irqmask , 1);
	state_save_register_UINT8 (state_name, num, "status"    , &ST->status  , 1);
	state_save_register_UINT32(state_name, num, "mode"      , &ST->mode    , 1);
	state_save_register_UINT8 (state_name, num, "prescaler" , &ST->prescaler_sel , 1);
	state_save_register_UINT8 (state_name, num, "freq latch", &ST->fn_h , 1);
	state_save_register_int   (state_name, num, "TIMER A"   , &ST->TA   );
	state_save_register_int   (state_name, num, "TIMER Acnt", &ST->TAC  );
	state_save_register_UINT8 (state_name, num, "TIMER B"   , &ST->TB   , 1);
	state_save_register_int   (state_name, num, "TIMER Bcnt", &ST->TBC  );
}
#endif /* _STATE_H */

#if BUILD_OPN



/* prescaler set (and make time tables) */
static void OPNSetPres(FM_OPN *OPN , int pres , int TimerPres, int SSGpres)
{
	int i;

	/* frequency base */
	OPN->ST.freqbase = (OPN->ST.rate) ? ((double)OPN->ST.clock / OPN->ST.rate) / pres : 0;

#if 0
	OPN->ST.rate = (double)OPN->ST.clock / pres;
	OPN->ST.freqbase = 1.0;
#endif

	OPN->eg_timer_add  = (1<<EG_SH)  *  OPN->ST.freqbase;
	OPN->eg_timer_overflow = ( 3 ) * (1<<EG_SH);


	/* Timer base time */
	OPN->ST.TimerBase = 1.0/((double)OPN->ST.clock / (double)TimerPres);

#if FM_SSG_PRESCALER
	/* SSG part  prescaler set */
	if( SSGpres ) SSGClk( OPN->ST.index, OPN->ST.clock * 2 / SSGpres );
#else
	(void)SSGpres;
#endif

	/* make time tables */
	init_timetables( &OPN->ST, dt_tab );

	/* there are 2048 FNUMs that can be generated using FNUM/BLK registers
		but LFO works with one more bit of a precision so we really need 4096 elements */
	/* calculate fnumber -> increment counter table */
	for(i = 0; i < 4096; i++)
	{
		/* freq table for octave 7 */
		/* OPN phase increment counter = 20bit */
		OPN->fn_table[i] = (UINT32)( (double)i * 32 * OPN->ST.freqbase * (1<<(FREQ_SH-10)) ); /* -10 because chip works with 10.10 fixed point, while we use 16.16 */
#if 0
		logerror("FM.C: fn_table[%4i] = %08x (dec=%8i)\n",
				 i, OPN->fn_table[i]>>6,OPN->fn_table[i]>>6 );
#endif
	}
	
	/* maximal frequency is required for Phase overflow calculation, register size is 17 bits (Nemesis) */
	OPN->fn_max = (UINT32)( (double)0x20000 * OPN->ST.freqbase * (1<<(FREQ_SH-10)) );

	/* LFO freq. table */
	for(i = 0; i < 8; i++)
	{
		/* Amplitude modulation: 64 output levels (triangle waveform); 1 level lasts for one of "lfo_samples_per_step" samples */
		/* Phase modulation: one entry from lfo_pm_output lasts for one of 4 * "lfo_samples_per_step" samples  */
		OPN->lfo_freq[i] = (1.0 / lfo_samples_per_step[i]) * (1<<LFO_SH) * OPN->ST.freqbase;
#if 0
		logerror("FM.C: lfo_freq[%i] = %08x (dec=%8i)\n",
				 i, OPN->lfo_freq[i],OPN->lfo_freq[i] );
#endif
	}
}



/* write a OPN mode register 0x20-0x2f */
static void OPNWriteMode(FM_OPN *OPN, int r, int v)
{
	UINT8 c;
	FM_CH *CH;

	switch(r){
	case 0x21:	/* Test */
		break;
	case 0x22:	/* LFO FREQ (YM2608/YM2610/YM2610B/YM2612) */
		if( OPN->type & TYPE_LFOPAN )
		{
			if (v&0x08) /* LFO enabled ? */
			{
				OPN->lfo_inc = OPN->lfo_freq[v&7];
			}
			else
			{
				OPN->lfo_inc = 0;
			}
		}
		break;
	case 0x24:	/* timer A High 8*/
		OPN->ST.TA = (OPN->ST.TA & 0x03)|(((int)v)<<2);
		break;
	case 0x25:	/* timer A Low 2*/
		OPN->ST.TA = (OPN->ST.TA & 0x3fc)|(v&3);
		break;
	case 0x26:	/* timer B */
		OPN->ST.TB = v;
		break;
	case 0x27:	/* mode, timer control */
		set_timers( &(OPN->ST),OPN->ST.index,v );
		break;
	case 0x28:	/* key on / off */
		c = v & 0x03;
		if( c == 3 ) break;
		if( (v&0x04) && (OPN->type & TYPE_6CH) ) c+=3;
		CH = OPN->P_CH;
		CH = &CH[c];
		if(v&0x10) FM_KEYON(OPN->type, CH,SLOT1); else FM_KEYOFF(CH,SLOT1);
		if(v&0x20) FM_KEYON(OPN->type, CH,SLOT2); else FM_KEYOFF(CH,SLOT2);
		if(v&0x40) FM_KEYON(OPN->type, CH,SLOT3); else FM_KEYOFF(CH,SLOT3);
		if(v&0x80) FM_KEYON(OPN->type, CH,SLOT4); else FM_KEYOFF(CH,SLOT4);
		break;
	}
}

/* write a OPN register (0x30-0xff) */
static void OPNWriteReg(FM_OPN *OPN, int r, int v)
{
	FM_CH *CH;
	FM_SLOT *SLOT;

	UINT8 c = OPN_CHAN(r);

	if (c == 3) return; /* 0xX3,0xX7,0xXB,0xXF */

	if (r >= 0x100) c+=3;

	CH = OPN->P_CH;
	CH = &CH[c];

	SLOT = &(CH->SLOT[OPN_SLOT(r)]);

	switch( r & 0xf0 ) {
	case 0x30:	/* DET , MUL */
		set_det_mul(&OPN->ST,CH,SLOT,v);
		break;

	case 0x40:	/* TL */
		set_tl(CH,SLOT,v);
		break;

	case 0x50:	/* KS, AR */
		set_ar_ksr(OPN->type, CH,SLOT,v);
		break;

	case 0x60:	/* bit7 = AM ENABLE, DR */
		set_dr(OPN->type, SLOT,v);

		if(OPN->type & TYPE_LFOPAN) /* YM2608/2610/2610B/2612 */
		{
			SLOT->AMmask = (v&0x80) ? ~0 : 0;
		}
		break;

	case 0x70:	/*     SR */
		set_sr(OPN->type, SLOT,v);
		break;

	case 0x80:	/* SL, RR */
		set_sl_rr(OPN->type, SLOT,v);
		break;

	case 0x90:	/* SSG-EG */
		SLOT->ssg  =  v&0x0f;
		SLOT->ssgn = (v&0x04)>>1; /* bit 1 in ssgn = attack */

		/* SSG-EG envelope shapes :

		E AtAlH
		1 0 0 0  \\\\

		1 0 0 1  \___

		1 0 1 0  \/\/
		          ___
		1 0 1 1  \

		1 1 0 0  ////
		          ___
		1 1 0 1  /

		1 1 1 0  /\/\

		1 1 1 1  /___


		E = SSG-EG enable


		The shapes are generated using Attack, Decay and Sustain phases.

		Each single character in the diagrams above represents this whole
		sequence:

		- when KEY-ON = 1, normal Attack phase is generated (*without* any
		  difference when compared to normal mode),

		- later, when envelope level reaches minimum level (max volume),
		  the EG switches to Decay phase (which works with bigger steps
		  when compared to normal mode - see below),

		- later when envelope level passes the SL level,
		  the EG swithes to Sustain phase (which works with bigger steps
		  when compared to normal mode - see below),

		- finally when envelope level reaches maximum level (min volume),
		  the EG switches to Attack phase again (depends on actual waveform).

		Important is that when switch to Attack phase occurs, the phase counter
		of that operator will be zeroed-out (as in normal KEY-ON) but not always.
		(I havent found the rule for that - perhaps only when the output level is low)

		The difference (when compared to normal Envelope Generator mode) is
		that the resolution in Decay and Sustain phases is 4 times lower;
		this results in only 256 steps instead of normal 1024.
		In other words:
		when SSG-EG is disabled, the step inside of the EG is one,
		when SSG-EG is enabled, the step is four (in Decay and Sustain phases).

		Times between the level changes are the same in both modes.


		Important:
		Decay 1 Level (so called SL) is compared to actual SSG-EG output, so
		it is the same in both SSG and no-SSG modes, with this exception:

		when the SSG-EG is enabled and is generating raising levels
		(when the EG output is inverted) the SL will be found at wrong level !!!
		For example, when SL=02:
			0 -6 = -6dB in non-inverted EG output
			96-6 = -90dB in inverted EG output
		Which means that EG compares its level to SL as usual, and that the
		output is simply inverted afterall.


		The Yamaha's manuals say that AR should be set to 0x1f (max speed).
		That is not necessary, but then EG will be generating Attack phase.

		*/


		break;

	case 0xa0:
		switch( OPN_SLOT(r) ){
		case 0:		/* 0xa0-0xa2 : FNUM1 */
			{
				UINT32 fn = (((UINT32)( (OPN->ST.fn_h)&7))<<8) + v;
				UINT8 blk = OPN->ST.fn_h>>3;
				/* keyscale code */
				CH->kcode = (blk<<2) | opn_fktable[fn >> 7];
				/* phase increment counter */
				CH->fc = OPN->fn_table[fn*2]>>(7-blk);

				/* store fnum in clear form for LFO PM calculations */
				CH->block_fnum = (blk<<11) | fn;

				CH->SLOT[SLOT1].Incr=-1;
			}
			break;
		case 1:		/* 0xa4-0xa6 : FNUM2,BLK */
			OPN->ST.fn_h = v&0x3f;
			break;
		case 2:		/* 0xa8-0xaa : 3CH FNUM1 */
			if(r < 0x100)
			{
				UINT32 fn = (((UINT32)(OPN->SL3.fn_h&7))<<8) + v;
				UINT8 blk = OPN->SL3.fn_h>>3;
				/* keyscale code */
				OPN->SL3.kcode[c]= (blk<<2) | opn_fktable[fn >> 7];
				/* phase increment counter */
				OPN->SL3.fc[c] = OPN->fn_table[fn*2]>>(7-blk);
				OPN->SL3.block_fnum[c] = (blk<<11) | fn;
				(OPN->P_CH)[2].SLOT[SLOT1].Incr=-1;
			}
			break;
		case 3:		/* 0xac-0xae : 3CH FNUM2,BLK */
			if(r < 0x100)
				OPN->SL3.fn_h = v&0x3f;
			break;
		}
		break;

	case 0xb0:
		switch( OPN_SLOT(r) ){
		case 0:		/* 0xb0-0xb2 : FB,ALGO */
			{
				int feedback = (v>>3)&7;
				CH->ALGO = v&7;
				CH->FB   = feedback ? feedback+6 : 0;
				setup_connection( CH, c );
			}
			break;
		case 1:		/* 0xb4-0xb6 : L , R , AMS , PMS (YM2612/YM2610B/YM2610/YM2608) */
			if( OPN->type & TYPE_LFOPAN)
			{
				/* b0-2 PMS */
				CH->pms = (v & 7) * 32; /* CH->pms = PM depth * 32 (index in lfo_pm_table) */

				/* b4-5 AMS */
				CH->ams = lfo_ams_depth_shift[(v>>4) & 0x03];

				/* PAN :  b7 = L, b6 = R */
				OPN->pan[ c*2   ] = (v & 0x80) ? ~0 : 0;
				OPN->pan[ c*2+1 ] = (v & 0x40) ? ~0 : 0;

			}
			break;
		}
		break;
	}
}

#endif /* BUILD_OPN */

#if BUILD_OPN_PRESCALER
/*
  prescaler circuit (best guess to verified chip behaviour)

               +--------------+  +-sel2-+
               |              +--|in20  |
         +---+ |  +-sel1-+       |      |
M-CLK -+-|1/2|-+--|in10  | +---+ |   out|--INT_CLOCK
       | +---+    |   out|-|1/3|-|in21  |
       +----------|in11  | +---+ +------+
                  +------+

reg.2d : sel2 = in21 (select sel2)
reg.2e : sel1 = in11 (select sel1)
reg.2f : sel1 = in10 , sel2 = in20 (clear selector)
reset  : sel1 = in11 , sel2 = in21 (clear both)

*/
void OPNPrescaler_w(FM_OPN *OPN , int addr, int pre_divider)
{
	static const int opn_pres[4] = { 2*12 , 2*12 , 6*12 , 3*12 };
	static const int ssg_pres[4] = { 1    ,    1 ,    4 ,    2 };
	int sel;

	switch(addr)
	{
	case 0:		/* when reset */
		OPN->ST.prescaler_sel = 2;
		break;
	case 1:		/* when postload */
		break;
	case 0x2d:	/* divider sel : select 1/1 for 1/3line    */
		OPN->ST.prescaler_sel |= 0x02;
		break;
	case 0x2e:	/* divider sel , select 1/3line for output */
		OPN->ST.prescaler_sel |= 0x01;
		break;
	case 0x2f:	/* divider sel , clear both selector to 1/2,1/2 */
		OPN->ST.prescaler_sel = 0;
		break;
	}
	sel = OPN->ST.prescaler_sel & 3;
	/* update prescaler */
	OPNSetPres( OPN,	opn_pres[sel]*pre_divider,
						opn_pres[sel]*pre_divider,
						ssg_pres[sel]*pre_divider );
}
#endif /* BUILD_OPN_PRESCALER */

#if BUILD_YM2612

#define YM2612_LOWPASS

/*******************************************************************************/
/*		YM2612 local section                                                   */
/*******************************************************************************/
/* here's the virtual YM2612 */
typedef struct
{
	UINT8		REGS[512];			/* registers			*/
	FM_OPN		OPN;				/* OPN state			*/
	FM_CH		CH[6];				/* channel state		*/
	UINT8		addr_A1;			/* address line A1		*/

#ifdef YM2612_LOWPASS
	INT32 memL, memR;
#endif

	/* dac output (YM2612) */
	int			dacen;
	INT32		dacout;
} YM2612;

static int YM2612NumChips;	/* total chip */
static YM2612 *FM2612=NULL;	/* array of YM2612's */

static int dacen;

#ifdef YM2612_LOWPASS
extern int rc_get_ym_lowpass_cutoff();
static INT16 lowpass_filter(INT32 *pMem, const int lowpass_cutoff, INT16 sample)
{
	const int b = 0x10000 - lowpass_cutoff;
	INT32 mem = ((*pMem) * b + sample * lowpass_cutoff) >> 16;
	*pMem = mem;
	return mem;
}
#endif

/* Generate samples for one of the YM2612s */
void YM2612UpdateOne(int num, INT16 *buffer, unsigned int length,
		     unsigned int volume, int loud)
{
	YM2612 *F2612 = &(FM2612[num]);
	FM_OPN *OPN   = &(FM2612[num].OPN);
	unsigned int i;
	INT32 dacout  = F2612->dacout;
#ifdef YM2612_LOWPASS
	const lowpass_cutoff = rc_get_ym_lowpass_cutoff();
#endif

	if( (void *)F2612 != cur_chip ){
		cur_chip = (void *)F2612;
		State = &OPN->ST;
		cch[0]   = &F2612->CH[0];
		cch[1]   = &F2612->CH[1];
		cch[2]   = &F2612->CH[2];
		cch[3]   = &F2612->CH[3];
		cch[4]   = &F2612->CH[4];
		cch[5]   = &F2612->CH[5];
		/* DAC mode */
		dacen = F2612->dacen;

	}

	/* refresh PG and EG */
	refresh_fc_eg_chan( OPN, cch[0] );
	refresh_fc_eg_chan( OPN, cch[1] );
	if( (State->mode & 0xc0) )
	{
		/* 3SLOT MODE */
		if( cch[2]->SLOT[SLOT1].Incr==-1)
		{
			refresh_fc_eg_slot(OPN, &cch[2]->SLOT[SLOT1] , OPN->SL3.fc[1] , OPN->SL3.kcode[1] );
			refresh_fc_eg_slot(OPN, &cch[2]->SLOT[SLOT2] , OPN->SL3.fc[2] , OPN->SL3.kcode[2] );
			refresh_fc_eg_slot(OPN, &cch[2]->SLOT[SLOT3] , OPN->SL3.fc[0] , OPN->SL3.kcode[0] );
			refresh_fc_eg_slot(OPN, &cch[2]->SLOT[SLOT4] , cch[2]->fc , cch[2]->kcode );
		}
	}else refresh_fc_eg_chan( OPN, cch[2] );
	refresh_fc_eg_chan( OPN, cch[3] );
	refresh_fc_eg_chan( OPN, cch[4] );
	refresh_fc_eg_chan( OPN, cch[5] );

	/* buffering */
	for(i=0; i < length ; i++)
	{

		advance_lfo(OPN);

		/* clear outputs */
		out_fm[0] = 0;
		out_fm[1] = 0;
		out_fm[2] = 0;
		out_fm[3] = 0;
		out_fm[4] = 0;
		out_fm[5] = 0;
		
		/* calculate FM */
		chan_calc(OPN, cch[0], 0 );
		chan_calc(OPN, cch[1], 1 );
		chan_calc(OPN, cch[2], 2 );
		chan_calc(OPN, cch[3], 3 );
		chan_calc(OPN, cch[4], 4 );
		if( dacen )
			*cch[5]->connect4 += dacout;
		else
			chan_calc(OPN, cch[5], 5 );

		/* advance envelope generator */
		OPN->eg_timer += OPN->eg_timer_add;
		while (OPN->eg_timer >= OPN->eg_timer_overflow)
		{
			OPN->eg_timer -= OPN->eg_timer_overflow;
			OPN->eg_cnt++;

			advance_eg_channel(OPN, &cch[0]->SLOT[SLOT1]);
			advance_eg_channel(OPN, &cch[1]->SLOT[SLOT1]);
			advance_eg_channel(OPN, &cch[2]->SLOT[SLOT1]);
			advance_eg_channel(OPN, &cch[3]->SLOT[SLOT1]);
			advance_eg_channel(OPN, &cch[4]->SLOT[SLOT1]);
			advance_eg_channel(OPN, &cch[5]->SLOT[SLOT1]);
		}

		{
			int32_t lt, rt;

			lt  = ((out_fm[0]>>0) & OPN->pan[0]);
			rt  = ((out_fm[0]>>0) & OPN->pan[1]);
			lt += ((out_fm[1]>>0) & OPN->pan[2]);
			rt += ((out_fm[1]>>0) & OPN->pan[3]);
			lt += ((out_fm[2]>>0) & OPN->pan[4]);
			rt += ((out_fm[2]>>0) & OPN->pan[5]);
			lt += ((out_fm[3]>>0) & OPN->pan[6]);
			rt += ((out_fm[3]>>0) & OPN->pan[7]);
			lt += ((out_fm[4]>>0) & OPN->pan[8]);
			rt += ((out_fm[4]>>0) & OPN->pan[9]);
			lt += ((out_fm[5]>>0) & OPN->pan[10]);
			rt += ((out_fm[5]>>0) & OPN->pan[11]);
			
			lt >>= FINAL_SH;
			rt >>= FINAL_SH;

#if 0
			Limit( lt, MAXOUT, MINOUT );
			Limit( rt, MAXOUT, MINOUT );
#endif

			#ifdef SAVE_SAMPLE
				SAVE_ALL_CHANNELS
			#endif

			/* Mix with buffer. */
			lt += *buffer;
			/* Make it louder. */
			if (loud)
				lt = ((lt * 3) >> 1);
			/* Lower volume? */
			if (volume != 100)
				lt = ((lt * (int)volume) / 100);
			/* Hard clipping for signed 16-bit output. */
			lt = ((abs(lt + 32767) - abs(lt - 32767)) >> 1);
#ifdef YM2612_LOWPASS
			lt = (lowpass_cutoff) ? lowpass_filter(&F2612->memL, lowpass_cutoff, lt) : lt;
#endif
			*(buffer++) = lt;

			rt += *buffer;
			if (loud)
				rt = ((rt * 3) >> 1);
			if (volume != 100)
				rt = ((rt * (int)volume) / 100);
			rt = ((abs(rt + 32767) - abs(rt - 32767)) >> 1);
#ifdef YM2612_LOWPASS
			rt = (lowpass_cutoff) ? lowpass_filter(&F2612->memR, lowpass_cutoff, rt) : rt;
#endif
			*(buffer++) = rt;
		}

		/* timer A control */
		INTERNAL_TIMER_A(OPN->type, State, cch[2])
	}
	INTERNAL_TIMER_B(State,length)

}

#ifdef _STATE_H
static void YM2612_postload(void)
{
	int num , r;

	for(num=0;num<YM2612NumChips;num++)
	{
		/* DAC data & port */
		FM2612[num].dacout = ((int)FM2612[num].REGS[0x2a] - 0x80) << 6;	/* level unknown */
		FM2612[num].dacen  = FM2612[num].REGS[0x2d] & 0x80;
		/* OPN registers */
		/* DT / MULTI , TL , KS / AR , AMON / DR , SR , SL / RR , SSG-EG */
		for(r=0x30;r<0x9e;r++)
			if((r&3) != 3)
			{
				OPNWriteReg(&FM2612[num].OPN,r,FM2612[num].REGS[r]);
				OPNWriteReg(&FM2612[num].OPN,r|0x100,FM2612[num].REGS[r|0x100]);
			}
		/* FB / CONNECT , L / R / AMS / PMS */
		for(r=0xb0;r<0xb6;r++)
			if((r&3) != 3)
			{
				OPNWriteReg(&FM2612[num].OPN,r,FM2612[num].REGS[r]);
				OPNWriteReg(&FM2612[num].OPN,r|0x100,FM2612[num].REGS[r|0x100]);
			}
		/* channels */
		/*FM_channel_postload(FM2612[num].CH,6);*/
	}
	cur_chip = NULL;
}

static void YM2612_save_state(void)
{
	int num;
	const char statename[] = "YM2612";

	for(num=0;num<YM2612NumChips;num++)
	{
		state_save_register_UINT8 (statename, num, "regs"   , FM2612[num].REGS   , 512);
		FMsave_state_st(statename,num,&FM2612[num].OPN.ST);
		FMsave_state_channel(statename,num,FM2612[num].CH,6);
		/* 3slots */
		state_save_register_UINT32 (statename, num, "slot3fc" , FM2612[num].OPN.SL3.fc ,   3);
		state_save_register_UINT8  (statename, num, "slot3fh" , &FM2612[num].OPN.SL3.fn_h, 1);
		state_save_register_UINT8  (statename, num, "slot3kc" , FM2612[num].OPN.SL3.kcode, 3);
		/* address register1 */
		state_save_register_UINT8 (statename, num, "addr_A1" , &FM2612[num].addr_A1, 1);
	}
	state_save_register_func_postload(YM2612_postload);
}
#endif /* _STATE_H */

/* initialize YM2612 emulator(s) */
int YM2612Init(int num, int clock, int rate, int mjazz,
               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler)
{
	int i;

	if (FM2612) return (-1);	/* duplicate init. */
	cur_chip = NULL;	/* hiro-shi!! */

	YM2612NumChips = num;

	/* allocate extend state space */
	if( (FM2612 = (YM2612 *)malloc(sizeof(YM2612) * YM2612NumChips))==NULL)
		return (-1);
	/* clear */
	memset(FM2612,0,sizeof(YM2612) * YM2612NumChips);
	/* allocate total level table (128kb space) */
	if( !init_tables() )
	{
		if (FM2612) {
			free( FM2612 );
			FM2612 = NULL;
		}
		return (-1);
	}

	for ( i = 0 ; i < YM2612NumChips; i++ ) {
		FM2612[i].OPN.ST.index = i;
		FM2612[i].OPN.type = TYPE_YM2612;
		FM2612[i].OPN.P_CH = FM2612[i].CH;
		FM2612[i].OPN.ST.clock = clock;
		FM2612[i].OPN.ST.rate = rate;
		if (mjazz)
			FM2612[i].OPN.ST.rate <<= i;
		/* FM2612[i].OPN.ST.irq = 0; */
		/* FM2612[i].OPN.ST.status = 0; */
		/* Extend handler */
		FM2612[i].OPN.ST.Timer_Handler = TimerHandler;
		FM2612[i].OPN.ST.IRQ_Handler   = IRQHandler;
		YM2612ResetChip(i);
	}
#ifdef _STATE_H
	YM2612_save_state();
#endif
	return 0;
}

/* shut down emulator */
void YM2612Shutdown()
{
	if (!FM2612) return;

	FMCloseTable();
	if (FM2612) {
		free(FM2612);
		FM2612 = NULL;
	}
}

/* reset one of chip */
void YM2612ResetChip(int num)
{
	int i;
	YM2612 *F2612 = &(FM2612[num]);
	FM_OPN *OPN   = &(FM2612[num].OPN);

	OPNSetPres( OPN, 6*24, 6*24, 0);
	/* status clear */
	FM_IRQMASK_SET(&OPN->ST,0x03);
	FM_BUSY_CLEAR(&OPN->ST);
	OPNWriteMode(OPN,0x27,0x30); /* mode 0 , timer reset */

	OPN->eg_timer = 0;
	OPN->eg_cnt   = 0;

	FM_STATUS_RESET(&OPN->ST, 0xff);

	reset_channels( &OPN->ST , &F2612->CH[0] , 6 );
	for(i = 0xb6 ; i >= 0xb4 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0xc0);
		OPNWriteReg(OPN,i|0x100,0xc0);
	}
	for(i = 0xb2 ; i >= 0x30 ; i-- )
	{
		OPNWriteReg(OPN,i      ,0);
		OPNWriteReg(OPN,i|0x100,0);
	}
	for(i = 0x26 ; i >= 0x20 ; i-- ) OPNWriteReg(OPN,i,0);
	/* DAC mode clear */
	F2612->dacen = 0;
}

/* YM2612 write */
/* n = number  */
/* a = address */
/* v = value   */
int YM2612Write(int n, int a, UINT8 v)
{
	YM2612 *F2612 = &(FM2612[n]);
	int addr;

	v &= 0xff;	/* adjust to 8 bit bus */

	switch( a&3){
	case 0:	/* address port 0 */
		F2612->OPN.ST.address = v;
		F2612->addr_A1 = 0;
		break;

	case 1:	/* data port 0    */
		if (F2612->addr_A1 != 0)
			break;	/* verified on real YM2608 */

		addr = F2612->OPN.ST.address;
/*#ifdef _STATE_H*/
		F2612->REGS[addr] = v;
/*#endif*/
		switch( addr & 0xf0 )
		{
		case 0x20:	/* 0x20-0x2f Mode */
			switch( addr )
			{
			case 0x2a:	/* DAC data (YM2612) */
				YM2612UpdateReq(n);
				F2612->dacout = ((int)v - 0x80) << 6;	/* level unknown */
				break;
			case 0x2b:	/* DAC Sel  (YM2612) */
				/* b7 = dac enable */
				F2612->dacen = v & 0x80;
				cur_chip = NULL;
				break;
			default:	/* OPN section */
				YM2612UpdateReq(n);
				/* write register */
				OPNWriteMode(&(F2612->OPN),addr,v);
			}
			break;
		default:	/* 0x30-0xff OPN section */
			YM2612UpdateReq(n);
			/* write register */
			OPNWriteReg(&(F2612->OPN),addr,v);
		}
		break;

	case 2:	/* address port 1 */
		F2612->OPN.ST.address = v;
		F2612->addr_A1 = 1;
		break;

	case 3:	/* data port 1    */
		if (F2612->addr_A1 != 1)
			break;	/* verified on real YM2608 */

		addr = F2612->OPN.ST.address;
/*#ifdef _STATE_H*/
		F2612->REGS[addr | 0x100] = v;
/*#endif*/
		YM2612UpdateReq(n);
		OPNWriteReg(&(F2612->OPN),addr | 0x100,v);
		break;
	}
	return F2612->OPN.ST.irq;
}

UINT8 YM2612Read(int n,int a)
{
	YM2612 *F2612 = &(FM2612[n]);

	switch( a&3){
	case 0:	/* status 0 */
		return FM_STATUS_FLAG(&F2612->OPN.ST);
	case 1:
	case 2:
	case 3:
		LOG(LOG_WAR,("YM2612 #%d:A=%d read unmapped area\n",n,a));
		return FM_STATUS_FLAG(&F2612->OPN.ST);
	}
	return 0;
}

int YM2612TimerOver(int n,int c)
{
	YM2612 *F2612 = &(FM2612[n]);

	if( c )
	{	/* Timer B */
		TimerBOver( &(F2612->OPN.ST) );
	}
	else
	{	/* Timer A */
		YM2612UpdateReq(n);
		/* timer update */
		TimerAOver( &(F2612->OPN.ST) );
		/* CSM mode key,TL controll */
		if( F2612->OPN.ST.mode & 0x80 )
		{	/* CSM mode total level latch and auto key on */
			CSMKeyControll( F2612->OPN.type, &(F2612->CH[2]) );
		}
	}
	return F2612->OPN.ST.irq;
}

/* Implemented by zamaz for dgen */
void YM2612_dump(int num, uint8_t buf[512])
{
	YM2612 *F2612 = &(FM2612[num]);

	memcpy(buf, F2612->REGS, 512);
}

/* Implemented by zamaz for dgen */
void YM2612_restore(int num, uint8_t buf[512])
{
	YM2612 *F2612 = &(FM2612[num]);
	unsigned int r;

	memcpy(F2612->REGS, buf, 512);
	/* See YM2612_postload(). */
	F2612->dacout = ((buf[0x2a] - 0x80) << 6);
	F2612->dacen  = (buf[0x2d] & 0x80);
	for (r = 0x30; (r != 0x9e); ++r) {
		if ((r & 3) == 3)
			continue;
		OPNWriteReg(&F2612->OPN, r, buf[r]);
		OPNWriteReg(&F2612->OPN, (r | 0x100), buf[(r | 0x100)]);
	}
	for (r = 0xb0; (r != 0xb6); ++r) {
		if ((r & 3) == 3)
			continue;
		OPNWriteReg(&F2612->OPN, r, buf[r]);
		OPNWriteReg(&F2612->OPN, (r | 0x100), buf[(r | 0x100)]);
	}
}

// ---------------------------------------------------------------------------
// Everything below this line is for the debugger.
// It can't be in debug.c because it needs the private structs defined here
// ---------------------------------------------------------------------------

#ifdef WITH_DEBUGGER

/*
 * I figure it is easier to extract the parameters directly rather
 * than have to interpret the structs defined above
 */

/* global ym2612 registers (YMREG_*) */
#define YMREG_LFO			0x22
#define YMREG_TIMER_A1			0x24
#define YMREG_TIMER_A2			0x25
#define YMREG_TIMER_B			0x26
#define YMREG_CH3_TIMERS		0x27
#define YMREG_KEY			0x28
#define YMREG_DAC			0x2a
#define YMREG_DAC_ENABLE		0x2b
#define YMREG_OP_SSG_EG			0x90

/* per channel ym2612 registers (YMREG_CHAN_*) */
#define YMREG_CHAN_FREQ1		0xa0
#define YMREG_CHAN_FREQ2		0xa4
#define YMREG_CHAN_CH3_OP1_FREQ1	0xa2
#define YMREG_CHAN_CH3_OP1_FREQ2	0xa6
#define YMREG_CHAN_CH3_OP2_FREQ1	0xa8
#define YMREG_CHAN_CH3_OP2_FREQ2	0xac
#define YMREG_CHAN_CH3_OP3_FREQ1	0xa9
#define YMREG_CHAN_CH3_OP3_FREQ2	0xad
#define YMREG_CHAN_CH3_OP4_FREQ1	0xaa
#define YMREG_CHAN_CH3_OP4_FREQ2	0xae
#define YMREG_CHAN_FBACK_ALGO		0xb0
#define YMREG_CHAN_LR_AMS_FMS		0xb4

/* per operator um2612 registers (YMREG_OP_*) */
#define YMREG_OP_DT1_MUL		0x30
#define YMREG_OP_TL			0x40
#define YMREG_OP_RS_AR			0x50
#define YMREG_OP_AM_D1R			0x60
#define YMREG_OP_D2R			0x70
#define YMREG_OP_D1L_RR			0x80
#define YMREG_OP_SSG_EG			0x90

/*
 * Given a channel return the part (1 or 2) and the channel offset (1-3).
 * Eg. Channel 5 is part 2 offset 1
 */
void
debug_get_chan_part_and_offset(uint8_t chan, uint8_t *part, uint8_t *offs)
{
        if ((chan >= 1) && (chan <= 3)) {
                *part = 1;
        } else if (chan <= 6) {
                *part = 2;
        } else {
		*part = 0;
		*offs = 0;
                printf("%s: bad channel: %d\n", __func__, chan);
                return;
        }

        *offs = (chan - 1) % 3;
}

#define DEBUG_PRINT_GLOBAL_VAL(r, v)	printf("  %-15s: 0x%02x\n", r, v);
void debug_show_ym2612_global_regs(uint8_t regs[512])
{
	printf("ym2612 global registers:\n");

	DEBUG_PRINT_GLOBAL_VAL("LFO Enable", (regs[YMREG_LFO] & 8) >> 3);
	DEBUG_PRINT_GLOBAL_VAL("LFO Freq.", regs[YMREG_LFO] & 7);
	DEBUG_PRINT_GLOBAL_VAL("Timer A", (regs[YMREG_TIMER_A1] & 0x3) | (regs[YMREG_TIMER_A2]));
	DEBUG_PRINT_GLOBAL_VAL("Timer B", regs[YMREG_TIMER_B]);
	DEBUG_PRINT_GLOBAL_VAL("Ch3 Mode",(regs[YMREG_CH3_TIMERS] & 0xa0) >> 6);
	DEBUG_PRINT_GLOBAL_VAL("Timer Reset B", (regs[YMREG_CH3_TIMERS] & 0x20) >> 5);
	DEBUG_PRINT_GLOBAL_VAL("Timer Reset A", (regs[YMREG_CH3_TIMERS] & 0x10) >> 4);
	DEBUG_PRINT_GLOBAL_VAL("Timer Enable B", (regs[YMREG_CH3_TIMERS] & 0x8) >> 3);
	DEBUG_PRINT_GLOBAL_VAL("Timer Enable A", (regs[YMREG_CH3_TIMERS] & 0x4) >> 2);
	DEBUG_PRINT_GLOBAL_VAL("Timer Load B", (regs[YMREG_CH3_TIMERS] & 0x2) >> 1);
	DEBUG_PRINT_GLOBAL_VAL("Timer Load A", regs[YMREG_CH3_TIMERS] & 0x2);
	DEBUG_PRINT_GLOBAL_VAL("Operator Enable", (regs[YMREG_KEY] & 0xf0) >> 4);
	DEBUG_PRINT_GLOBAL_VAL("Key Enable", regs[YMREG_KEY] & 0xf);
	DEBUG_PRINT_GLOBAL_VAL("DAC", regs[YMREG_DAC]);
	DEBUG_PRINT_GLOBAL_VAL("DAC Enable", (regs[YMREG_DAC_ENABLE] & 0x80) >> 7);
}

/* get the address of one of the operator specific registers */
uint8_t debug_get_opn_reg_addr(uint8_t reg, uint8_t ch, uint8_t op)
{
	uint8_t		part, ch_offset, addr;

	if ((ch < 1) || (ch > 6)) {
		printf("%s: bad channel: %u\n", __func__, ch);
		return (0);
	}

	if ((op < 1) || (op > 4)) {
		printf("%s: bad operator: %u\n", __func__, op);
		return (0);
	}

	debug_get_chan_part_and_offset(ch, &part, &ch_offset);
	addr = reg + ((op - 1) * 4) + ch_offset;

	if (part > 1)
		addr += 0x100; /* part 2 is stored in bytes 256 - 511 */

	return addr;
}

/* show a single slot's (aka operator's) registers (many per channel) */
#define DEBUG_PRINT_OP_VAL(r, v)	printf("      %-15s: 0x%02x\n", r, v);
void debug_show_ym2612_operator_regs(uint8_t regs[512], uint8_t ch, uint8_t op)
{
	printf("\n    ym2612 channel %u operator %u registers:\n", ch, op);

	DEBUG_PRINT_OP_VAL("Detune (DT1)",
	    (regs[debug_get_opn_reg_addr(YMREG_OP_DT1_MUL, ch, op)] & 0x70) >> 4);
	DEBUG_PRINT_OP_VAL("Multiplier (MUL)",
	    regs[debug_get_opn_reg_addr(YMREG_OP_DT1_MUL, ch, op)] & 0xf);
	DEBUG_PRINT_OP_VAL("Total Level (TL)",
	    regs[debug_get_opn_reg_addr(YMREG_OP_TL, ch, op)] & 0x7f);
	DEBUG_PRINT_OP_VAL("Rate Scaling (RS)",
	    (regs[debug_get_opn_reg_addr(YMREG_OP_RS_AR, ch, op)] & 0xc0) >> 6);
	DEBUG_PRINT_OP_VAL("Attack rate (AR)",
	    regs[debug_get_opn_reg_addr(YMREG_OP_RS_AR, ch, op)] & 0x1f);
	DEBUG_PRINT_OP_VAL("AM Enable (AM)",
	    (regs[debug_get_opn_reg_addr(YMREG_OP_AM_D1R, ch, op)] & 0x80) >> 7);
	DEBUG_PRINT_OP_VAL("First Decay Rate (D1R)",
	    regs[debug_get_opn_reg_addr(YMREG_OP_AM_D1R, ch, op)] & 0x1f);
	DEBUG_PRINT_OP_VAL("Second Decay Rate (D2R)",
	    regs[debug_get_opn_reg_addr(YMREG_OP_D2R, ch, op)] & 0x1f);
	DEBUG_PRINT_OP_VAL("Secondary Level (D1L)",
	    (regs[debug_get_opn_reg_addr(YMREG_OP_D1L_RR, ch, op)] & 0xf0) >> 4);
	DEBUG_PRINT_OP_VAL("Release Rate (RR)",
	    regs[debug_get_opn_reg_addr(YMREG_OP_D1L_RR, ch, op)] & 0xf);
	DEBUG_PRINT_OP_VAL("Sega Proproetary (SSG-EG)",
	    regs[debug_get_opn_reg_addr(YMREG_OP_SSG_EG, ch, op)] & 0xf);
}


/* get the address of one of the operator specific registers */
uint8_t debug_get_chan_reg_addr(uint8_t reg, uint8_t ch)
{
	uint8_t		part, ch_offset;

	if ((ch < 1) || (ch > 6)) {
		printf("%s: bad channel: %u\n", __func__, ch);
		return (0);
	}

	debug_get_chan_part_and_offset(ch, &part, &ch_offset);
	return (reg + ch_offset + ((part - 1) * 0x100));
}
/* shows a single channel's regsiters */
#define DEBUG_MAX_OPS			4
#define DEBUG_PRINT_CHAN_VAL(r, v)	printf("    %-15s: 0x%02x\n", r, v);
void debug_show_ym2612_chan_regs(uint8_t regs[512], uint8_t ch)
{
	uint8_t			op;

	printf("\n  ym2612 channel %u registers:\n", ch);

	DEBUG_PRINT_CHAN_VAL("Octave",
	    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_FREQ2, ch)]) & 0x38) >> 3);

	DEBUG_PRINT_CHAN_VAL("Frequency",
	    (regs[debug_get_chan_reg_addr(YMREG_CHAN_FREQ2, ch)] & 0x7) << 8 |
	    regs[debug_get_chan_reg_addr(YMREG_CHAN_FREQ1, ch)]);

	/* channel 3 and 6 can have separate freq's per op */
	if ((ch % 3) == 0) {
			DEBUG_PRINT_CHAN_VAL("Supp. Freq 1",
			    (regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP1_FREQ2, ch)] & 0x7) << 8 |
			    regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP1_FREQ1, ch)]);
			DEBUG_PRINT_CHAN_VAL("Supp. Octave 1",
			    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP1_FREQ2, ch)]) & 0x38) >> 3);

			DEBUG_PRINT_CHAN_VAL("Supp. Freq 2",
			    (regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP2_FREQ2, ch)] & 0x7) << 8 |
			    regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP2_FREQ1, ch)]);
			DEBUG_PRINT_CHAN_VAL("Supp. Octave 2",
			    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP2_FREQ2, ch)]) & 0x38) >> 3);

			DEBUG_PRINT_CHAN_VAL("Supp. Freq 3",
			    (regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP3_FREQ2, ch)] & 0x7) << 8 |
			    regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP3_FREQ1, ch)]);
			DEBUG_PRINT_CHAN_VAL("Supp. Octave 3",
			    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP3_FREQ2, ch)]) & 0x38) >> 3);

			DEBUG_PRINT_CHAN_VAL("Supp. Freq 4",
			    (regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP3_FREQ2, ch)] & 0x7) << 8 |
			    regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP3_FREQ1, ch)]);
			DEBUG_PRINT_CHAN_VAL("Supp. Octave 4",
			    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_CH3_OP1_FREQ2, ch)]) & 0x38) >> 3);
	}

	DEBUG_PRINT_CHAN_VAL("Feedback",
	    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_FBACK_ALGO, ch)]) & 0x38) >> 3);
	DEBUG_PRINT_CHAN_VAL("Algorithm",
	    (regs[debug_get_chan_reg_addr(YMREG_CHAN_FBACK_ALGO, ch)]) & 0x7);
	DEBUG_PRINT_CHAN_VAL("Stereo L",
	    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_LR_AMS_FMS, ch)]) & 0x80) >> 7);
	DEBUG_PRINT_CHAN_VAL("Stereo R",
	    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_LR_AMS_FMS, ch)]) & 0x40) >> 6);
	DEBUG_PRINT_CHAN_VAL("AM Sensitivity",
	    ((regs[debug_get_chan_reg_addr(YMREG_CHAN_LR_AMS_FMS, ch)]) & 0x30) >> 4);
	DEBUG_PRINT_CHAN_VAL("FM Sensitivity",
	    (regs[debug_get_chan_reg_addr(YMREG_CHAN_LR_AMS_FMS, ch)]) & 0x7);


	for (op = 1; op <= DEBUG_MAX_OPS; op++)
		debug_show_ym2612_operator_regs(regs, ch, op);
}

#define DEBUG_MAX_CHAN				6
void debug_show_ym2612_regs()
{
	uint8_t			regs[512], chan;

	YM2612_dump(0, regs);

	printf("ym2612:\n");
	debug_show_ym2612_global_regs(regs);

	for (chan = 1; chan <= DEBUG_MAX_CHAN; chan ++)
		debug_show_ym2612_chan_regs(regs, chan);
}
#endif

#endif /* BUILD_YM2612 */
