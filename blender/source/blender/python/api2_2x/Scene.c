/* 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Jacques Guignot, Joseph Gilbert,
 * Campbell Barton, Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/
struct View3D;

#include "Scene.h" /*This must come first */

#include "BKE_global.h"
#include "BKE_main.h"
#include "MEM_guardedalloc.h"	/* for MEM_callocN */
#include "DNA_screen_types.h"	/* SPACE_VIEW3D, SPACE_SEQ */
#include "BKE_depsgraph.h"
#include "BKE_library.h"
#include "BKE_scene.h"
#include "BLI_blenlib.h"
#include "BSE_drawview.h"	/* for play_anim */
#include "BSE_headerbuttons.h"	/* for copy_scene */
#include "BIF_drawscene.h"	/* for set_scene */
#include "BIF_space.h"		/* for copy_view3d_lock() */
#include "BIF_screen.h"		/* curarea */
#include "mydevice.h"		/* for #define REDRAW */
#include "DNA_view3d_types.h"
#include "Object.h"
#include "Camera.h"
#include "gen_utils.h"
#include "sceneRender.h"
#include "sceneRadio.h"
#include "sceneTimeLine.h"


PyObject *M_Object_Get( PyObject * self, PyObject * args ); /* from Object.c */

//----------------------------------- Python BPy_Scene defaults------------
#define EXPP_SCENE_FRAME_MAX 30000
#define EXPP_SCENE_RENDER_WINRESOLUTION_MIN 4
#define EXPP_SCENE_RENDER_WINRESOLUTION_MAX 10000
//-----------------------Python API function prototypes for the Scene module--
static PyObject *M_Scene_New( PyObject * self, PyObject * args,
			      PyObject * keywords );
static PyObject *M_Scene_Get( PyObject * self, PyObject * args );
static PyObject *M_Scene_GetCurrent( PyObject * self );
static PyObject *M_Scene_Unlink( PyObject * self, PyObject * arg );
//-----------------------Scene module doc strings-----------------------------
static char M_Scene_doc[] = "The Blender.Scene submodule";
static char M_Scene_New_doc[] =
	"(name = 'Scene') - Create a new Scene called 'name' in Blender.";
static char M_Scene_Get_doc[] =
	"(name = None) - Return the scene called 'name'. If 'name' is None, return a list with all Scenes.";
static char M_Scene_GetCurrent_doc[] =
	"() - Return the currently active Scene in Blender.";
static char M_Scene_Unlink_doc[] =
	"(scene) - Unlink (delete) scene 'Scene' from Blender. (scene) is of type Blender scene.";
//----------------------Scene module method def----------------------------
struct PyMethodDef M_Scene_methods[] = {
	{"New", ( PyCFunction ) M_Scene_New, METH_VARARGS | METH_KEYWORDS,
	 M_Scene_New_doc},
	{"Get", M_Scene_Get, METH_VARARGS, M_Scene_Get_doc},
	{"get", M_Scene_Get, METH_VARARGS, M_Scene_Get_doc},
	{"GetCurrent", ( PyCFunction ) M_Scene_GetCurrent,
	 METH_NOARGS, M_Scene_GetCurrent_doc},
	{"getCurrent", ( PyCFunction ) M_Scene_GetCurrent,
	 METH_NOARGS, M_Scene_GetCurrent_doc},
	{"Unlink", M_Scene_Unlink, METH_VARARGS, M_Scene_Unlink_doc},
	{"unlink", M_Scene_Unlink, METH_VARARGS, M_Scene_Unlink_doc},
	{NULL, NULL, 0, NULL}
};
//-----------------------BPy_Scene  method declarations--------------------
static PyObject *Scene_getName( BPy_Scene * self );
static PyObject *Scene_setName( BPy_Scene * self, PyObject * arg );
static PyObject *Scene_getLayers( BPy_Scene * self );
static PyObject *Scene_setLayers( BPy_Scene * self, PyObject * arg );
static PyObject *Scene_setLayersMask( BPy_Scene * self, PyObject * arg );
static PyObject *Scene_copy( BPy_Scene * self, PyObject * arg );
static PyObject *Scene_makeCurrent( BPy_Scene * self );
static PyObject *Scene_update( BPy_Scene * self, PyObject * args );
static PyObject *Scene_link( BPy_Scene * self, PyObject * args );
static PyObject *Scene_unlink( BPy_Scene * self, PyObject * args );
static PyObject *Scene_getChildren( BPy_Scene * self );
static PyObject *Scene_getActiveObject(BPy_Scene *self);
static PyObject *Scene_getCurrentCamera( BPy_Scene * self );
static PyObject *Scene_setCurrentCamera( BPy_Scene * self, PyObject * args );
static PyObject *Scene_getRenderingContext( BPy_Scene * self );
static PyObject *Scene_getRadiosityContext( BPy_Scene * self );
static PyObject *Scene_getScriptLinks( BPy_Scene * self, PyObject * args );
static PyObject *Scene_addScriptLink( BPy_Scene * self, PyObject * args );
static PyObject *Scene_clearScriptLinks( BPy_Scene * self, PyObject * args );
static PyObject *Scene_play( BPy_Scene * self, PyObject * args );
static PyObject *Scene_getTimeLine( BPy_Scene * self );
static PyObject *Scene_getObjects( BPy_Scene * self );


