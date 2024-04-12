#pragma once

/** Defines **/
// Debug levels:
// 0: no debug printouts
// 1: only print errors
// 2: only print errors and warnings
// 3: print errors, warnings, and notes
#define DEBUG_LEVEL	3



/** Macros **/
#if (DEBUG_LEVEL >= 1)
#define DEBUG_ERROR(_msg)	SerialUSB.println("ERROR at T=" + String(millis()) + ": " + String(_msg))
#else	// #if (DEBUG_LEVEL >= 1)
#define DEBUG_ERROR(_msg)
#endif	// #if (DEBUG_LEVEL >= 1)

#if (DEBUG_LEVEL >= 2)
#define DEBUG_WARNING(_msg)	SerialUSB.println("WARN at T=" + String(millis()) + ": " + String(_msg))
#else	// #if (DEBUG_LEVEL >= 1)
#define DEBUG_WARNING(_msg)
#endif	// #if (DEBUG_LEVEL >= 1)

#if (DEBUG_LEVEL >= 3)
#define DEBUG_NOTE(_msg)	SerialUSB.println("NOTE at T=" + String(millis()) + ": " + String(_msg))
#else	// #if (DEBUG_LEVEL >= 1)
#define DEBUG_NOTE(_msg)
#endif	// #if (DEBUG_LEVEL >= 1)
