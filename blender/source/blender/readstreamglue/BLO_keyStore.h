/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * The contents of this file may be used under the terms of either the GNU
 * General Public License Version 2 or later (the "GPL", see
 * http://www.gnu.org/licenses/gpl.html ), or the Blender License 1.0 or
 * later (the "BL", see http://www.blender.org/BL/ ) which has to be
 * bought from the Blender Foundation to become active, in which case the
 * above mentioned GPL option does not apply.
 *
 * The Original Code is Copyright (C) 2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * make all key elements available through functions
 */

#ifndef BLO_KEYSTORE_H
#define BLO_KEYSTORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "blenkey.h"

	void
keyStoreConstructor(
	UserStruct *keyUserStruct,
	char *privHexKey,
	char *pubHexKey,
	byte *ByteChecks,
	char *HexPython);

	void
keyStoreDestructor(
	void);

	int
keyStoreGetPubKey(
	byte **PubKey);

	int
keyStoreGetPrivKey(
	byte **PrivKey);

	char *
keyStoreGetUserName(
	void);

	char *
keyStoreGetEmail(
	void);

#ifdef __cplusplus
}
#endif

#endif /* BLO_KEYSTORE_H */