//internal
static void Scene_dealloc( BPy_Scene * self );
static int Scene_setAttr( BPy_Scene * self, char *name, PyObject * v );
static int Scene_compare( BPy_Scene * a, BPy_Scene * b );
static PyObject *Scene_getAttr( BPy_Scene * self, char *name );
static PyObject *Scene_repr( BPy_Scene * self );
//-----------------------BPy_Scene method def------------------------------
static PyMethodDef BPy_Scene_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) Scene_getName, METH_NOARGS,
	 "() - Return Scene name"},
	{"setName", ( PyCFunction ) Scene_setName, METH_VARARGS,
	 "(str) - Change Scene name"},
	{"getLayers", ( PyCFunction ) Scene_getLayers, METH_NOARGS,
	 "() - Return a list of layers int indices which are set in this scene "},
	{"setLayers", ( PyCFunction ) Scene_setLayers, METH_VARARGS,
	 "(layers) - Change layers which are set in this scene\n"
	 "(layers) - list of integers in the range [1, 20]."},
	{"copy", ( PyCFunction ) Scene_copy, METH_VARARGS,
	 "(duplicate_objects = 1) - Return a copy of this scene\n"
	 "The optional argument duplicate_objects defines how the scene\n"
	 "children are duplicated:\n\t0: Link Objects\n\t1: Link Object Data"
	 "\n\t2: Full copy\n"},
	{"makeCurrent", ( PyCFunction ) Scene_makeCurrent, METH_NOARGS,
	 "() - Make self the current scene"},
	{"update", ( PyCFunction ) Scene_update, METH_VARARGS,
	 "(full = 0) - Update scene self.\n"
	 "full = 0: sort the base list of objects."
	 "full = 1: full update -- also regroups, does ipos, keys"},
	{"link", ( PyCFunction ) Scene_link, METH_VARARGS,
	 "(obj) - Link Object obj to this scene"},
	{"unlink", ( PyCFunction ) Scene_unlink, METH_VARARGS,
	 "(obj) - Unlink Object obj from this scene"},
	{"getChildren", ( PyCFunction ) Scene_getChildren, METH_NOARGS,
	 "() - Return list of all objects linked to this scene"},
	{"getActiveObject", (PyCFunction)Scene_getActiveObject, METH_NOARGS,
	 "() - Return this scene's active object"},
	{"getCurrentCamera", ( PyCFunction ) Scene_getCurrentCamera,
	 METH_NOARGS,
	 "() - Return current active Camera"},
	{"getScriptLinks", ( PyCFunction ) Scene_getScriptLinks, METH_VARARGS,
	 "(eventname) - Get a list of this scene's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged, OnLoad, OnSave, Redraw or Render."},
	{"addScriptLink", ( PyCFunction ) Scene_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new scene scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged, OnLoad, OnSave, Redraw or Render."},
	{"clearScriptLinks", ( PyCFunction ) Scene_clearScriptLinks,
	 METH_VARARGS,
	 "() - Delete all scriptlinks from this scene.\n"
	 "([s1<,s2,s3...>]) - Delete specified scriptlinks from this scene."},
	{"setCurrentCamera", ( PyCFunction ) Scene_setCurrentCamera,
	 METH_VARARGS,
	 "() - Set the currently active Camera"},
	{"getRenderingContext", ( PyCFunction ) Scene_getRenderingContext,
	 METH_NOARGS,
	 "() - Get the rendering context for the scene and return it as a BPy_RenderData"},
	{"getRadiosityContext", ( PyCFunction ) Scene_getRadiosityContext,
	 METH_NOARGS,
	 "() - Get the radiosity context for this scene."},
	{"play", ( PyCFunction ) Scene_play, METH_VARARGS,
	 "(mode = 0, win = VIEW3D) - Play realtime animation in Blender"
	 " (not rendered).\n"
	 "(mode) - int:\n"
	 "\t0 - keep playing in biggest given 'win';\n"
	 "\t1 - keep playing in all 'win', VIEW3D and SEQ windows;\n"
	 "\t2 - play once in biggest given 'win';\n"
	 "\t3 - play once in all 'win', VIEW3D and SEQ windows.\n"
	 "(win) - int: see Blender.Window.Types. Only these are meaningful here:"
	 "VIEW3D, SEQ,	IPO, ACTION, NLA, SOUND.  But others are also accepted, "
	 "since they can be used just as an interruptible timer.  If 'win' is not"
	 "available or invalid, VIEW3D is tried, then any bigger window."
	 "Returns 0 for normal exit or 1 when canceled by user input."},
	{"getTimeLine", ( PyCFunction ) Scene_getTimeLine, METH_NOARGS,
	"() - Get time line of this Scene"},
	{NULL, NULL, 0, NULL}
};
//-----------------------BPy_Scene method def------------------------------
PyTypeObject Scene_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Scene",		/* tp_name */
	sizeof( BPy_Scene ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) Scene_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Scene_getAttr,	/* tp_getattr */
	( setattrfunc ) Scene_setAttr,	/* tp_setattr */
	( cmpfunc ) Scene_compare,	/* tp_compare */
	( reprfunc ) Scene_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_Scene_methods,	/* tp_methods */
	0,			/* tp_members */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
