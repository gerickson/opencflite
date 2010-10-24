/*
 * Copyright (c) 2009 Brent Fulgham <bfulgham@gmail.org>.  All rights reserved.
 *
 * This source code is a modified version of the CoreFoundation sources released by Apple Inc. under
 * the terms of the APSL version 2.0 (see below).
 *
 * Note: This is just a C-compatible version of CFLocaleKeys.m.
 *
 * For information about changes from the original Apple source release can be found by reviewing the
 * source control system for the project at https://sourceforge.net/svn/?group_id=246198.
 *
 * The original license information is as follows:
 * 
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CFLocale.h>
#include "CFInternal.h"
#include "CFLocaleInternal.h"

// Remember to keep the names such that they would make sense for the user locale,
// in addition to the others; for example, it is "Currency", not "DefaultCurrency".
// (And besides, "Default" is almost always implied.)  Words like "Default" and
// "Preferred" and so on should be left out of the names.
CONST_STRING_DECL(kCFLocaleIdentifier, "locale:id")
CONST_STRING_DECL(kCFLocaleLanguageCode, "locale:language code")
CONST_STRING_DECL(kCFLocaleCountryCode, "locale:country code")
CONST_STRING_DECL(kCFLocaleScriptCode, "locale:script code")
CONST_STRING_DECL(kCFLocaleVariantCode, "locale:variant code")
CONST_STRING_DECL(kCFLocaleExemplarCharacterSet, "locale:exemplar characters")
CONST_STRING_DECL(kCFLocaleCalendarIdentifier, "calendar")
CONST_STRING_DECL(kCFLocaleCalendar, "locale:calendarref")
CONST_STRING_DECL(kCFLocaleCollationIdentifier, "collation")
CONST_STRING_DECL(kCFLocaleUsesMetricSystem, "locale:uses metric")
CONST_STRING_DECL(kCFLocaleMeasurementSystem, "locale:measurement system")
CONST_STRING_DECL(kCFLocaleDecimalSeparator, "locale:decimal separator")
CONST_STRING_DECL(kCFLocaleGroupingSeparator, "locale:grouping separator")
CONST_STRING_DECL(kCFLocaleCurrencySymbol, "locale:currency symbol")
CONST_STRING_DECL(kCFLocaleCurrencyCode, "currency")

CONST_STRING_DECL(kCFLocaleAlternateQuotationBeginDelimiterKey, "kCFLocaleAlternateQuotationBeginDelimiterKey")
CONST_STRING_DECL(kCFLocaleAlternateQuotationEndDelimiterKey, "kCFLocaleAlternateQuotationEndDelimiterKey")
CONST_STRING_DECL(kCFLocaleQuotationBeginDelimiterKey, "kCFLocaleQuotationBeginDelimiterKey")
CONST_STRING_DECL(kCFLocaleQuotationEndDelimiterKey, "kCFLocaleQuotationEndDelimiterKey")
CONST_STRING_DECL(kCFLocaleCalendarIdentifierKey, "calendar")
CONST_STRING_DECL(kCFLocaleCalendarKey, "kCFLocaleCalendarKey")
CONST_STRING_DECL(kCFLocaleCollationIdentifierKey, "collation")
CONST_STRING_DECL(kCFLocaleCollatorIdentifierKey, "kCFLocaleCollatorIdentifierKey")
CONST_STRING_DECL(kCFLocaleCountryCodeKey, "kCFLocaleCountryCodeKey")
CONST_STRING_DECL(kCFLocaleCurrencyCodeKey, "currency")
CONST_STRING_DECL(kCFLocaleCurrencySymbolKey, "kCFLocaleCurrencySymbolKey")
CONST_STRING_DECL(kCFLocaleDecimalSeparatorKey, "kCFLocaleDecimalSeparatorKey")
CONST_STRING_DECL(kCFLocaleExemplarCharacterSetKey, "kCFLocaleExemplarCharacterSetKey")
CONST_STRING_DECL(kCFLocaleGroupingSeparatorKey, "kCFLocaleGroupingSeparatorKey")
CONST_STRING_DECL(kCFLocaleIdentifierKey, "kCFLocaleIdentifierKey")
CONST_STRING_DECL(kCFLocaleLanguageCodeKey, "kCFLocaleLanguageCodeKey")
CONST_STRING_DECL(kCFLocaleMeasurementSystemKey, "kCFLocaleMeasurementSystemKey")
CONST_STRING_DECL(kCFLocaleScriptCodeKey, "kCFLocaleScriptCodeKey")
CONST_STRING_DECL(kCFLocaleUsesMetricSystemKey, "kCFLocaleUsesMetricSystemKey")
CONST_STRING_DECL(kCFLocaleVariantCodeKey, "kCFLocaleVariantCodeKey")

CONST_STRING_DECL(kCFDateFormatterAMSymbolKey, "kCFDateFormatterAMSymbolKey")
CONST_STRING_DECL(kCFDateFormatterCalendarKey, "kCFDateFormatterCalendarKey")
CONST_STRING_DECL(kCFDateFormatterCalendarIdentifierKey, "kCFDateFormatterCalendarIdentifierKey")
CONST_STRING_DECL(kCFDateFormatterDefaultDateKey, "kCFDateFormatterDefaultDateKey")
CONST_STRING_DECL(kCFDateFormatterDefaultFormatKey, "kCFDateFormatterDefaultFormatKey")
CONST_STRING_DECL(kCFDateFormatterDoesRelativeDateFormattingKey, "kCFDateFormatterDoesRelativeDateFormattingKey")
CONST_STRING_DECL(kCFDateFormatterEraSymbolsKey, "kCFDateFormatterEraSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterGregorianStartDateKey, "kCFDateFormatterGregorianStartDateKey")
CONST_STRING_DECL(kCFDateFormatterIsLenientKey, "kCFDateFormatterIsLenientKey")
CONST_STRING_DECL(kCFDateFormatterLongEraSymbolsKey, "kCFDateFormatterLongEraSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterMonthSymbolsKey, "kCFDateFormatterMonthSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterPMSymbolKey, "kCFDateFormatterPMSymbolKey")
CONST_STRING_DECL(kCFDateFormatterQuarterSymbolsKey, "kCFDateFormatterQuarterSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterShortMonthSymbolsKey, "kCFDateFormatterShortMonthSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterShortQuarterSymbolsKey, "kCFDateFormatterShortQuarterSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterShortStandaloneMonthSymbolsKey, "kCFDateFormatterShortStandaloneMonthSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterShortStandaloneQuarterSymbolsKey, "kCFDateFormatterShortStandaloneQuarterSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterShortStandaloneWeekdaySymbolsKey, "kCFDateFormatterShortStandaloneWeekdaySymbolsKey")
CONST_STRING_DECL(kCFDateFormatterShortWeekdaySymbolsKey, "kCFDateFormatterShortWeekdaySymbolsKey")
CONST_STRING_DECL(kCFDateFormatterStandaloneMonthSymbolsKey, "kCFDateFormatterStandaloneMonthSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterStandaloneQuarterSymbolsKey, "kCFDateFormatterStandaloneQuarterSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterStandaloneWeekdaySymbolsKey, "kCFDateFormatterStandaloneWeekdaySymbolsKey")
CONST_STRING_DECL(kCFDateFormatterTimeZoneKey, "kCFDateFormatterTimeZoneKey")
CONST_STRING_DECL(kCFDateFormatterTimeZone, "kCFDateFormatterTimeZoneKey")
CONST_STRING_DECL(kCFDateFormatterTwoDigitStartDateKey, "kCFDateFormatterTwoDigitStartDateKey")
CONST_STRING_DECL(kCFDateFormatterVeryShortMonthSymbolsKey, "kCFDateFormatterVeryShortMonthSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterVeryShortStandaloneMonthSymbolsKey, "kCFDateFormatterVeryShortStandaloneMonthSymbolsKey")
CONST_STRING_DECL(kCFDateFormatterVeryShortStandaloneWeekdaySymbolsKey, "kCFDateFormatterVeryShortStandaloneWeekdaySymbolsKey")
CONST_STRING_DECL(kCFDateFormatterVeryShortWeekdaySymbolsKey, "kCFDateFormatterVeryShortWeekdaySymbolsKey")
CONST_STRING_DECL(kCFDateFormatterWeekdaySymbolsKey, "kCFDateFormatterWeekdaySymbolsKey")

CONST_STRING_DECL(kCFNumberFormatterAlwaysShowDecimalSeparatorKey, "kCFNumberFormatterAlwaysShowDecimalSeparatorKey")
CONST_STRING_DECL(kCFNumberFormatterCurrencyCodeKey, "kCFNumberFormatterCurrencyCodeKey")
CONST_STRING_DECL(kCFNumberFormatterCurrencyDecimalSeparatorKey, "kCFNumberFormatterCurrencyDecimalSeparatorKey")
CONST_STRING_DECL(kCFNumberFormatterCurrencyGroupingSeparatorKey, "kCFNumberFormatterCurrencyGroupingSeparatorKey")
CONST_STRING_DECL(kCFNumberFormatterCurrencySymbolKey, "kCFNumberFormatterCurrencySymbolKey")
CONST_STRING_DECL(kCFNumberFormatterDecimalSeparatorKey, "kCFNumberFormatterDecimalSeparatorKey")
CONST_STRING_DECL(kCFNumberFormatterDefaultFormatKey, "kCFNumberFormatterDefaultFormatKey")
CONST_STRING_DECL(kCFNumberFormatterExponentSymbolKey, "kCFNumberFormatterExponentSymbolKey")
CONST_STRING_DECL(kCFNumberFormatterFormatWidthKey, "kCFNumberFormatterFormatWidthKey")
CONST_STRING_DECL(kCFNumberFormatterGroupingSeparatorKey, "kCFNumberFormatterGroupingSeparatorKey")
CONST_STRING_DECL(kCFNumberFormatterGroupingSizeKey, "kCFNumberFormatterGroupingSizeKey")
CONST_STRING_DECL(kCFNumberFormatterInfinitySymbolKey, "kCFNumberFormatterInfinitySymbolKey")
CONST_STRING_DECL(kCFNumberFormatterInternationalCurrencySymbolKey, "kCFNumberFormatterInternationalCurrencySymbolKey")
CONST_STRING_DECL(kCFNumberFormatterIsLenientKey, "kCFNumberFormatterIsLenientKey")
CONST_STRING_DECL(kCFNumberFormatterMaxFractionDigitsKey, "kCFNumberFormatterMaxFractionDigitsKey")
CONST_STRING_DECL(kCFNumberFormatterMaxIntegerDigitsKey, "kCFNumberFormatterMaxIntegerDigitsKey")
CONST_STRING_DECL(kCFNumberFormatterMaxSignificantDigitsKey, "kCFNumberFormatterMaxSignificantDigitsKey")
CONST_STRING_DECL(kCFNumberFormatterMinFractionDigitsKey, "kCFNumberFormatterMinFractionDigitsKey")
CONST_STRING_DECL(kCFNumberFormatterMinIntegerDigitsKey, "kCFNumberFormatterMinIntegerDigitsKey")
CONST_STRING_DECL(kCFNumberFormatterMinSignificantDigitsKey, "kCFNumberFormatterMinSignificantDigitsKey")
CONST_STRING_DECL(kCFNumberFormatterMinusSignKey, "kCFNumberFormatterMinusSignKey")
CONST_STRING_DECL(kCFNumberFormatterMultiplierKey, "kCFNumberFormatterMultiplierKey")
CONST_STRING_DECL(kCFNumberFormatterNaNSymbolKey, "kCFNumberFormatterNaNSymbolKey")
CONST_STRING_DECL(kCFNumberFormatterNegativePrefixKey, "kCFNumberFormatterNegativePrefixKey")
CONST_STRING_DECL(kCFNumberFormatterNegativeSuffixKey, "kCFNumberFormatterNegativeSuffixKey")
CONST_STRING_DECL(kCFNumberFormatterPaddingCharacterKey, "kCFNumberFormatterPaddingCharacterKey")
CONST_STRING_DECL(kCFNumberFormatterPaddingPositionKey, "kCFNumberFormatterPaddingPositionKey")
CONST_STRING_DECL(kCFNumberFormatterPerMillSymbolKey, "kCFNumberFormatterPerMillSymbolKey")
CONST_STRING_DECL(kCFNumberFormatterPercentSymbolKey, "kCFNumberFormatterPercentSymbolKey")
CONST_STRING_DECL(kCFNumberFormatterPlusSignKey, "kCFNumberFormatterPlusSignKey")
CONST_STRING_DECL(kCFNumberFormatterPositivePrefixKey, "kCFNumberFormatterPositivePrefixKey")
CONST_STRING_DECL(kCFNumberFormatterPositiveSuffixKey, "kCFNumberFormatterPositiveSuffixKey")
CONST_STRING_DECL(kCFNumberFormatterRoundingIncrementKey, "kCFNumberFormatterRoundingIncrementKey")
CONST_STRING_DECL(kCFNumberFormatterRoundingModeKey, "kCFNumberFormatterRoundingModeKey")
CONST_STRING_DECL(kCFNumberFormatterSecondaryGroupingSizeKey, "kCFNumberFormatterSecondaryGroupingSizeKey")
CONST_STRING_DECL(kCFNumberFormatterUseGroupingSeparatorKey, "kCFNumberFormatterUseGroupingSeparatorKey")
CONST_STRING_DECL(kCFNumberFormatterUseSignificantDigitsKey, "kCFNumberFormatterUseSignificantDigitsKey")
CONST_STRING_DECL(kCFNumberFormatterZeroSymbolKey, "kCFNumberFormatterZeroSymbolKey")

CONST_STRING_DECL(kCFCalendarIdentifierGregorian, "gregorian")
CONST_STRING_DECL(kCFCalendarIdentifierBuddhist, "buddhist")
CONST_STRING_DECL(kCFCalendarIdentifierJapanese, "japanese")
CONST_STRING_DECL(kCFCalendarIdentifierIslamic, "islamic")
CONST_STRING_DECL(kCFCalendarIdentifierIslamicCivil, "islamic-civil")
CONST_STRING_DECL(kCFCalendarIdentifierHebrew, "hebrew")
CONST_STRING_DECL(kCFCalendarIdentifierChinese, "chinese")
CONST_STRING_DECL(kCFCalendarIdentifierRepublicOfChina, "roc")
CONST_STRING_DECL(kCFCalendarIdentifierPersian, "persian")
CONST_STRING_DECL(kCFCalendarIdentifierIndian, "indian")
CONST_STRING_DECL(kCFCalendarIdentifierISO8601, "")
CONST_STRING_DECL(kCFCalendarIdentifierCoptic, "coptic")
CONST_STRING_DECL(kCFCalendarIdentifierEthiopicAmeteMihret, "ethiopic")
CONST_STRING_DECL(kCFCalendarIdentifierEthiopicAmeteAlem, "ethiopic-amete-alem")

CONST_STRING_DECL(kCFGregorianCalendar, "gregorian")
CONST_STRING_DECL(kCFBuddhistCalendar, "buddhist")
CONST_STRING_DECL(kCFChineseCalendar, "chinese")
CONST_STRING_DECL(kCFHebrewCalendar, "hebrew")
CONST_STRING_DECL(kCFIslamicCalendar, "islamic")
CONST_STRING_DECL(kCFIslamicCivilCalendar, "islamic-civil")
CONST_STRING_DECL(kCFJapaneseCalendar, "japanese")
CONST_STRING_DECL(kCFRepublicOfChinaCalendar, "roc")
CONST_STRING_DECL(kCFPersianCalendar, "persian")
CONST_STRING_DECL(kCFIndianCalendar, "indian")
CONST_STRING_DECL(kCFISO8601Calendar, "")

CONST_STRING_DECL(kAppleSystemLibraryDirectory, "C:\\")

CF_EXPORT CFStringRef _CFGetWindowsAppleSystemLibraryDirectory(void) {
    return kAppleSystemLibraryDirectory;
}
