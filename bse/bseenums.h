/* BSE - Bedevilled Sound Engine
 * Copyright (C) 1998, 1999 Olaf Hoehmann and Tim Janik
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * bseenums.h: main portion of enum definitions in BSE and convenience functions
 */
#ifndef __BSE_ENUMS_H__
#define __BSE_ENUMS_H__

#include	<bse/bsetype.h>
#include	<gsl/gsldefs.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* --- enum definitions --- */
typedef enum
{
  BSE_LITTLE_ENDIAN	= G_LITTLE_ENDIAN	/* 1234 */,
  BSE_BIG_ENDIAN	= G_BIG_ENDIAN		/* 4321 */
} BseEndianType;
#define	BSE_BYTE_ORDER	G_BYTE_ORDER
typedef enum
{
  BSE_INTERPOL_NONE,		/*< nick=None >*/
  BSE_INTERPOL_LINEAR,		/*< nick=Linear >*/
  BSE_INTERPOL_CUBIC		/*< nick=Cubic >*/
} BseInterpolType;
typedef enum
{
  BSE_LOOP_NONE,
  BSE_LOOP_PATTERN,
  BSE_LOOP_PATTERN_ROWS,
  BSE_LOOP_SONG,
  BSE_LOOP_LAST				/*< skip >*/
} BseLoopType;
typedef enum
{
  BSE_MAGIC_BSE_BIN_EXTENSION   = 1 << 0,
  BSE_MAGIC_BSE_SONG            = 1 << 2
} BseMagicFlags;
typedef enum
{
  /* GSL errors are mirrored into BSE */
  BSE_ERROR_NONE		= GSL_ERROR_NONE,	/* 0 */
  BSE_ERROR_INTERNAL		= GSL_ERROR_INTERNAL,
  BSE_ERROR_UNKNOWN		= GSL_ERROR_UNKNOWN,
  /* I/O errors */
  BSE_ERROR_IO			= GSL_ERROR_IO,
  BSE_ERROR_PERMS		= GSL_ERROR_PERMS,
  BSE_ERROR_EOF			= GSL_ERROR_EOF,
  BSE_ERROR_NOT_FOUND		= GSL_ERROR_NOT_FOUND,
  BSE_ERROR_OPEN_FAILED		= GSL_ERROR_OPEN_FAILED,
  BSE_ERROR_SEEK_FAILED		= GSL_ERROR_SEEK_FAILED,
  BSE_ERROR_READ_FAILED		= GSL_ERROR_READ_FAILED,
  BSE_ERROR_WRITE_FAILED	= GSL_ERROR_WRITE_FAILED,
  /* content errors */
  BSE_ERROR_FORMAT_INVALID	= GSL_ERROR_FORMAT_INVALID,
  BSE_ERROR_FORMAT_UNKNOWN	= GSL_ERROR_FORMAT_UNKNOWN,
  BSE_ERROR_DATA_CORRUPT	= GSL_ERROR_DATA_CORRUPT,
  /* miscellaneous errors */
  BSE_ERROR_CODEC_FAILURE	= GSL_ERROR_CODEC_FAILURE,

  /* BSE errors */
  BSE_ERROR_UNIMPLEMENTED	= GSL_ERROR_LAST,
  BSE_ERROR_NOT_OWNER,
  /* File, Loading/Saving errors */
#define BSE_ERROR_FILE_IO		BSE_ERROR_IO
#define BSE_ERROR_FILE_NOT_FOUND	BSE_ERROR_NOT_FOUND
  BSE_ERROR_FILE_EXISTS,
  BSE_ERROR_FILE_TOO_SHORT,
  BSE_ERROR_FILE_TOO_LONG,
#define BSE_ERROR_FORMAT_MISMATCH	BSE_ERROR_FORMAT_INVALID
#define	BSE_ERROR_FORMAT_TOO_NEW	BSE_ERROR_FORMAT_UNKNOWN
#define	BSE_ERROR_FORMAT_TOO_OLD	BSE_ERROR_FORMAT_UNKNOWN
  BSE_ERROR_HEADER_CORRUPT,
  BSE_ERROR_SUB_HEADER_CORRUPT,
  BSE_ERROR_BINARY_DATA_CORRUPT,
  BSE_ERROR_PARSE_ERROR,
  /* Device errors */
#define BSE_ERROR_DEVICE_IO	BSE_ERROR_IO
#define BSE_ERROR_DEVICE_PERMS	BSE_ERROR_PERMS
  BSE_ERROR_DEVICE_ASYNC,
  BSE_ERROR_DEVICE_BUSY,
  BSE_ERROR_DEVICE_GET_CAPS,
  BSE_ERROR_DEVICE_CAPS_MISMATCH,
  BSE_ERROR_DEVICE_SET_CAPS,
  /* Date parsing errors */
  BSE_ERROR_DATE_INVALID,
  BSE_ERROR_DATE_CLUTTERED,
  BSE_ERROR_DATE_YEAR_BOUNDS,
  BSE_ERROR_DATE_MONTH_BOUNDS,
  BSE_ERROR_DATE_DAY_BOUNDS,
  BSE_ERROR_DATE_HOUR_BOUNDS,
  BSE_ERROR_DATE_MINUTE_BOUNDS,
  BSE_ERROR_DATE_SECOND_BOUNDS,
  BSE_ERROR_DATE_TOO_OLD,
  /* BseSource errors */
  BSE_ERROR_SOURCE_NO_SUCH_ICHANNEL,
  BSE_ERROR_SOURCE_NO_SUCH_OCHANNEL,
  BSE_ERROR_SOURCE_NO_SUCH_CONNECTION,
  BSE_ERROR_SOURCE_ICHANNEL_IN_USE,
  BSE_ERROR_SOURCE_CHANNELS_CONNECTED,
  BSE_ERROR_SOURCE_BAD_LOOPBACK,
  BSE_ERROR_SOURCE_BUSY,
  BSE_ERROR_SOURCE_TYPE_INVALID,
  /* BseProcedure invocation errors */
  BSE_ERROR_PROC_BUSY,
  BSE_ERROR_PROC_PARAM_INVAL,
  BSE_ERROR_PROC_EXECUTION,
  BSE_ERROR_PROC_ABORT,
  /* misc procedure errors */
  BSE_ERROR_INVALID_OFFSET,
  BSE_ERROR_INVALID_DURATION,
  BSE_ERROR_INVALID_OVERLAP,
  /* BseServer errors */
  BSE_ERROR_NO_PCM_DEVICE,
  BSE_ERROR_PCM_DEVICE_ACTIVE,
  BSE_ERROR_LAST			/*< skip >*/
} BseErrorType;
/* --- MIDI signals --- */
typedef enum
{
  BSE_MIDI_SIGNAL_CONTROL,      /* + 0..127 */
  BSE_MIDI_SIGNAL_PROGRAM       = 128,
  BSE_MIDI_SIGNAL_PRESSURE,
  BSE_MIDI_SIGNAL_PITCH_BEND,
  BSE_MIDI_SIGNAL_FREQUENCY,    /* of note */
  BSE_MIDI_SIGNAL_GATE,         /* of note */
  BSE_MIDI_SIGNAL_VELOCITY,     /* of note */
  BSE_MIDI_SIGNAL_AFTERTOUCH,   /* of note */
  BSE_MIDI_SIGNAL_LAST
} BseMidiSignalType;
/* --- MIDI controls --- */
typedef enum	/*< prefix=BSE_MIDI >*/
{
  BSE_MIDI_CONTROL_PROGRAM	= BSE_MIDI_SIGNAL_PROGRAM,	  /*< nick=Program Change >*/
  BSE_MIDI_CONTROL_PRESSURE	= BSE_MIDI_SIGNAL_PRESSURE,	  /*< nick=Channel Pressure >*/
  BSE_MIDI_CONTROL_PITCH_BEND	= BSE_MIDI_SIGNAL_PITCH_BEND,	  /*< nick=Pitch Bend >*/
  BSE_MIDI_CONTROL_0		= BSE_MIDI_SIGNAL_CONTROL +   0,
  BSE_MIDI_CONTROL_1		= BSE_MIDI_SIGNAL_CONTROL +   1,  /*< nick=Control 1 Modulation >*/
  BSE_MIDI_CONTROL_2		= BSE_MIDI_SIGNAL_CONTROL +   2,  /*< nick=Control 2 Breath Control >*/
  BSE_MIDI_CONTROL_3		= BSE_MIDI_SIGNAL_CONTROL +   3,
  BSE_MIDI_CONTROL_4		= BSE_MIDI_SIGNAL_CONTROL +   4,  /*< nick=Control 4 Foot Controller >*/
  BSE_MIDI_CONTROL_5		= BSE_MIDI_SIGNAL_CONTROL +   5,  /*< nick=Control 5 Portamento Time >*/
  BSE_MIDI_CONTROL_6		= BSE_MIDI_SIGNAL_CONTROL +   6,  /*< nick=Control 6 Data Entry >*/
  BSE_MIDI_CONTROL_7		= BSE_MIDI_SIGNAL_CONTROL +   7,  /*< nick=Control 7 Volume >*/
  BSE_MIDI_CONTROL_8		= BSE_MIDI_SIGNAL_CONTROL +   8,  /*< nick=Control 8 Balance >*/
  BSE_MIDI_CONTROL_9		= BSE_MIDI_SIGNAL_CONTROL +   9,
  BSE_MIDI_CONTROL_10		= BSE_MIDI_SIGNAL_CONTROL +  10,  /*< nick=Control 10 Panorama >*/
  BSE_MIDI_CONTROL_11		= BSE_MIDI_SIGNAL_CONTROL +  11,  /*< nick=Control 11 Expression >*/
  BSE_MIDI_CONTROL_12		= BSE_MIDI_SIGNAL_CONTROL +  12,
  BSE_MIDI_CONTROL_13		= BSE_MIDI_SIGNAL_CONTROL +  13,
  BSE_MIDI_CONTROL_14		= BSE_MIDI_SIGNAL_CONTROL +  14,
  BSE_MIDI_CONTROL_15		= BSE_MIDI_SIGNAL_CONTROL +  15,
  BSE_MIDI_CONTROL_16		= BSE_MIDI_SIGNAL_CONTROL +  16,  /*< nick=Control 16 General Purpose Controller 1 >*/
  BSE_MIDI_CONTROL_17		= BSE_MIDI_SIGNAL_CONTROL +  17,  /*< nick=Control 17 General Purpose Controller 2 >*/
  BSE_MIDI_CONTROL_18		= BSE_MIDI_SIGNAL_CONTROL +  18,  /*< nick=Control 18 General Purpose Controller 3 >*/
  BSE_MIDI_CONTROL_19		= BSE_MIDI_SIGNAL_CONTROL +  19,  /*< nick=Control 19 General Purpose Controller 4 >*/
  BSE_MIDI_CONTROL_20		= BSE_MIDI_SIGNAL_CONTROL +  20,
  BSE_MIDI_CONTROL_21		= BSE_MIDI_SIGNAL_CONTROL +  21,
  BSE_MIDI_CONTROL_22		= BSE_MIDI_SIGNAL_CONTROL +  22,
  BSE_MIDI_CONTROL_23		= BSE_MIDI_SIGNAL_CONTROL +  23,
  BSE_MIDI_CONTROL_24		= BSE_MIDI_SIGNAL_CONTROL +  24,
  BSE_MIDI_CONTROL_25		= BSE_MIDI_SIGNAL_CONTROL +  25,
  BSE_MIDI_CONTROL_26		= BSE_MIDI_SIGNAL_CONTROL +  26,
  BSE_MIDI_CONTROL_27		= BSE_MIDI_SIGNAL_CONTROL +  27,
  BSE_MIDI_CONTROL_28		= BSE_MIDI_SIGNAL_CONTROL +  28,
  BSE_MIDI_CONTROL_29		= BSE_MIDI_SIGNAL_CONTROL +  29,
  BSE_MIDI_CONTROL_30		= BSE_MIDI_SIGNAL_CONTROL +  30,
  BSE_MIDI_CONTROL_31		= BSE_MIDI_SIGNAL_CONTROL +  31,
  BSE_MIDI_CONTROL_32		= BSE_MIDI_SIGNAL_CONTROL +  32,
  BSE_MIDI_CONTROL_33		= BSE_MIDI_SIGNAL_CONTROL +  33,
  BSE_MIDI_CONTROL_34		= BSE_MIDI_SIGNAL_CONTROL +  34,
  BSE_MIDI_CONTROL_35		= BSE_MIDI_SIGNAL_CONTROL +  35,
  BSE_MIDI_CONTROL_36		= BSE_MIDI_SIGNAL_CONTROL +  36,
  BSE_MIDI_CONTROL_37		= BSE_MIDI_SIGNAL_CONTROL +  37,
  BSE_MIDI_CONTROL_38		= BSE_MIDI_SIGNAL_CONTROL +  38,
  BSE_MIDI_CONTROL_39		= BSE_MIDI_SIGNAL_CONTROL +  39,
  BSE_MIDI_CONTROL_40		= BSE_MIDI_SIGNAL_CONTROL +  40,
  BSE_MIDI_CONTROL_41		= BSE_MIDI_SIGNAL_CONTROL +  41,
  BSE_MIDI_CONTROL_42		= BSE_MIDI_SIGNAL_CONTROL +  42,
  BSE_MIDI_CONTROL_43		= BSE_MIDI_SIGNAL_CONTROL +  43,
  BSE_MIDI_CONTROL_44		= BSE_MIDI_SIGNAL_CONTROL +  44,
  BSE_MIDI_CONTROL_45		= BSE_MIDI_SIGNAL_CONTROL +  45,
  BSE_MIDI_CONTROL_46		= BSE_MIDI_SIGNAL_CONTROL +  46,
  BSE_MIDI_CONTROL_47		= BSE_MIDI_SIGNAL_CONTROL +  47,
  BSE_MIDI_CONTROL_48		= BSE_MIDI_SIGNAL_CONTROL +  48,
  BSE_MIDI_CONTROL_49		= BSE_MIDI_SIGNAL_CONTROL +  49,
  BSE_MIDI_CONTROL_50		= BSE_MIDI_SIGNAL_CONTROL +  50,
  BSE_MIDI_CONTROL_51		= BSE_MIDI_SIGNAL_CONTROL +  51,
  BSE_MIDI_CONTROL_52		= BSE_MIDI_SIGNAL_CONTROL +  52,
  BSE_MIDI_CONTROL_53		= BSE_MIDI_SIGNAL_CONTROL +  53,
  BSE_MIDI_CONTROL_54		= BSE_MIDI_SIGNAL_CONTROL +  54,
  BSE_MIDI_CONTROL_55		= BSE_MIDI_SIGNAL_CONTROL +  55,
  BSE_MIDI_CONTROL_56		= BSE_MIDI_SIGNAL_CONTROL +  56,
  BSE_MIDI_CONTROL_57		= BSE_MIDI_SIGNAL_CONTROL +  57,
  BSE_MIDI_CONTROL_58		= BSE_MIDI_SIGNAL_CONTROL +  58,
  BSE_MIDI_CONTROL_59		= BSE_MIDI_SIGNAL_CONTROL +  59,
  BSE_MIDI_CONTROL_60		= BSE_MIDI_SIGNAL_CONTROL +  60,
  BSE_MIDI_CONTROL_61		= BSE_MIDI_SIGNAL_CONTROL +  61,
  BSE_MIDI_CONTROL_62		= BSE_MIDI_SIGNAL_CONTROL +  62,
  BSE_MIDI_CONTROL_63		= BSE_MIDI_SIGNAL_CONTROL +  63,
  BSE_MIDI_CONTROL_64		= BSE_MIDI_SIGNAL_CONTROL +  64,
  BSE_MIDI_CONTROL_65		= BSE_MIDI_SIGNAL_CONTROL +  65,
  BSE_MIDI_CONTROL_66		= BSE_MIDI_SIGNAL_CONTROL +  66,
  BSE_MIDI_CONTROL_67		= BSE_MIDI_SIGNAL_CONTROL +  67,
  BSE_MIDI_CONTROL_68		= BSE_MIDI_SIGNAL_CONTROL +  68,
  BSE_MIDI_CONTROL_69		= BSE_MIDI_SIGNAL_CONTROL +  69,
  BSE_MIDI_CONTROL_70		= BSE_MIDI_SIGNAL_CONTROL +  70,
  BSE_MIDI_CONTROL_71		= BSE_MIDI_SIGNAL_CONTROL +  71,
  BSE_MIDI_CONTROL_72		= BSE_MIDI_SIGNAL_CONTROL +  72,
  BSE_MIDI_CONTROL_73		= BSE_MIDI_SIGNAL_CONTROL +  73,
  BSE_MIDI_CONTROL_74		= BSE_MIDI_SIGNAL_CONTROL +  74,
  BSE_MIDI_CONTROL_75		= BSE_MIDI_SIGNAL_CONTROL +  75,
  BSE_MIDI_CONTROL_76		= BSE_MIDI_SIGNAL_CONTROL +  76,
  BSE_MIDI_CONTROL_77		= BSE_MIDI_SIGNAL_CONTROL +  77,
  BSE_MIDI_CONTROL_78		= BSE_MIDI_SIGNAL_CONTROL +  78,
  BSE_MIDI_CONTROL_79		= BSE_MIDI_SIGNAL_CONTROL +  79,
  BSE_MIDI_CONTROL_80		= BSE_MIDI_SIGNAL_CONTROL +  80,
  BSE_MIDI_CONTROL_81		= BSE_MIDI_SIGNAL_CONTROL +  81,
  BSE_MIDI_CONTROL_82		= BSE_MIDI_SIGNAL_CONTROL +  82,
  BSE_MIDI_CONTROL_83		= BSE_MIDI_SIGNAL_CONTROL +  83,
  BSE_MIDI_CONTROL_84		= BSE_MIDI_SIGNAL_CONTROL +  84,  /*< nick=Control 84 Portamento Control >*/
  BSE_MIDI_CONTROL_85		= BSE_MIDI_SIGNAL_CONTROL +  85,
  BSE_MIDI_CONTROL_86		= BSE_MIDI_SIGNAL_CONTROL +  86,
  BSE_MIDI_CONTROL_87		= BSE_MIDI_SIGNAL_CONTROL +  87,
  BSE_MIDI_CONTROL_88		= BSE_MIDI_SIGNAL_CONTROL +  88,
  BSE_MIDI_CONTROL_89		= BSE_MIDI_SIGNAL_CONTROL +  89,
  BSE_MIDI_CONTROL_90		= BSE_MIDI_SIGNAL_CONTROL +  90,
  BSE_MIDI_CONTROL_91		= BSE_MIDI_SIGNAL_CONTROL +  91,  /*< nick=Control 91 Reverb Depth >*/
  BSE_MIDI_CONTROL_92		= BSE_MIDI_SIGNAL_CONTROL +  92,  /*< nick=Control 92 Tremolo Depth >*/
  BSE_MIDI_CONTROL_93		= BSE_MIDI_SIGNAL_CONTROL +  93,  /*< nick=Control 93 Chorus Depth >*/
  BSE_MIDI_CONTROL_94		= BSE_MIDI_SIGNAL_CONTROL +  94,
  BSE_MIDI_CONTROL_95		= BSE_MIDI_SIGNAL_CONTROL +  95,  /*< nick=Control 95 Phase Depth >*/
  BSE_MIDI_CONTROL_96		= BSE_MIDI_SIGNAL_CONTROL +  96,  /*< nick=Control 96 Data Increment >*/
  BSE_MIDI_CONTROL_97		= BSE_MIDI_SIGNAL_CONTROL +  97,  /*< nick=Control 97 Data Decrement >*/
  BSE_MIDI_CONTROL_98		= BSE_MIDI_SIGNAL_CONTROL +  98,
  BSE_MIDI_CONTROL_99		= BSE_MIDI_SIGNAL_CONTROL +  99,
  BSE_MIDI_CONTROL_100		= BSE_MIDI_SIGNAL_CONTROL + 100,
  BSE_MIDI_CONTROL_101		= BSE_MIDI_SIGNAL_CONTROL + 101,
  BSE_MIDI_CONTROL_102		= BSE_MIDI_SIGNAL_CONTROL + 102,
  BSE_MIDI_CONTROL_103		= BSE_MIDI_SIGNAL_CONTROL + 103,
  BSE_MIDI_CONTROL_104		= BSE_MIDI_SIGNAL_CONTROL + 104,
  BSE_MIDI_CONTROL_105		= BSE_MIDI_SIGNAL_CONTROL + 105,
  BSE_MIDI_CONTROL_106		= BSE_MIDI_SIGNAL_CONTROL + 106,
  BSE_MIDI_CONTROL_107		= BSE_MIDI_SIGNAL_CONTROL + 107,
  BSE_MIDI_CONTROL_108		= BSE_MIDI_SIGNAL_CONTROL + 108,
  BSE_MIDI_CONTROL_109		= BSE_MIDI_SIGNAL_CONTROL + 109,
  BSE_MIDI_CONTROL_110		= BSE_MIDI_SIGNAL_CONTROL + 110,
  BSE_MIDI_CONTROL_111		= BSE_MIDI_SIGNAL_CONTROL + 111,
  BSE_MIDI_CONTROL_112		= BSE_MIDI_SIGNAL_CONTROL + 112,
  BSE_MIDI_CONTROL_113		= BSE_MIDI_SIGNAL_CONTROL + 113,
  BSE_MIDI_CONTROL_114		= BSE_MIDI_SIGNAL_CONTROL + 114,
  BSE_MIDI_CONTROL_115		= BSE_MIDI_SIGNAL_CONTROL + 115,
  BSE_MIDI_CONTROL_116		= BSE_MIDI_SIGNAL_CONTROL + 116,
  BSE_MIDI_CONTROL_117		= BSE_MIDI_SIGNAL_CONTROL + 117,
  BSE_MIDI_CONTROL_118		= BSE_MIDI_SIGNAL_CONTROL + 118,
  BSE_MIDI_CONTROL_119		= BSE_MIDI_SIGNAL_CONTROL + 119,
  BSE_MIDI_CONTROL_120		= BSE_MIDI_SIGNAL_CONTROL + 120,
  BSE_MIDI_CONTROL_121		= BSE_MIDI_SIGNAL_CONTROL + 121,
  BSE_MIDI_CONTROL_122		= BSE_MIDI_SIGNAL_CONTROL + 122,
  BSE_MIDI_CONTROL_123		= BSE_MIDI_SIGNAL_CONTROL + 123,
  BSE_MIDI_CONTROL_124		= BSE_MIDI_SIGNAL_CONTROL + 124,
  BSE_MIDI_CONTROL_125		= BSE_MIDI_SIGNAL_CONTROL + 125,
  BSE_MIDI_CONTROL_126		= BSE_MIDI_SIGNAL_CONTROL + 126,
  BSE_MIDI_CONTROL_127		= BSE_MIDI_SIGNAL_CONTROL + 127
} BseMidiControlType;


/* --- convenience functions --- */
gchar*		bse_error_name			(BseErrorType	 error_value);
gchar*		bse_error_nick			(BseErrorType	 error_value);
gchar*		bse_error_blurb			(BseErrorType	 error_value);
BseErrorType	bse_error_from_errno		(gint		 v_errno,
						 BseErrorType    fallback);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BSE_ENUMS_H__ */