//-----------------------Scene module Init())-----------------------------
PyObject *Scene_Init( void )
{

	PyObject *submodule;
	PyObject *dict;
	
	if( PyType_Ready( &Scene_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &SceneObSeq_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Scene", M_Scene_methods, M_Scene_doc );

	dict = PyModule_GetDict( submodule );
	PyDict_SetItemString( dict, "Render", Render_Init(  ) );
	PyDict_SetItemString( dict, "Radio", Radio_Init(  ) );
	
	return submodule;
}

//-----------------------Scene module internal callbacks------------------
//-----------------------dealloc------------------------------------------
static void Scene_dealloc( BPy_Scene * self )
{
	PyObject_DEL( self );
}

//-----------------------getAttr----------------------------------------
static PyObject *Scene_getAttr( BPy_Scene * self, char *name )
{
	PyObject *attr = Py_None;

	if( !(self->scene) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );
	
	if( strcmp( name, "name" ) == 0 )
		attr = PyString_FromString( self->scene->id.name + 2 );
	/* accept both Layer (for compatibility with ob.Layer) and Layers */
	else if( strncmp( name, "Layer", 5 ) == 0 )
		attr = PyInt_FromLong( self->scene->lay );
	/* Layers returns a bitmask, layers returns a list of integers */
	else if( strcmp( name, "layers") == 0)
		return Scene_getLayers(self);
	else if( strcmp( name, "objects") == 0)
		return Scene_getObjects(self);

	else if( strcmp( name, "__members__" ) == 0 )
		attr = Py_BuildValue( "[sss]", "name", "Layers", "layers", "objects");

	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject" ) );

	if( attr != Py_None )
		return attr;	/* member attribute found, return it */

	/* not an attribute, search the methods table */
	return Py_FindMethod( BPy_Scene_methods, ( PyObject * ) self, name );
}

//-----------------------setAttr----------------------------------------
static int Scene_setAttr( BPy_Scene * self, char *name, PyObject * value )
{
	PyObject *valtuple;
	PyObject *error = NULL;

	if( !(self->scene) )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );
	
	
/* We're playing a trick on the Python API users here.	Even if they use
 * Scene.member = val instead of Scene.setMember(val), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Scene structure when necessary. */

/* First we put "value" in a tuple, because we want to pass it to functions
 * that only accept PyTuples. Using "N" doesn't increment value's ref count */
	valtuple = Py_BuildValue( "(O)", value );

	if( !valtuple )		/* everything OK with our PyObject? */
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "SceneSetAttr: couldn't create PyTuple" );

/* Now we just compare "name" with all possible BPy_Scene member variables */
	if( strcmp( name, "name" ) == 0 )
		error = Scene_setName( self, valtuple );
	else if (strncmp(name, "Layer", 5) == 0)
		error = Scene_setLayersMask(self, valtuple);	
	else if (strcmp(name, "layers") == 0)
		error = Scene_setLayers(self, valtuple);

	else { /* Error: no member with the given name was found */
		Py_DECREF( valtuple );
		return ( EXPP_ReturnIntError( PyExc_AttributeError, name ) );
	}

/* valtuple won't be returned to the caller, so we need to DECREF it */
	Py_DECREF( valtuple );

	if( error != Py_None )
		return -1;

/* Py_None was incref'ed by the called Scene_set* function. We probably
 * don't need to decref Py_None (!), but since Python/C API manual tells us
 * to treat it like any other PyObject regarding ref counting ... */
	Py_DECREF( Py_None );
	return 0;		/* normal exit */
}

//-----------------------compare----------------------------------------
static int Scene_compare( BPy_Scene * a, BPy_Scene * b )
{
	Scene *pa = a->scene, *pb = b->scene;
	return ( pa == pb ) ? 0 : -1;
}

