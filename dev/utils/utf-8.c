/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    https://www.eclipse.org/legal/epl-2.0/
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/


/**
 * @file
 * \brief Functions for checking that strings contain UTF-8 characters only
 *
 * See page 104 of the Unicode Standard 5.0 for the list of well formed
 * UTF-8 byte sequences.
 *
 */
#include "utf-8.h"
#include <stdlib.h>
#include <string.h>
#include "TypeDefine.h"

/**
 * Macro to determine the number of elements in a single-dimension array
 */
#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

/**
 * Structure to hold the valid ranges of UTF-8 characters, for each byte up to 4
 */
struct
{
	int len; /**< number of elements in the following array (1 to 4) */
	struct
	{
		char lower; /**< lower limit of valid range */
		char upper; /**< upper limit of valid range */
	} bytes[4];   /**< up to 4 bytes can be used per character */
}
valid_ranges[] =
{
		{1, { {00, 0x7F} } },
		{2, { {0xC2, 0xDF}, {0x80, 0xBF} } },
		{3, { {0xE0, 0xE0}, {0xA0, 0xBF}, {0x80, 0xBF} } },
		{3, { {0xE1, 0xEC}, {0x80, 0xBF}, {0x80, 0xBF} } },
		{3, { {0xED, 0xED}, {0x80, 0x9F}, {0x80, 0xBF} } },
		{3, { {0xEE, 0xEF}, {0x80, 0xBF}, {0x80, 0xBF} } },
		{4, { {0xF0, 0xF0}, {0x90, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF} } },
		{4, { {0xF1, 0xF3}, {0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF} } },
		{4, { {0xF4, 0xF4}, {0x80, 0x8F}, {0x80, 0xBF}, {0x80, 0xBF} } },
};

static const char* UTF8_char_validate(int len, const char* data);


/**
 * Validate a single UTF-8 character
 * @param len the length of the string in "data"
 * @param data the bytes to check for a valid UTF-8 char
 * @return pointer to the start of the next UTF-8 character in "data"
 */
static const char* UTF8_char_validate(int len, const char* data)
{
	int good = 0;
	int charlen = 2;
	int i, j;
	const char *rc = NULL;

	if (data == NULL)
		goto exit;	/* don't have data, can't continue */

	/* first work out how many bytes this char is encoded in */
	if ((data[0] & 128) == 0)
		charlen = 1;
	else if ((data[0] & 0xF0) == 0xF0)
		charlen = 4;
	else if ((data[0] & 0xE0) == 0xE0)
		charlen = 3;

	if (charlen > len)
		goto exit;	/* not enough characters in the string we were given */

	for (i = 0; i < ARRAY_SIZE(valid_ranges); ++i)
	{ /* just has to match one of these rows */
		if (valid_ranges[i].len == charlen)
		{
			good = 1;
			for (j = 0; j < charlen; ++j)
			{
				if (data[j] < valid_ranges[i].bytes[j].lower ||
						data[j] > valid_ranges[i].bytes[j].upper)
				{
					good = 0;  /* failed the check */
					break;
				}
			}
			if (good)
				break;
		}
	}

	if (good)
		rc = data + charlen;
	exit:
	return rc;
}


/**
 * Validate a length-delimited string has only UTF-8 characters
 * @param len the length of the string in "data"
 * @param data the bytes to check for valid UTF-8 characters
 * @return 1 (true) if the string has only UTF-8 characters, 0 (false) otherwise
 */
int UTF8_validate(int len, const char* data)
{
	const char* curdata = NULL;
	int rc = 0;
	if (len == 0 || data == NULL)
	{
		rc = 1;
		goto exit;
	}
	curdata = UTF8_char_validate(len, data);
	while (curdata && (curdata < data + len))
		curdata = UTF8_char_validate((int)(data + len - curdata), curdata);

	rc = curdata != NULL;
exit:
	return rc;
}


/**
 * Validate a null-terminated string has only UTF-8 characters
 * @param string the string to check for valid UTF-8 characters
 * @return 1 (true) if the string has only UTF-8 characters, 0 (false) otherwise
 */
int UTF8_validateString(const char* string)
{
	int rc = 0;
	if (string != NULL)
	{
		rc = UTF8_validate((int)strlen(string), string);
	}
	return rc;
}

