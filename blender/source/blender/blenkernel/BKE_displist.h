/* display list (or rather multi purpose list) stuff */
/* 
	$Id$
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

*/

#ifndef BKE_DISPLIST_H
#define BKE_DISPLIST_H


#define DL_POLY                 0
#define DL_SEGM                 1
#define DL_SURF                 2
#define DL_TRIA                 3
#define DL_INDEX3               4
#define DL_INDEX4               5
#define DL_VERTCOL              6
#define DL_VERTS                7
#define DL_NORS                 8
#define DL_MESH					9

/* EVIL: #define DL_SURFINDEX(cyclu, cyclv, sizeu, sizev) */

/* prototypes */

struct Object;
struct Curve;
struct ListBase;
struct Material;
struct Bone;
struct TFace;

typedef struct DispListMesh DispListMesh;
struct DispListMesh {
	int totvert, totface;
	struct MVert *mvert;
	struct MCol *mcol;
	struct MFaceInt *mface;
	struct TFace *tface;
};

/*
 * All the different DispList.type's use the
 * data in the displist structure in fairly
 * different ways which can be rather confusing, 
 * the best thing to do would be to make a structure
 * for each displaylist type that has the fields
 * needed w/ proper names, and then make the actual
 * DispList structure a typed union.
 *   - zr
 */

/* needs splitting! */
typedef struct DispList {
    struct DispList *next, *prev;
    short type, flag;
    int parts, nr;
    short col, rt;              /* rt wordt gebruikt door initrenderNurbs */
	float *verts, *nors;
	int *index;
	unsigned int *col1, *col2;
	struct DispListMesh *mesh;
	
	/* Begin NASTY_NLA_STUFF */
//	int *offset, *run;	/* Used to index into the bone & weight lists */
//	struct Bone *bones;
//	float *weights;
	/* End NASTY_NLA_STUFF */
} DispList;

extern void copy_displist(struct ListBase *lbn, struct ListBase *lb);
extern void free_disp_elem(DispList *dl);
extern void free_displist_by_type(struct ListBase *lb, int type);
extern DispList *find_displist_create(struct ListBase *lb, int type);
extern DispList *find_displist(struct ListBase *lb, int type);
extern void addnormalsDispList(struct Object *ob, struct ListBase *lb);
extern void count_displist(struct ListBase *lb, int *totvert, int *totface);
extern void curve_to_filledpoly(struct Curve *cu, struct ListBase *dispbase);
extern void freedisplist(struct ListBase *lb);
extern void makeDispList(struct Object *ob);
extern void set_displist_onlyzero(int val);
extern void shadeDispList(struct Object *ob);
void freefastshade(void);
void boundbox_displist(struct Object *ob);
void imagestodisplist(void);
void reshadeall_displist(void);
void test_all_displists(void);

#endif