//----------------------repr--------------------------------------------
static PyObject *Scene_repr( BPy_Scene * self )
{
	if( !(self->scene) )
		return PyString_FromString( "[Scene Unlinked]");
	else
		return PyString_FromFormat( "[Scene \"%s\"]",
					self->scene->id.name + 2 );
}

//-----------------------CreatePyObject---------------------------------
PyObject *Scene_CreatePyObject( Scene * scene )
{
	BPy_Scene *pyscene;

	pyscene = ( BPy_Scene * ) PyObject_NEW( BPy_Scene, &Scene_Type );

	if( !pyscene )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Scene object" );

	pyscene->scene = scene;

	return ( PyObject * ) pyscene;
}

//-----------------------CheckPyObject----------------------------------
int Scene_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Scene_Type );
}

//-----------------------FromPyObject-----------------------------------
Scene *Scene_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Scene * ) pyobj )->scene;
}

//-----------------------GetSceneByName()-------------------------------
/* Description: Returns the object with the name specified by the argument	name. 
Note that the calling function has to remove the first two characters of the object name. 
These two characters	specify the type of the object (OB, ME, WO, ...)The function 
will return NULL when no object with the given  name is found.	 */
Scene *GetSceneByName( char *name )
{
	Scene *scene_iter;

	scene_iter = G.main->scene.first;
	while( scene_iter ) {
		if( StringEqual( name, GetIdName( &( scene_iter->id ) ) ) ) {
			return ( scene_iter );
		}
		scene_iter = scene_iter->id.next;
	}

	/* There is no object with the given name */
	return ( NULL );
}


