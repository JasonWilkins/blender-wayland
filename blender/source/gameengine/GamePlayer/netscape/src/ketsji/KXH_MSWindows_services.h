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
 * MS Windows specific implentations
 */

#ifndef KXH_MSWINDOWS_SERVICES_H
#define KXH_MSWINDOWS_SERVICES_H

#include "KXH_engine_data_wraps.h"

#ifdef __cplusplus
extern "C" {
#endif

	/** Create appropriate devices for the gameengine. */
	void
	KXH_create_devices(
		ketsji_engine_data* k
		);

	/** Add banners to the canvas. */
	void
	KXH_add_banners(
		ketsji_engine_data* k
		);


#ifdef __cplusplus
}
#endif

#endif