//-----------------------Scene module function defintions---------------
//-----------------------Scene.New()------------------------------------
static PyObject *M_Scene_New( PyObject * self, PyObject * args,
			      PyObject * kword )
{
	char *name = "Scene";
	char *kw[] = { "name", NULL };
	PyObject *pyscene;	/* for the Scene object wrapper in Python */
	Scene *blscene;		/* for the actual Scene we create in Blender */

	if( !PyArg_ParseTupleAndKeywords( args, kword, "|s", kw, &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected a string or an empty argument list" ) );

	blscene = add_scene( name );	/* first create the Scene in Blender */

	if( blscene ) {
		/* normally, for most objects, we set the user count to zero here.
		 * Scene is different than most objs since it is the container
		 * for all the others. Since add_scene() has already set 
		 * the user count to one, we leave it alone.
		 */

		/* now create the wrapper obj in Python */
		pyscene = Scene_CreatePyObject( blscene );
	} else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Scene obj in Blender" ) );

	if( pyscene == NULL )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create Scene PyObject" ) );

	return pyscene;
}

//-----------------------Scene.Get()------------------------------------
static PyObject *M_Scene_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Scene *scene_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	scene_iter = G.main->scene.first;

	if( name ) {		/* (name) - Search scene by name */

		PyObject *wanted_scene = NULL;

		while( ( scene_iter ) && ( wanted_scene == NULL ) ) {

			if( strcmp( name, scene_iter->id.name + 2 ) == 0 )
				wanted_scene =
					Scene_CreatePyObject( scene_iter );

			scene_iter = scene_iter->id.next;
		}

		if( wanted_scene == NULL ) {	/* Requested scene doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Scene \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return wanted_scene;
	}

	else {	/* () - return a list with wrappers for all scenes in Blender */
		int index = 0;
		PyObject *sce_pylist, *pyobj;

		sce_pylist = PyList_New( BLI_countlist( &( G.main->scene ) ) );

		if( sce_pylist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( scene_iter ) {
			pyobj = Scene_CreatePyObject( scene_iter );

			if( !pyobj )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyString" ) );

			PyList_SET_ITEM( sce_pylist, index, pyobj );

			scene_iter = scene_iter->id.next;
			index++;
		}

		return sce_pylist;
	}
}

//-----------------------Scene.GetCurrent()------------------------------
static PyObject *M_Scene_GetCurrent( PyObject * self )
{
	return Scene_CreatePyObject( ( Scene * ) G.scene );
}

//-----------------------Scene.Unlink()----------------------------------
static PyObject *M_Scene_Unlink( PyObject * self, PyObject * args )
{
	PyObject *pyobj;
	BPy_Scene *pyscn;
	Scene *scene;

	if( !PyArg_ParseTuple( args, "O!", &Scene_Type, &pyobj ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Scene PyType object" );
	
	pyscn = (BPy_Scene *)pyobj;
	scene = pyscn->scene;
	
	if (!(scene))
		return EXPP_ReturnPyObjError( PyExc_SystemError,
					      "scene has alredy been removed!" );
	
	if( scene == G.scene )
		return EXPP_ReturnPyObjError( PyExc_SystemError,
					      "current Scene cannot be removed!" );

	free_libblock( &G.main->scene, scene );
	
	pyscn->scene=NULL;
	Py_INCREF( Py_None );
	return Py_None;
}

//-----------------------BPy_Scene function defintions-------------------
//-----------------------Scene.getName()---------------------------------
static PyObject *Scene_getName( BPy_Scene * self )
{
	PyObject *attr = PyString_FromString( self->scene->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Scene.name attribute" ) );
}

//-----------------------Scene.setName()---------------------------------
static PyObject *Scene_setName( BPy_Scene * self, PyObject * args )
{
	char *name;
	char buf[21];

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	rename_id( &self->scene->id, buf );

	Py_INCREF( Py_None );
	return Py_None;
}

//-----------------------Scene.getLayers()---------------------------------
static PyObject *Scene_getLayers( BPy_Scene * self )
{
	PyObject *laylist = PyList_New( 0 ), *item;
	int layers, bit = 0, val = 0;

	if( !laylist )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
			"couldn't create pylist!" ) );

	layers = self->scene->lay;

	while( bit < 20 ) {
		val = 1 << bit;
		if( layers & val ) {
			item = Py_BuildValue( "i", bit + 1 );
			PyList_Append( laylist, item );
			Py_DECREF( item );
		}
		bit++;
	}
	return laylist;
}

//-----------------------Scene.setLayers()---------------------------------
static PyObject *Scene_setLayers( BPy_Scene * self, PyObject * args )
{
	PyObject *list = NULL, *item = NULL;
	int layers = 0, val, i, len_list;

	if( !PyArg_ParseTuple( args, "O!", &PyList_Type, &list ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected a list of integers in the range [1, 20]" ) );

	len_list = PyList_Size(list);

	if (len_list == 0)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"list can't be empty, at least one layer must be set" ) );

	for( i = 0; i < len_list; i++ ) {
		item = PyList_GetItem( list, i );
		if( !PyInt_Check( item ) )
			return EXPP_ReturnPyObjError
				( PyExc_AttributeError,
				  "list must contain only integer numbers" );

		val = ( int ) PyInt_AsLong( item );
		if( val < 1 || val > 20 )
			return EXPP_ReturnPyObjError
				( PyExc_AttributeError,
				  "layer values must be in the range [1, 20]" );

		layers |= 1 << ( val - 1 );
	}
	self->scene->lay = layers;

	if (G.vd && (self->scene == G.scene)) {
		int bit = 0;
		G.vd->lay = layers;

		while( bit < 20 ) {
			val = 1 << bit;
			if( layers & val ) {
				G.vd->layact = val;
				break;
			}
			bit++;
		}
	}

	return EXPP_incr_ret(Py_None);
}

/* only used by setAttr */
static PyObject *Scene_setLayersMask(BPy_Scene *self, PyObject *args)
{
	int laymask = 0;

	if (!PyArg_ParseTuple(args , "i", &laymask)) {
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected an integer (bitmask) as argument" );
	}

	if (laymask <= 0 || laymask > 1048575) /* binary: 1111 1111 1111 1111 1111 */
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
			"bitmask must have from 1 up to 20 bits set");

	self->scene->lay = laymask;
	/* if this is the current scene then apply the scene layers value
	 * to the view layers value: */
	if (G.vd && (self->scene == G.scene)) {
		int val, bit = 0;
		G.vd->lay = laymask;

		while( bit < 20 ) {
			val = 1 << bit;
			if( laymask & val ) {
				G.vd->layact = val;
				break;
			}
			bit++;
		}
	}

	return EXPP_incr_ret(Py_None);
}

//-----------------------Scene.copy()------------------------------------
static PyObject *Scene_copy( BPy_Scene * self, PyObject * args )
{
	short dup_objs = 1;
	Scene *scene = self->scene;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	if( !PyArg_ParseTuple( args, "|h", &dup_objs ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int in [0,2] or nothing as argument" );

	return Scene_CreatePyObject( copy_scene( scene, dup_objs ) );
}

//-----------------------Scene.makeCurrent()-----------------------------
static PyObject *Scene_makeCurrent( BPy_Scene * self )
{
	Scene *scene = self->scene;

	if( scene ) {
		set_scene( scene );
		scene_update_for_newframe(scene, scene->lay);
	}

	Py_INCREF( Py_None );
	return Py_None;
}

//-----------------------Scene.update()----------------------------------
static PyObject *Scene_update( BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	int full = 0;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	if( !PyArg_ParseTuple( args, "|i", &full ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or int (0 or 1) argument" );

/* Under certain circunstances, DAG_scene_sort *here* can crash Blender.
 * A "RuntimeError: max recursion limit" happens when a scriptlink
 * on frame change has scene.update(1).
 * Investigate better how to avoid this. */
	if( !full )
		DAG_scene_sort( scene );

	else if( full == 1 )
		set_scene_bg( scene );

	else
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "in method scene.update(full), full should be:\n"
					      "0: to only sort scene elements (old behavior); or\n"
					      "1: for a full update (regroups, does ipos, keys, etc.)" );

	Py_INCREF( Py_None );
	return Py_None;
}

//-----------------------Scene.link()------------------------------------
static PyObject *Scene_link( BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	BPy_Object *bpy_obj;
	Object *object = NULL;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &bpy_obj ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Object argument" );
	
	
		//return EXPP_ReturnPyObjError( PyExc_RuntimeError,
		//			          "Could not create data on demand for this object type!" );
	
	object = bpy_obj->object;
	
	/* Object.c's EXPP_add_obdata does not support these objects */
	if (!object->data && (object->type == OB_SURF || object->type == OB_FONT || object->type == OB_WAVE )) {
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Object has no data and new data cant be automaticaly created for Surf, Text or Wave type objects!" );
	} else {
		/* Ok, all is fine, let's try to link it */
		Base *base;

		/* We need to link the object to a 'Base', then link this base
		 * to the scene.        See DNA_scene_types.h ... */

		/* First, check if the object isn't already in the scene */
		base = object_in_scene( object, scene );
		/* if base is not NULL ... */
		if( base )	/* ... the object is already in one of the Scene Bases */
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "object already in scene!" );

		/* not linked, go get mem for a new base object */

		base = MEM_callocN( sizeof( Base ), "pynewbase" );

		if( !base )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't allocate new Base for object" );

		/* check if this object has obdata, case not, try to create it */
		
		if( !object->data && ( object->type != OB_EMPTY ) )
			EXPP_add_obdata( object ); /* returns -1 on error, defined in Object.c */

		base->object = object;	/* link object to the new base */
		base->lay = object->lay;
		base->flag = object->flag;

		object->id.us += 1;	/* incref the object user count in Blender */

		BLI_addhead( &scene->base, base );	/* finally, link new base to scene */
	}

	Py_INCREF( Py_None );
	return Py_None;
}

//-----------------------Scene.unlink()----------------------------------
static PyObject *Scene_unlink( BPy_Scene * self, PyObject * args )
{
	BPy_Object *bpy_obj = NULL;
	Object *object;
	Scene *scene = self->scene;
	Base *base;
	short retval = 0;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender scene was deleted!" );

	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &bpy_obj ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Object as argument" );

	object = bpy_obj->object;

	/* is the object really in the scene? */
	base = object_in_scene( object, scene );

	if( base ) {		/* if it is, remove it */
		/* check that there is a data block before decrementing refcount */
		if( (ID *)object->data )
			((ID *)object->data)->us--;
		else if( object->type != OB_EMPTY )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Object has no data!" );

		BLI_remlink( &scene->base, base );
		object->id.us--;
		MEM_freeN( base );
		scene->basact = 0;	/* in case the object was selected */
		retval = 1;
	}

	return Py_BuildValue( "i", PyInt_FromLong( retval ) );
}

//-----------------------Scene.getChildren()-----------------------------
static PyObject *Scene_getChildren( BPy_Scene * self )
{
	Scene *scene = self->scene;
	PyObject *pylist = PyList_New( 0 );
	PyObject *bpy_obj;
	Object *object;
	Base *base;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	base = scene->base.first;

	while( base ) {
		object = base->object;
		
		bpy_obj = Object_CreatePyObject( object );
		
		if( !bpy_obj )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						      "couldn't create new object wrapper" );

		PyList_Append( pylist, bpy_obj );
		Py_XDECREF( bpy_obj );	/* PyList_Append incref'ed it */
		
		base = base->next;
	}

	return pylist;
}

//-----------------------Scene.getActiveObject()------------------------
static PyObject *Scene_getActiveObject(BPy_Scene *self)
{
	Scene *scene = self->scene;
	PyObject *pyob;
	Object *ob;

	if (!scene)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"Blender Scene was deleted!");

	ob = ((scene->basact) ? (scene->basact->object) : 0);

	if (ob) {
		pyob = Object_CreatePyObject( ob );

		if (!pyob)
			return EXPP_ReturnPyObjError(PyExc_MemoryError,
					"couldn't create new object wrapper!");

		return pyob;
	}

	return EXPP_incr_ret(Py_None); /* no active object */
}

//-----------------------Scene.getCurrentCamera()------------------------
static PyObject *Scene_getCurrentCamera( BPy_Scene * self )
{
	Object *cam_obj;
	PyObject *pyob;
	Scene *scene = self->scene;
	
	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	cam_obj = scene->camera;

	if( cam_obj ) {		/* if found, return a wrapper for it */
		pyob = Object_CreatePyObject( cam_obj );
		if (!pyob)
			return EXPP_ReturnPyObjError(PyExc_MemoryError,
					"couldn't create new object wrapper!");
		return pyob;
	}

	Py_INCREF( Py_None );	/* none found */
	return Py_None;
}

//-----------------------Scene.setCurrentCamera()------------------------
static PyObject *Scene_setCurrentCamera( BPy_Scene * self, PyObject * args )
{
	Object *object;
	BPy_Object *cam_obj;
	Scene *scene = self->scene;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &cam_obj ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Camera Object as argument" );

	object = cam_obj->object;
	if( object->type != OB_CAMERA )
		return EXPP_ReturnPyObjError( PyExc_ValueError,
					      "expected Camera Object as argument" );

	scene->camera = object;	/* set the current Camera */

	/* if this is the current scene, update its window now */
	if( !G.background && scene == G.scene ) /* Traced a crash to redrawing while in background mode -Campbell */
		copy_view3d_lock( REDRAW );

/* XXX copy_view3d_lock(REDRAW) prints "bad call to addqueue: 0 (18, 1)".
 * The same happens in bpython. */

	Py_INCREF( Py_None );
	return Py_None;
}

//-----------------------Scene.getRenderingContext()---------------------
static PyObject *Scene_getRenderingContext( BPy_Scene * self )
{
	if( !self->scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	return RenderData_CreatePyObject( self->scene );
}

static PyObject *Scene_getRadiosityContext( BPy_Scene * self )
{
	if( !self->scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	return Radio_CreatePyObject( self->scene );
}

/* scene.addScriptLink */
static PyObject *Scene_addScriptLink( BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	slink = &( scene )->scriptlink;

	return EXPP_addScriptLink( slink, args, 1 );
}

/* scene.clearScriptLinks */
static PyObject *Scene_clearScriptLinks( BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	slink = &( scene )->scriptlink;

	return EXPP_clearScriptLinks( slink, args );
}

/* scene.getScriptLinks */
static PyObject *Scene_getScriptLinks( BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	slink = &( scene )->scriptlink;

	ret = EXPP_getScriptLinks( slink, args, 1 );

	if( ret )
		return ret;
	else
		return NULL;
}

static PyObject *Scene_play( BPy_Scene * self, PyObject * args )
{
	Scene *scene = self->scene;
	int mode = 0, win = SPACE_VIEW3D;
	PyObject *ret = NULL;
	ScrArea *sa = NULL, *oldsa = curarea;

	if( !scene )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" );

	if( !PyArg_ParseTuple( args, "|ii", &mode, &win ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing, or or two ints as arguments." );

	if( mode < 0 || mode > 3 )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "mode should be in range [0, 3]." );

	switch ( win ) {
	case SPACE_VIEW3D:
	case SPACE_SEQ:
	case SPACE_IPO:
	case SPACE_ACTION:
	case SPACE_NLA:
	case SPACE_SOUND:
	case SPACE_BUTS:	/* from here they don't 'play', but ... */
	case SPACE_TEXT:	/* ... might be used as a timer. */
	case SPACE_SCRIPT:
	case SPACE_OOPS:
	case SPACE_IMAGE:
	case SPACE_IMASEL:
	case SPACE_INFO:
	case SPACE_FILE:
		break;
	default:
		win = SPACE_VIEW3D;
	}

	/* we have to move to a proper win */
	sa = find_biggest_area_of_type( win );
	if( !sa && win != SPACE_VIEW3D )
		sa = find_biggest_area_of_type( SPACE_VIEW3D );

	if( !sa )
		sa = find_biggest_area(  );

	if( sa )
		areawinset( sa->win );

	/* play_anim returns 0 for normal exit or 1 if user canceled it */
	ret = Py_BuildValue( "i", play_anim( mode ) );

	if( sa )
		areawinset( oldsa->win );

	return ret;
}

static PyObject *Scene_getTimeLine( BPy_Scene *self ) 
{
	BPy_TimeLine *tm; 

	if( !(self->scene) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender scene was deleted!" );

	
	tm= (BPy_TimeLine *) PyObject_NEW (BPy_TimeLine, &TimeLine_Type);
	if (!tm)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
					      "couldn't create BPy_TimeLine object");
	tm->marker_list= &(self->scene->markers);
	tm->sfra= (int) self->scene->r.sfra;
	tm->efra= (int) self->scene->r.efra;

	return (PyObject *)tm;
}


static PyObject *Scene_getObjects( BPy_Scene *self ) 
{
	BPy_SceneObSeq *seq = PyObject_NEW( BPy_SceneObSeq, &SceneObSeq_Type);
	seq->bpyscene = self; Py_INCREF(self);
	return (PyObject *)seq;
}

/************************************************************************
 *
 * Object Sequence 
 *
 ************************************************************************/
/*
 * create a thin wrapper for the scenes objects
 */

static PyObject *SceneObSeq_CreatePyObject( Scene *scene, int i )
{
	int index=0;
	PyObject *bpy_obj;
	Base *base;
	
	for (base= scene->base.first; base&& i!=index; base= base->next, index++) {}
	
	if (!(base))
		return EXPP_ReturnPyObjError( PyExc_IndexError,
					      "array index out of range" );
	
	bpy_obj = Object_CreatePyObject( base->object );

	if( !bpy_obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"PyObject_New() failed" );

	return (PyObject *)bpy_obj;
}

static int SceneObSeq_len( BPy_SceneObSeq * self )
{
	return BLI_countlist( &( self->bpyscene->scene->base ) );
}

/*
 * retrive a single MGroupOb from somewhere in the GroupObex list
 */

static PyObject *SceneObSeq_item( BPy_SceneObSeq * self, int i )
{
	return SceneObSeq_CreatePyObject( self->bpyscene->scene, i );
}

static PySequenceMethods SceneObSeq_as_sequence = {
	( inquiry ) SceneObSeq_len,	/* sq_length */
	( binaryfunc ) 0,	/* sq_concat */
	( intargfunc ) 0,	/* sq_repeat */
	( intargfunc ) SceneObSeq_item,	/* sq_item */
	( intintargfunc ) 0,	/* sq_slice */
	( intobjargproc ) 0,	/* sq_ass_item */
	( intintobjargproc ) 0,	/* sq_ass_slice */
	0,0,0,
};


/************************************************************************
 *
 * Python SceneObSeq_Type iterator (iterates over GroupObjects)
 *
 ************************************************************************/

/*
 * Initialize the interator index
 */

static PyObject *SceneObSeq_getIter( BPy_SceneObSeq * self )
{
	self->iter = self->bpyscene->scene->base.first;
	return EXPP_incr_ret ( (PyObject *) self );
}

/*
 * Return next SceneOb.
 */

static PyObject *SceneObSeq_nextIter( BPy_SceneObSeq * self )
{
	PyObject *object;
	if( !(self->iter) ||  !(self->bpyscene->scene) )
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	
	object= Object_CreatePyObject( self->iter->object ); 
	self->iter= self->iter->next;
	return object;
}


static PyObject *SceneObSeq_add( BPy_SceneObSeq * self, PyObject *pyobj )
{
	/* this shold eventually replace Scene_link */
	return Scene_link(self->bpyscene, pyobj);
}



static PyObject *SceneObSeq_remove( BPy_SceneObSeq * self, PyObject *args )
{
	PyObject *pyobj;
	Object *blen_ob;
	Base *base= NULL;
	
	if( !(self->bpyscene->scene) )
		return (EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Blender Scene was deleted!" ));
	
	if( !PyArg_ParseTuple( args, "O!", &Object_Type, &pyobj ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a python object as an argument" ) );
	
	blen_ob = ( ( BPy_Object * ) pyobj )->object;

	/* is the object really in the scene? */
	base = object_in_scene( blen_ob, self->bpyscene->scene );

	if( base ) {		/* if it is, remove it */
		/* check that there is a data block before decrementing refcount */
		if( (ID *)blen_ob->data )
			((ID *)blen_ob->data)->us--;
		else if( blen_ob->type != OB_EMPTY )
			return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "Object has no data!" );

		BLI_remlink( &(self->bpyscene->scene)->base, base );
		blen_ob->id.us--;
		MEM_freeN( base );
		self->bpyscene->scene->basact = 0;	/* in case the object was selected */
	}
	return EXPP_incr_ret( Py_None );
}


static struct PyMethodDef BPy_SceneObSeq_methods[] = {
	{"add", (PyCFunction)SceneObSeq_add, METH_VARARGS,
		"add object to group"},
	{"remove", (PyCFunction)SceneObSeq_remove, METH_VARARGS,
		"remove object from group"},
	{NULL, NULL, 0, NULL}
};

/************************************************************************
 *
 * Python SceneObSeq_Type standard operations
 *
 ************************************************************************/

static void SceneObSeq_dealloc( BPy_SceneObSeq * self )
{
	Py_DECREF(self->bpyscene);
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Python SceneObSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject SceneObSeq_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender SceneObSeq",           /* char *tp_name; */
	sizeof( BPy_SceneObSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) SceneObSeq_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&SceneObSeq_as_sequence,	    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) SceneObSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) SceneObSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_SceneObSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};