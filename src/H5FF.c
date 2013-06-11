/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Programmer:  Quincey Koziol <koziol@hdfgroup.org>
 *              March, 2013
 *
 * Purpose:	Wrappers around existing HDF5 to support Exascale FastForward
 *              functionality.
 *              
 */


/****************/
/* Module Setup */
/****************/

#define H5A_PACKAGE		/*suppress error about including H5Apkg	  */
#define H5D_PACKAGE		/*suppress error about including H5Dpkg	  */
#define H5F_PACKAGE		/*suppress error about including H5Fpkg	  */
#define H5G_PACKAGE		/*suppress error about including H5Gpkg	  */
#define H5T_PACKAGE		/*suppress error about including H5Tpkg	  */

/* Interface initialization */
#define H5_INTERFACE_INIT_FUNC	H5FF__init_interface

/***********/
/* Headers */
/***********/
#include "H5private.h"		/* Generic Functions			*/
#include "H5Apkg.h"             /* Attribute access			*/
#include "H5Dpkg.h"             /* Dataset access			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5Fpkg.h"             /* File access				*/
#include "H5FFprivate.h"        /* FastForward wrappers                 */
#include "H5Gpkg.h"             /* Group access				*/
#include "H5Iprivate.h"		/* IDs			  		*/
#include "H5MMprivate.h"	/* Memory management			*/
#include "H5Pprivate.h"		/* Property lists			*/
#include "H5Tpkg.h"             /* Datatype access			*/
#include "H5VLprivate.h"	/* VOL plugins				*/
#include "H5VLiod.h"		/* IOD plugin - tmp      		*/
#include "H5VLiod_client.h"	/* Client IOD - tmp			*/

#ifdef H5_HAVE_EFF
/****************/
/* Local Macros */
/****************/


/******************/
/* Local Typedefs */
/******************/


/********************/
/* Local Prototypes */
/********************/


/*********************/
/* Package Variables */
/*********************/


/*****************************/
/* Library Private Variables */
/*****************************/


/*******************/
/* Local Variables */
/*******************/

static herr_t
H5FF__init_interface(void)
{
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_STATIC

    if(H5F_init() < 0)
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to init file interface")

    if(H5G__init() < 0)
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to init group interface")

    if(H5D_init() < 0)
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to init dataset interface")

    if(H5A_init() < 0)
        HDONE_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to init attribute interface")

    FUNC_LEAVE_NOAPI(ret_value)
} /* H5FF__init_interface() */


/*-------------------------------------------------------------------------
 * Function:	H5Fcreate_ff
 *
 * Purpose:	Asynchronous wrapper around H5Fcreate().
 *
 * Return:	Success:	The placeholder ID for a new file.  When
 *                              the asynchronous operation completes, this
 *                              ID will transparently be modified to be a
 *                              "normal" ID.
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Fcreate_ff(const char *filename, unsigned flags, hid_t fcpl_id, hid_t fapl_id, hid_t eq_id)
{
    void    *file = NULL;            /* file token from VOL plugin */
    H5VL_t  *vol_plugin;             /* VOL plugin information */
    hid_t    ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check/fix arguments */
    if(!filename || !*filename)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file name")
    /* In this routine, we only accept the following flags:
     *          H5F_ACC_EXCL, H5F_ACC_TRUNC and H5F_ACC_DEBUG
     */
    if(flags & ~(H5F_ACC_EXCL | H5F_ACC_TRUNC | H5F_ACC_DEBUG))
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid flags")
    /* The H5F_ACC_EXCL and H5F_ACC_TRUNC flags are mutually exclusive */
    if((flags & H5F_ACC_EXCL) && (flags & H5F_ACC_TRUNC))
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "mutually exclusive flags for file creation")

    /* Check file creation property list */
    if(H5P_DEFAULT == fcpl_id)
        fcpl_id = H5P_FILE_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(fcpl_id, H5P_FILE_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not file create property list")

    /* Check the file access property list */
    if(H5P_DEFAULT == fapl_id)
        fapl_id = H5P_FILE_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(fapl_id, H5P_FILE_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not file access property list")

    /* create a new file or truncate an existing file through the VOL */
    if(NULL == (file = H5VL_file_create(&vol_plugin, filename, flags, fcpl_id, fapl_id, 
                                        H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, FAIL, "unable to create file")

    /* Get an atom for the file with the VOL information as the auxilary struct*/
    if((ret_value = H5I_register2(H5I_FILE, file, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize file handle")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Fcreate_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Fopen_ff
 *
 * Purpose:	Asynchronous wrapper around H5Fopen().
 *
 * Return:	Success:	The placeholder ID for a new file.  When
 *                              the asynchronous operation completes, this
 *                              ID will transparently be modified to be a
 *                              "normal" ID.
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Fopen_ff(const char *filename, unsigned flags, hid_t fapl_id, hid_t eq_id)
{
    void    *file = NULL;            /* file token from VOL plugin */
    H5VL_t  *vol_plugin;             /* VOL plugin information */
    hid_t    ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check/fix arguments. */
    if(!filename || !*filename)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file name")
    /* Reject undefined flags (~H5F_ACC_PUBLIC_FLAGS) and the H5F_ACC_TRUNC & H5F_ACC_EXCL flags */
    if((flags & ~H5F_ACC_PUBLIC_FLAGS) ||
            (flags & H5F_ACC_TRUNC) || (flags & H5F_ACC_EXCL))
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file open flags")
    if(H5P_DEFAULT == fapl_id)
        fapl_id = H5P_FILE_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(fapl_id, H5P_FILE_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not file access property list")

    /* Open the file through the VOL layer */
    if(NULL == (file = H5VL_file_open(&vol_plugin, filename, flags, fapl_id, 
                                      H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, FAIL, "unable to create file")

    /* Get an atom for the file with the VOL information as the auxilary struct*/
    if((ret_value = H5I_register2(H5I_FILE, file, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize file handle")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Fopen_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Fflush_ff
 *
 * Purpose:	FF version of H5Fflush()
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Fflush_ff(hid_t object_id, H5F_scope_t scope, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *obj;
    H5I_type_t obj_type;
    H5VL_loc_params_t loc_params;
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    obj_type = H5I_get_type(object_id);
    if(H5I_FILE != obj_type && H5I_GROUP != obj_type && H5I_DATATYPE != obj_type &&
       H5I_DATASET != obj_type && H5I_ATTR != obj_type) {
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file or file object")
    }

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(object_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(object_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = obj_type;

    if((ret_value = H5VL_file_flush(obj, loc_params, vol_plugin, scope, 
                                    H5AC_dxpl_id, eq_id)) < 0)
        HGOTO_ERROR(H5E_FILE, H5E_CANTFLUSH, FAIL, "unable to flush file")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Fflush_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Fclose_ff
 *
 * Purpose:	This function closes the file specified by FILE_ID by
 *		flushing all data to storage, and terminating access to the
 *		file through FILE_ID.  If objects (e.g., datasets, groups,
 *		etc.) are open in the file then the underlying storage is not
 *		closed until those objects are closed; however, all data for
 *		the file and the open objects is flushed.
 *
 * Return:	Success:	Non-negative
 *		Failure:	Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Fclose_ff(hid_t file_id, hid_t eq_id)
{
    H5VL_t  *vol_plugin = NULL;
    herr_t   ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)

    /* Check/fix arguments. */
    if(H5I_FILE != H5I_get_type(file_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a file ID")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(file_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information");
    /* set the event queue and dxpl IDs to be passed on to the VOL layer */
    vol_plugin->close_eq_id = eq_id;
    vol_plugin->close_dxpl_id = H5AC_dxpl_id;

    /* Decrement reference count on atom.  When it reaches zero the file will be closed. */
    if(H5I_dec_app_ref(file_id) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_CANTCLOSEFILE, FAIL, "decrementing file ID failed")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Fclose_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Gcreate_ff
 *
 * Purpose:	Asynchronous wrapper around H5Gcreate().
 *
 * Return:	Success:	The placeholder ID for a group.  When
 *                              the asynchronous operation completes, this
 *                              ID will transparently be modified to be a
 *                              "normal" ID.
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Gcreate_ff(hid_t loc_id, const char *name, hid_t lcpl_id, hid_t gcpl_id, hid_t gapl_id,
             uint64_t trans, hid_t eq_id)
{
    void    *grp = NULL;       /* dset token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    H5P_genplist_t  *plist;            /* Property list pointer */
    hid_t       ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(!name || !*name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    /* Get correct property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link creation property list")

    /* Check group creation property list */
    if(H5P_DEFAULT == gcpl_id)
        gcpl_id = H5P_GROUP_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(gcpl_id, H5P_GROUP_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not group create property list")

    /* Check the group access property list */
    if(H5P_DEFAULT == gapl_id)
        gapl_id = H5P_GROUP_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(gapl_id, H5P_GROUP_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not group access property list")

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(gcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID")

    if(H5P_set(plist, H5VL_GRP_LCPL_ID, &lcpl_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for lcpl id")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the location object */
    if(NULL == (obj = (void *)H5I_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the group through the VOL */
    if(NULL == (grp = H5VL_group_create(obj, loc_params, vol_plugin, name, gcpl_id, gapl_id, 
                                        H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create group")

    /* Get an atom for the group */
    if((ret_value = H5I_register2(H5I_GROUP, grp, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize group handle")

done:
    if (ret_value < 0 && grp)
        if(H5VL_group_close (grp, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_SYM, H5E_CLOSEERROR, FAIL, "unable to release group")

    FUNC_LEAVE_API(ret_value)
} /* end H5Gcreate_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Gopen_ff
 *
 * Purpose:	Asynchronous wrapper around H5Gopen().
 *
 * Return:	Success:	The placeholder ID for a group.  When
 *                              the asynchronous operation completes, this
 *                              ID will transparently be modified to be a
 *                              "normal" ID.
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Gopen_ff(hid_t loc_id, const char *name, hid_t gapl_id,
           uint64_t trans, hid_t eq_id)
{
    void    *grp = NULL;       /* dset token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    hid_t       ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
    if(!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    /* Check the group access property list */
    if(H5P_DEFAULT == gapl_id)
        gapl_id = H5P_GROUP_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(gapl_id, H5P_GROUP_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not group access property list")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5I_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the group through the VOL */
    if(NULL == (grp = H5VL_group_open(obj, loc_params, vol_plugin, name, gapl_id, 
                                      H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create group")

    /* Get an atom for the group */
    if((ret_value = H5I_register2(H5I_GROUP, grp, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Gopen_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Gclose_ff
 *
 * Purpose:	Closes the specified group.  The group ID will no longer be
 *		valid for accessing the group.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Gclose_ff(hid_t group_id, hid_t eq_id)
{
    H5VL_t  *vol_plugin = NULL;
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
    if(NULL == H5I_object_verify(group_id,H5I_GROUP))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a group")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(group_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information");
    /* set the event queue and dxpl IDs to be passed on to the VOL layer */
    vol_plugin->close_eq_id = eq_id;
    vol_plugin->close_dxpl_id = H5AC_dxpl_id;

    /*
     * Decrement the counter on the group atom.	 It will be freed if the count
     * reaches zero.
     */
    if(H5I_dec_app_ref(group_id) < 0)
    	HGOTO_ERROR(H5E_SYM, H5E_CANTRELEASE, FAIL, "unable to close group")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Gclose_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Dcreate_ff
 *
 * Purpose:	Asynchronous wrapper around H5Dcreate().
 *
 * Return:	Success:	The placeholder ID for a new dataset.  When
 *                              the asynchronous operation completes, this
 *                              ID will transparently be modified to be a
 *                              "normal" ID.
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Dcreate_ff(hid_t loc_id, const char *name, hid_t type_id, hid_t space_id, 
             hid_t lcpl_id, hid_t dcpl_id, hid_t dapl_id, uint64_t trans, hid_t eq_id)
{
    void    *dset = NULL;       /* dset token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    H5P_genplist_t  *plist;     /* Property list pointer */
    hid_t       ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    if(!name || !*name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    /* Get correct property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link creation property list")

    /* Get correct property list */
    if(H5P_DEFAULT == dcpl_id)
        dcpl_id = H5P_DATASET_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(dcpl_id, H5P_DATASET_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not dataset create property list ID")

    /* Get correct property list */
    if(H5P_DEFAULT == dapl_id)
        dapl_id = H5P_DATASET_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(dapl_id, H5P_DATASET_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not dataset access property list")

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(dcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID")

    /* set creation properties */
    if(H5P_set(plist, H5VL_DSET_TYPE_ID, &type_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for datatype id")
    if(H5P_set(plist, H5VL_DSET_SPACE_ID, &space_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for space id")
    if(H5P_set(plist, H5VL_DSET_LCPL_ID, &lcpl_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for lcpl id")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the dataset through the VOL */
    if(NULL == (dset = H5VL_dataset_create(obj, loc_params, vol_plugin, name, dcpl_id, dapl_id, 
                                           H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "unable to create dataset")

    /* Get an atom for the dataset */
    if((ret_value = H5I_register2(H5I_DATASET, dset, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    if (ret_value < 0 && dset)
        if(H5VL_dataset_close(dset, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release dataset")

    FUNC_LEAVE_API(ret_value)
} /* end H5Dcreate_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Dopen_ff
 *
 * Purpose:	Asynchronous wrapper around H5Dopen().
 *
 * Return:	Success:	The placeholder ID for a dataset.  When
 *                              the asynchronous operation completes, this
 *                              ID will transparently be modified to be a
 *                              "normal" ID.
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Dopen_ff(hid_t loc_id, const char *name, hid_t dapl_id, uint64_t trans, hid_t eq_id)
{
    void    *dset = NULL;       /* dset token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    hid_t       ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
    if(!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    /* Get correct property list */
    if(H5P_DEFAULT == dapl_id)
        dapl_id = H5P_DATASET_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(dapl_id, H5P_DATASET_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not dataset access property list")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the dataset through the VOL */
    if(NULL == (dset = H5VL_dataset_open(obj, loc_params, vol_plugin, name, dapl_id, 
                                         H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "unable to create dataset")

    /* Get an atom for the dataset */
    if((ret_value = H5I_register2(H5I_DATASET, dset, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    if (ret_value < 0 && dset)
        if(H5VL_dataset_close (dset, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release dataset")

    FUNC_LEAVE_API(ret_value)
} /* end H5Dopen_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Dwrite_ff
 *
 * Purpose:	Asynchronous wrapper around H5Dwrite().
 *
 * Return:	Success:	SUCCEED
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Dwrite_ff(hid_t dset_id, hid_t mem_type_id, hid_t mem_space_id,
            hid_t file_space_id, hid_t dxpl_id, const void *buf,
            uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *dset;
    herr_t      ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(!dset_id)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a dataset")

    /* Get the default dataset transfer property list if the user didn't provide one */
    if(H5P_DEFAULT == dxpl_id)
        dxpl_id= H5P_DATASET_XFER_DEFAULT;
    else
        if(TRUE != H5P_isa_class(dxpl_id, H5P_DATASET_XFER))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not xfer parms")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(dset_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (dset = (void *)H5I_object(dset_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid dataset identifier")

    /* Write the data through the VOL */
    if((ret_value = H5VL_dataset_write(dset, vol_plugin, mem_type_id, mem_space_id, 
                                       file_space_id, dxpl_id, buf, eq_id)) < 0)
	HGOTO_ERROR(H5E_DATASET, H5E_WRITEERROR, FAIL, "can't write data")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Dwrite_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Dread_ff
 *
 * Purpose:	Asynchronous wrapper around H5Dread().
 *
 * Return:	Success:	SUCCEED
 *		Failure:	FAIL
 *
 * Programmer:	Quincey Koziol
 *		Wednesday, March 20, 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Dread_ff(hid_t dset_id, hid_t mem_type_id, hid_t mem_space_id,
           hid_t file_space_id, hid_t dxpl_id, void *buf/*out*/,
           uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *dset;
    herr_t      ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    if(mem_space_id < 0 || file_space_id < 0)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a data space")

    /* Get the default dataset transfer property list if the user didn't provide one */
    if (H5P_DEFAULT == dxpl_id)
        dxpl_id= H5P_DATASET_XFER_DEFAULT;
    else
        if(TRUE != H5P_isa_class(dxpl_id, H5P_DATASET_XFER))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not xfer parms")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(dset_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (dset = (void *)H5I_object(dset_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid dataset identifier")

    /* Read the data through the VOL */
    if((ret_value = H5VL_dataset_read(dset, vol_plugin, mem_type_id, mem_space_id, 
                                      file_space_id, dxpl_id, buf, eq_id)) < 0)
	HGOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "can't read data")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Dread_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Dset_extent_ff
 *
 * Purpose:	Modifies the dimensions of a dataset.
 *		Can change to a smaller dimension.
 *
 * Return:	Non-negative on success, negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Dset_extent_ff(hid_t dset_id, const hsize_t size[], hid_t eq_id)
{
    H5VL_t *vol_plugin;
    void   *dset;
    herr_t ret_value = SUCCEED; /* Return value */

    FUNC_ENTER_API(FAIL)

    if(!size)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no size specified")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(dset_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (dset = (void *)H5I_object(dset_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid dataset identifier")

    /* set the extent through the VOL */
    if((ret_value = H5VL_dataset_set_extent(dset, vol_plugin, size, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_DATASET, H5E_CANTINIT, FAIL, "unable to set extent of dataset")

done:
        FUNC_LEAVE_API(ret_value)
} /* end H5Dset_extent_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Dclose_ff
 *
 * Purpose:	Closes access to a dataset (DATASET_ID) and releases
 *		resources used by it. It is illegal to subsequently use that
 *		same dataset ID in calls to other dataset functions.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Dclose_ff(hid_t dset_id, hid_t eq_id)
{
    H5VL_t      *vol_plugin = NULL;
    herr_t       ret_value = SUCCEED;   /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check/fix arguments. */
    if(H5I_DATASET != H5I_get_type(dset_id))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a dataset ID")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(dset_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information");
    /* set the event queue and dxpl IDs to be passed on to the VOL layer */
    vol_plugin->close_eq_id = eq_id;
    vol_plugin->close_dxpl_id = H5AC_dxpl_id;

    /*
     * Decrement the counter on the dataset.  It will be freed if the count
     * reaches zero.  
     *
     * Pass in TRUE for the 3rd parameter to tell the function to remove
     * dataset's ID even though the freeing function might fail.  Please
     * see the comments in H5I_dec_ref for details. (SLU - 2010/9/7)
     */
    if(H5I_dec_app_ref_always_close(dset_id) < 0)
	HGOTO_ERROR(H5E_DATASET, H5E_CANTDEC, FAIL, "can't decrement count on dataset ID")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Dclose() */


/*-------------------------------------------------------------------------
 * Function:	H5Tcommit_ff
 *
 * Purpose:	Save a transient datatype to a file and turn the type handle
 *		into a "named", immutable type.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Tcommit_ff(hid_t loc_id, const char *name, hid_t type_id, hid_t lcpl_id,
             hid_t tcpl_id, hid_t tapl_id, uint64_t trans, hid_t eq_id)
{
    void    *dt = NULL;
    H5T_t   *type = NULL;
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if (H5Tcommitted(type_id))
	HGOTO_ERROR(H5E_ARGS, H5E_CANTSET, FAIL, "datatype is already committed")
    if(!name || !*name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    /* Get correct property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link creation property list")

    /* Get correct property list */
    if(H5P_DEFAULT == tcpl_id)
        tcpl_id = H5P_DATATYPE_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(tcpl_id, H5P_DATATYPE_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not datatype creation property list")

    /* Get correct property list */
    if(H5P_DEFAULT == tapl_id)
        tapl_id = H5P_DATATYPE_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(tapl_id, H5P_DATATYPE_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not datatype access property list")

    if(NULL == (type = (H5T_t *)H5I_object_verify(type_id, H5I_DATATYPE)))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the object from the loc_id */
    if(NULL == (obj = (void *)H5I_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid object identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* commit the datatype through the VOL */
    if (NULL == (dt = H5VL_datatype_commit(obj, loc_params, vol_plugin, name, type_id, lcpl_id, 
                                           tcpl_id, tapl_id, H5AC_dxpl_id, eq_id)))
        HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to commit datatype")

    /* attach the vol object created using the commit call to the 
       library datatype structure */
    /* set the committed type object to the VOL pluging pointer in the H5T_t struct */
    type->vol_obj = dt;

    /* attach VOL information to the ID */
    if (H5I_register_aux(type_id, vol_plugin) < 0)
        HGOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't attach vol info to ID")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Tcommit_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Topen_ff
 *
 * Purpose:	Opens a named datatype using a Datatype Access Property
 *              List.
 *
 * Return:	Success:	Object ID of the named datatype.
 *		Failure:	Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Topen_ff(hid_t loc_id, const char *name, hid_t tapl_id, uint64_t trans, hid_t eq_id)
{
    void    *vol_dt = NULL;       /* datatype token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5T_t   *dt = NULL;
    H5VL_loc_params_t loc_params;
    hid_t     ret_value = FAIL;      /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
     if(!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    /* Get correct property list */
    if(H5P_DEFAULT == tapl_id)
        tapl_id = H5P_DATATYPE_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(tapl_id, H5P_DATATYPE_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not datatype access property list")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5I_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the datatype through the VOL */
    if(NULL == (vol_dt = H5VL_datatype_open(obj, loc_params, vol_plugin, name, tapl_id, 
                                        H5AC_dxpl_id, H5_EVENT_QUEUE_NULL)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to open datatype");

    /* Get an atom for the datatype */
    if ((ret_value = H5VL_create_datatype(vol_dt, vol_plugin, TRUE)) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize datatype handle");

    /* Get an atom for the datatype with the VOL information as the auxilary struct*/
    //if((ret_value = H5I_register2(H5I_DATATYPE, dt, vol_plugin, app_ref)) < 0)
    //HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize file handle")

done:
    if (ret_value < 0 && dt)
        if(H5VL_datatype_close (dt, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_SYM, H5E_CLOSEERROR, FAIL, "unable to release dataset")
    FUNC_LEAVE_API(ret_value)
} /* end H5Topen_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Tclose_ff
 *
 * Purpose:	Frees a datatype and all associated memory.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              April 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Tclose_ff(hid_t type_id, hid_t eq_id)
{
    H5T_t   *dt;                    /* Pointer to datatype to close */
    H5VL_t  *vol_plugin = NULL;
    herr_t   ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
    if(NULL == (dt = (H5T_t *)H5I_object_verify(type_id, H5I_DATATYPE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a datatype")
    if(H5T_STATE_IMMUTABLE == dt->shared->state)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "immutable datatype")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(type_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information");
    /* set the event queue and dxpl IDs to be passed on to the VOL layer */
    vol_plugin->close_eq_id = eq_id;
    vol_plugin->close_dxpl_id = H5AC_dxpl_id;

    /* When the reference count reaches zero the resources are freed */
    if(H5I_dec_app_ref(type_id) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "problem freeing id")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Tclose_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Acreate_ff
 PURPOSE
    Creates an attribute on an object
 RETURNS
    Non-negative on success/Negative on failure
--------------------------------------------------------------------------*/
hid_t
H5Acreate_ff(hid_t loc_id, const char *attr_name, hid_t type_id, hid_t space_id,
             hid_t acpl_id, hid_t aapl_id, uint64_t trans, hid_t eq_id)
{
    void    *attr = NULL;       /* attr token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5P_genplist_t      *plist;            /* Property list pointer */
    H5VL_loc_params_t   loc_params;
    hid_t		ret_value;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!attr_name || !*attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no attribute name")

    /* Get correct property list */
    if(H5P_DEFAULT == acpl_id)
        acpl_id = H5P_ATTRIBUTE_CREATE_DEFAULT;

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(acpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID")

    /* set creation properties */
    if(H5P_set(plist, H5VL_ATTR_TYPE_ID, &type_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for datatype id")
    if(H5P_set(plist, H5VL_ATTR_SPACE_ID, &space_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for space id")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the attribute through the VOL */
    if(NULL == (attr = H5VL_attr_create(obj, loc_params, vol_plugin, attr_name, acpl_id, aapl_id, 
                                        H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create attribute")

    /* Get an atom for the attribute */
    if((ret_value = H5I_register2(H5I_ATTR, attr, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    if (ret_value < 0 && attr)
        if(H5VL_attr_close (attr, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release dataset")
    FUNC_LEAVE_API(ret_value)
} /* H5Acreate_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Acreate_by_name_ff
 PURPOSE
    Creates an attribute on an object
 RETURNS
    Non-negative on success/Negative on failure
--------------------------------------------------------------------------*/
hid_t
H5Acreate_by_name_ff(hid_t loc_id, const char *obj_name, const char *attr_name,
    hid_t type_id, hid_t space_id, hid_t acpl_id, hid_t aapl_id,
    hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *attr = NULL;       /* attr token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5P_genplist_t      *plist;            /* Property list pointer */
    H5VL_loc_params_t    loc_params;
    hid_t		 ret_value;        /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!obj_name || !*obj_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no object name")
    if(!attr_name || !*attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no attribute name")

    /* Get correct property list */
    if(H5P_DEFAULT == acpl_id)
        acpl_id = H5P_ATTRIBUTE_CREATE_DEFAULT;

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(acpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID")

    /* set creation properties */
    if(H5P_set(plist, H5VL_ATTR_TYPE_ID, &type_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for datatype id")
    if(H5P_set(plist, H5VL_ATTR_SPACE_ID, &space_id) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for space id")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.obj_type = H5I_get_type(loc_id);
    loc_params.loc_data.loc_by_name.name = obj_name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the attribute through the VOL */
    if(NULL == (attr = H5VL_attr_create(obj, loc_params, vol_plugin, attr_name, acpl_id, 
                                        aapl_id, H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create attribute")

    /* Get an atom for the attribute */
    if((ret_value = H5I_register2(H5I_ATTR, attr, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    if (ret_value < 0 && attr)
        if(H5VL_attr_close (attr, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release dataset")
    FUNC_LEAVE_API(ret_value)
} /* H5Acreate_by_name_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Aopen_ff
 PURPOSE
    Opens an attribute for an object by looking up the attribute name
 RETURNS
    ID of attribute on success, negative on failure
--------------------------------------------------------------------------*/
hid_t
H5Aopen_ff(hid_t loc_id, const char *attr_name, hid_t aapl_id, 
           uint64_t trans, hid_t eq_id)
{
    void    *attr = NULL;       /* attr token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params; 
    hid_t		ret_value;

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!attr_name || !*attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no attribute name")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the attribute through the VOL */
    if(NULL == (attr = H5VL_attr_open(obj, loc_params, vol_plugin, attr_name, aapl_id, H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to open attribute")

    /* Get an atom for the attribute */
    if((ret_value = H5I_register2(H5I_ATTR, attr, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    if (ret_value < 0 && attr)
        if(H5VL_attr_close (attr, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release dataset")
    FUNC_LEAVE_API(ret_value)
} /* H5Aopen_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Aopen_by_name_ff
 PURPOSE
    Opens an attribute for an object by looking up the attribute name
 RETURNS
    ID of attribute on success, negative on failure
--------------------------------------------------------------------------*/
hid_t
H5Aopen_by_name_ff(hid_t loc_id, const char *obj_name, const char *attr_name,
    hid_t aapl_id, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *attr = NULL;       /* attr token from VOL plugin */
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t   loc_params;
    hid_t		ret_value;

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!obj_name || !*obj_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no object name")
    if(!attr_name || !*attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no attribute name")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = obj_name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Create the attribute through the VOL */
    if(NULL == (attr = H5VL_attr_open(obj, loc_params, vol_plugin, attr_name, aapl_id, H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to open attribute")

    /* Get an atom for the attribute */
    if((ret_value = H5I_register2(H5I_ATTR, attr, vol_plugin, TRUE)) < 0)
	HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    if (ret_value < 0 && attr)
        if(H5VL_attr_close (attr, vol_plugin, H5AC_dxpl_id, eq_id) < 0)
            HDONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "unable to release dataset")
    FUNC_LEAVE_API(ret_value)
} /* H5Aopen_by_name_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Awrite_ff
 PURPOSE
    Write out data to an attribute
 RETURNS
    Non-negative on success/Negative on failure
--------------------------------------------------------------------------*/
herr_t
H5Awrite_ff(hid_t attr_id, hid_t dtype_id, const void *buf, uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *attr;
    herr_t ret_value;           /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(NULL == buf)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "null attribute buffer")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(attr_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (attr = (void *)H5I_object(attr_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid attribute identifier")

    /* write the data through the VOL */
    if((ret_value = H5VL_attr_write(attr, vol_plugin, dtype_id, buf, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_ATTR, H5E_READERROR, FAIL, "can't read data")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Awrite_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Aread_ff
 PURPOSE
    Read in data from an attribute
 RETURNS
    Non-negative on success/Negative on failure
--------------------------------------------------------------------------*/
herr_t
H5Aread_ff(hid_t attr_id, hid_t dtype_id, void *buf, uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *attr;
    herr_t ret_value;           /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(NULL == buf)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "null attribute buffer")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(attr_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (attr = (void *)H5I_object(attr_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid attribute identifier")

    /* Read the data through the VOL */
    if((ret_value = H5VL_attr_read(attr, vol_plugin, dtype_id, buf, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_ATTR, H5E_READERROR, FAIL, "can't read data")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Aread_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Arename_ff
 *
 * Purpose:     Rename an attribute
 *
 * Return:	Success:             Non-negative
 *		Failure:             Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Arename_ff(hid_t loc_id, const char *old_name, const char *new_name, 
             uint64_t trans, hid_t eq_id)
{
    herr_t	ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(!old_name || !new_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "name is nil")
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")

    /* Avoid thrashing things if the names are the same */
    if(HDstrcmp(old_name, new_name)) {
        H5VL_t     *vol_plugin;
        void       *obj;
        H5VL_loc_params_t loc_params;

        loc_params.type = H5VL_OBJECT_BY_SELF;
        loc_params.obj_type = H5I_get_type(loc_id);

        /* get the file object */
        if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

        /* rename the attribute info through the VOL */
        if(H5VL_object_misc(obj, loc_params, vol_plugin, H5VL_ATTR_RENAME, H5AC_dxpl_id, 
                            eq_id, old_name, new_name) < 0)
            HGOTO_ERROR(H5E_ATTR, H5E_CANTRENAME, FAIL, "can't rename attribute")
    }
done:
    FUNC_LEAVE_API(ret_value)
} /* H5Arename_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Arename_by_name_ff
 *
 * Purpose:     Rename an attribute
 *
 * Return:	Success:             Non-negative
 *		Failure:             Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Arename_by_name_ff(hid_t loc_id, const char *obj_name, const char *old_attr_name,
                     const char *new_attr_name, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    herr_t	ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!obj_name || !*obj_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no object name")
    if(!old_attr_name || !*old_attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no old attribute name")
    if(!new_attr_name || !*new_attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no new attribute name")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    /* Avoid thrashing things if the names are the same */
    if(HDstrcmp(old_attr_name, new_attr_name)) {
        H5VL_t     *vol_plugin;
        void       *obj;
        H5VL_loc_params_t loc_params;

        loc_params.type = H5VL_OBJECT_BY_NAME;
        loc_params.loc_data.loc_by_name.name = obj_name;
        loc_params.loc_data.loc_by_name.plist_id = lapl_id;
        loc_params.obj_type = H5I_get_type(loc_id);

        /* get the file object */
        if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

        /* rename the attribute info through the VOL */
        if(H5VL_object_misc(obj, loc_params, vol_plugin, H5VL_ATTR_RENAME, H5AC_dxpl_id, 
                            eq_id, old_attr_name, new_attr_name) < 0)
            HGOTO_ERROR(H5E_ATTR, H5E_CANTRENAME, FAIL, "can't rename attribute")
    } /* end if */

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Arename_by_name_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Adelete_ff
 PURPOSE
    Deletes an attribute from a location
 RETURNS
    Non-negative on success/Negative on failure
--------------------------------------------------------------------------*/
herr_t
H5Adelete_ff(hid_t loc_id, const char *name, uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *obj;
    H5VL_loc_params_t loc_params;
    herr_t	ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!name || !*name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid object identifier")

    /* Open the attribute through the VOL */
    if(H5VL_attr_remove(obj, loc_params, vol_plugin, name, H5AC_dxpl_id, eq_id) < 0)
	HGOTO_ERROR(H5E_ATTR, H5E_CANTDELETE, FAIL, "unable to delete attribute")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Adelete_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Adelete_by_name_ff
 PURPOSE
    Deletes an attribute from a location
 RETURNS
    Non-negative on success/Negative on failure
 DESCRIPTION
    This function removes the named attribute from an object.
--------------------------------------------------------------------------*/
herr_t
H5Adelete_by_name_ff(hid_t loc_id, const char *obj_name, const char *attr_name,
    hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *obj;
    H5VL_loc_params_t loc_params;
    herr_t	ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!obj_name || !*obj_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no object name")
    if(!attr_name || !*attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no attribute name")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = obj_name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid object identifier")

    /* Open the attribute through the VOL */
    if(H5VL_attr_remove(obj, loc_params, vol_plugin, attr_name, H5AC_dxpl_id, eq_id) < 0)
	HGOTO_ERROR(H5E_ATTR, H5E_CANTDELETE, FAIL, "unable to delete attribute")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Adelete_by_name_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Aexists_ff
 *
 * Purpose:	Checks if an attribute with a given name exists on an opened
 *              object.
 *
 * Return:	Success:	Positive
 * 		Failure:	Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Aexists_ff(hid_t obj_id, const char *attr_name, htri_t *ret, uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *obj;
    H5VL_loc_params_t loc_params;
    herr_t	ret_value = SUCCEED;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(obj_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!attr_name || !*attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no attribute name")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(obj_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (obj = (void *)H5VL_get_object(obj_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid object identifier")

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(obj_id);

    /* get the attribute info through the VOL */
    if(H5VL_attr_get(obj, vol_plugin, H5VL_ATTR_EXISTS, H5AC_dxpl_id, eq_id, 
                     loc_params, attr_name, ret) < 0)
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "unable to get attribute info")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Aexists_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Aexists_by_name_ff
 *
 * Purpose:	Checks if an attribute with a given name exists on an object.
 *
 * Return:	Success:	Positive
 * 		Failure:	Negative
 *
 * Programmer:	Quincey Koziol
 *              Thursday, November 1, 2007
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Aexists_by_name_ff(hid_t loc_id, const char *obj_name, const char *attr_name,
                     hid_t lapl_id, htri_t *ret, uint64_t trans, hid_t eq_id)
{
    H5VL_t     *vol_plugin;
    void       *obj;
    H5VL_loc_params_t loc_params;
    herr_t	ret_value = SUCCEED;   /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(H5I_ATTR == H5I_get_type(loc_id))
	HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "location is not valid for an attribute")
    if(!obj_name || !*obj_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no object name")
    if(!attr_name || !*attr_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no attribute name")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    /* get the dataset object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid object identifier")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = obj_name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the attribute info through the VOL */
    if(H5VL_attr_get(obj, vol_plugin, H5VL_ATTR_EXISTS, H5AC_dxpl_id, eq_id, 
                     loc_params, attr_name, ret) < 0)
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "unable to get attribute info")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Aexists_by_name_ff() */


/*--------------------------------------------------------------------------
 NAME
    H5Aclose_ff
 PURPOSE
    Close an attribute ID
 RETURNS
    Non-negative on success/Negative on failure

 DESCRIPTION
        This function releases an attribute from use.  Further use of the
    attribute ID will result in undefined behavior.
--------------------------------------------------------------------------*/
herr_t
H5Aclose_ff(hid_t attr_id, hid_t eq_id)
{
    H5VL_t *vol_plugin = NULL;
    herr_t ret_value = SUCCEED;   /* Return value */

    FUNC_ENTER_API(FAIL)

    /* check arguments */
    if(NULL == H5I_object_verify(attr_id, H5I_ATTR))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not an attribute")

    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(attr_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information");
    /* set the event queue and dxpl IDs to be passed on to the VOL layer */
    vol_plugin->close_eq_id = eq_id;
    vol_plugin->close_dxpl_id = H5AC_dxpl_id;

    /* Decrement references to that atom (and close it) */
    if(H5I_dec_app_ref(attr_id) < 0)
        HGOTO_ERROR(H5E_ATTR, H5E_CANTDEC, FAIL, "can't close attribute")

done:
    FUNC_LEAVE_API(ret_value)
} /* H5Aclose_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Lmove_ff
 *
 * Purpose:	Renames an object within an HDF5 file and moves it to a new
 *              group.  The original name SRC is unlinked from the group graph
 *              and then inserted with the new name DST (which can specify a
 *              new path for the object) as an atomic operation. The names
 *              are interpreted relative to SRC_LOC_ID and
 *              DST_LOC_ID, which are either file IDs or group ID.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Lmove_ff(hid_t src_loc_id, const char *src_name, hid_t dst_loc_id, 
           const char *dst_name, hid_t lcpl_id, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj1 = NULL;        /* object token of src_id */
    H5VL_t  *vol_plugin1;        /* VOL plugin information */
    H5VL_loc_params_t loc_params1;
    void    *obj2 = NULL;        /* object token of dst_id */
    H5VL_t  *vol_plugin2;        /* VOL plugin information */
    H5VL_loc_params_t loc_params2;
    herr_t      ret_value=SUCCEED;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(src_loc_id == H5L_SAME_LOC && dst_loc_id == H5L_SAME_LOC)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "source and destination should not both be H5L_SAME_LOC")
    if(!src_name || !*src_name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no current name specified")
    if(!dst_name || !*dst_name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no destination name specified")
    if(lcpl_id != H5P_DEFAULT && (TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a link creation property list")

    /* Check the link create property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    /* Check the link access property list */
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;

    /* set location paramter for source object */
    loc_params1.type = H5VL_OBJECT_BY_NAME;
    loc_params1.loc_data.loc_by_name.name = src_name;
    loc_params1.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params1.obj_type = H5I_get_type(src_loc_id);
    /* set location paramter for destination object */
    loc_params2.type = H5VL_OBJECT_BY_NAME;
    loc_params2.loc_data.loc_by_name.name = dst_name;
    loc_params2.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params2.obj_type = H5I_get_type(dst_loc_id);

    if(H5L_SAME_LOC != src_loc_id) {
        /* get the file object */
        if(NULL == (obj1 = (void *)H5I_object(src_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin1 = (H5VL_t *)H5I_get_aux(src_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    if(H5L_SAME_LOC != dst_loc_id) {
        /* get the file object */
        if(NULL == (obj2 = (void *)H5I_object(dst_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin2 = (H5VL_t *)H5I_get_aux(dst_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    /* Make sure that the VOL plugins are the same */
    if(H5L_SAME_LOC != dst_loc_id && H5L_SAME_LOC != src_loc_id) {
        if (vol_plugin1->cls != vol_plugin2->cls)
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "Objects are accessed through different VOL plugins and can't be linked")
    }

    /* Move the link through the VOL */
    if((ret_value = H5VL_link_move(obj1, loc_params1, obj2, loc_params2, 
                                   (vol_plugin1!=NULL ? vol_plugin1 : vol_plugin2), 
                                   FALSE, lcpl_id, lapl_id, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create link")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Lmove_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Lcopy_ff
 *
 * Purpose:	Creates an identical copy of a link with the same creation
 *              time and target.  The new link can have a different name
 *              and be in a different location than the original.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Lcopy_ff(hid_t src_loc_id, const char *src_name, hid_t dst_loc_id,
           const char *dst_name, hid_t lcpl_id, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj1 = NULL;        /* object token of src_id */
    H5VL_t  *vol_plugin1;        /* VOL plugin information */
    H5VL_loc_params_t loc_params1;
    void    *obj2 = NULL;        /* object token of dst_id */
    H5VL_t  *vol_plugin2;        /* VOL plugin information */
    H5VL_loc_params_t loc_params2;
    herr_t      ret_value=SUCCEED;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(src_loc_id == H5L_SAME_LOC && dst_loc_id == H5L_SAME_LOC)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "source and destination should not both be H5L_SAME_LOC")
    if(!src_name || !*src_name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no current name specified")
    if(!dst_name || !*dst_name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no destination name specified")
    if(lcpl_id != H5P_DEFAULT && (TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a link creation property list")

    /* Check the link create property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    /* Check the link access property list */
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;

    /* set location paramter for source object */
    loc_params1.type = H5VL_OBJECT_BY_NAME;
    loc_params1.loc_data.loc_by_name.name = src_name;
    loc_params1.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params1.obj_type = H5I_get_type(src_loc_id);
    /* set location paramter for destination object */
    loc_params2.type = H5VL_OBJECT_BY_NAME;
    loc_params2.loc_data.loc_by_name.name = dst_name;
    loc_params2.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params2.obj_type = H5I_get_type(dst_loc_id);

    if(H5L_SAME_LOC != src_loc_id) {
        /* get the file object */
        if(NULL == (obj1 = (void *)H5I_object(src_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin1 = (H5VL_t *)H5I_get_aux(src_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    if(H5L_SAME_LOC != dst_loc_id) {
        /* get the file object */
        if(NULL == (obj2 = (void *)H5I_object(dst_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin2 = (H5VL_t *)H5I_get_aux(dst_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    /* Make sure that the VOL plugins are the same */
    if(H5L_SAME_LOC != dst_loc_id && H5L_SAME_LOC != src_loc_id) {
        if (vol_plugin1->cls != vol_plugin2->cls)
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "Objects are accessed through different VOL plugins and can't be linked")
    }

    /* Move the link through the VOL */
    if((ret_value = H5VL_link_move(obj1, loc_params1, obj2, loc_params2, 
                                   (vol_plugin1!=NULL ? vol_plugin1 : vol_plugin2), 
                                   TRUE, lcpl_id, lapl_id, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create link")
done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Lcopy_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Lcreate_soft_ff
 *
 * Purpose:	Creates a soft link from LINK_NAME to LINK_TARGET.
 *
 * 		LINK_TARGET can be anything and is interpreted at lookup
 *              time relative to the group which contains the final component
 *              of LINK_NAME.  For instance, if LINK_TARGET is `./foo' and
 *              LINK_NAME is `./x/y/bar' and a request is made for `./x/y/bar'
 *              then the actual object looked up is `./x/y/./foo'.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Lcreate_soft_ff(const char *link_target, hid_t link_loc_id, const char *link_name, 
                  hid_t lcpl_id, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    H5P_genplist_t *plist;      /* Property list pointer */
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(link_loc_id == H5L_SAME_LOC)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "link location id should not be H5L_SAME_LOC")
    if(!link_target || !*link_target)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no target specified")
    if(!link_name || !*link_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no new name specified")
    if(lcpl_id != H5P_DEFAULT && (TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a link creation property list")

    /* Check the group access property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    /* Check the link access property list */
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = link_name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(link_loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5I_object(link_loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(link_loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(lcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID")

    /* set creation properties */
    if(H5P_set(plist, H5VL_LINK_TARGET_NAME, &link_target) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't get property value for target name")

    /* Create the link through the VOL */
    if((ret_value = H5VL_link_create(H5VL_LINK_CREATE_SOFT, obj, loc_params, vol_plugin,
                                     lcpl_id, lapl_id, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create link")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Lcreate_soft_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Lcreate_hard_ff
 *
 * Purpose:	Creates a hard link from NEW_NAME to CUR_NAME.
 *
 *		CUR_NAME must name an existing object.  CUR_NAME and
 *              NEW_NAME are interpreted relative to CUR_LOC_ID and
 *              NEW_LOC_ID, which are either file IDs or group IDs.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Lcreate_hard_ff(hid_t cur_loc_id, const char *cur_name, hid_t new_loc_id, 
                  const char *new_name, hid_t lcpl_id, hid_t lapl_id, 
                  uint64_t trans, hid_t eq_id)
{
    void    *obj1 = NULL;        /* object token of loc_id */
    void    *obj2 = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin1 = NULL; /* VOL plugin information */
    H5VL_t  *vol_plugin2 = NULL; /* VOL plugin information */
    H5VL_loc_params_t loc_params1;
    H5VL_loc_params_t loc_params2;
    H5P_genplist_t *plist;      /* Property list pointer */
    herr_t   ret_value = SUCCEED;            /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(cur_loc_id == H5L_SAME_LOC && new_loc_id == H5L_SAME_LOC)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "source and destination should not be both H5L_SAME_LOC")
    if(!cur_name || !*cur_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no current name specified")
    if(!new_name || !*new_name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no new name specified")
    if(lcpl_id != H5P_DEFAULT && (TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a link creation property list")

    /* Check the link create property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    /* Check the link access property list */
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;

    loc_params1.type = H5VL_OBJECT_BY_NAME;
    loc_params1.obj_type = H5I_get_type(cur_loc_id);
    loc_params1.loc_data.loc_by_name.name = cur_name;
    loc_params1.loc_data.loc_by_name.plist_id = lapl_id;

    loc_params2.type = H5VL_OBJECT_BY_NAME;
    loc_params2.obj_type = H5I_get_type(new_loc_id);
    loc_params2.loc_data.loc_by_name.name = new_name;
    loc_params2.loc_data.loc_by_name.plist_id = lapl_id;

    if(H5L_SAME_LOC != cur_loc_id) {
        /* get the file object */
        if(NULL == (obj1 = (void *)H5I_object(cur_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin1 = (H5VL_t *)H5I_get_aux(cur_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    if(H5L_SAME_LOC != new_loc_id) {
        /* get the file object */
        if(NULL == (obj2 = (void *)H5I_object(new_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin2 = (H5VL_t *)H5I_get_aux(new_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    /* Make sure that the VOL plugins are the same */
    if(H5L_SAME_LOC != new_loc_id && H5L_SAME_LOC != cur_loc_id) {
        if (vol_plugin1->cls != vol_plugin2->cls)
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "Objects are accessed through different VOL plugins and can't be linked")
    }

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(lcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID")

    /* set creation properties */
    if(H5P_set(plist, H5VL_LINK_TARGET, &obj1) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for target id")
    if(H5P_set(plist, H5VL_LINK_TARGET_LOC_PARAMS, &loc_params1) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for target name")

    /* Create the link through the VOL */
    if((ret_value = H5VL_link_create(H5VL_LINK_CREATE_HARD, obj2, loc_params2, 
                                     (vol_plugin1!=NULL ? vol_plugin1 : vol_plugin2),
                                     lcpl_id, lapl_id, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create link")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Lcreate_hard_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Ldelete_ff
 *
 * Purpose:	Removes the specified NAME from the group graph and
 *		decrements the link count for the object to which NAME
 *		points.  If the link count reaches zero then all file-space
 *		associated with the object will be reclaimed (but if the
 *		object is open, then the reclamation of the file space is
 *		delayed until all handles to the object are closed).
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Ldelete_ff(hid_t loc_id, const char *name, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t ret_value = SUCCEED;         /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(!name || !*name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.obj_type = H5I_get_type(loc_id);
    loc_params.loc_data.loc_by_name.name = name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;

    /* get the file object */
    if(NULL == (obj = (void *)H5I_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Delete the link through the VOL */
    if((ret_value = H5VL_link_remove(obj, loc_params, vol_plugin, H5AC_dxpl_id, 
                                     eq_id)) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create link")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Ldelete_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Lexists_ff
 *
 * Purpose:	Checks if a link of a given name exists in a group
 *
 * Return:	Success:	Positive
 * 		Failure:	Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Lexists_ff(hid_t loc_id, const char *name, hid_t lapl_id, htri_t *ret, 
             uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(!name || !*name)
	HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name specified")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.obj_type = H5I_get_type(loc_id);
    loc_params.loc_data.loc_by_name.name = name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;

    /* get the file object */
    if(NULL == (obj = (void *)H5I_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* check link existence through the VOL */
    if(H5VL_link_get(obj, loc_params, vol_plugin, H5VL_LINK_EXISTS, 
                     H5AC_dxpl_id, eq_id, ret) < 0)
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "unable to get link info")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Lexists_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Oopen_ff
 *
 * Purpose:	Opens an object within an HDF5 file.
 *
 *              This function opens an object in the same way that H5Gopen2,
 *              H5Topen2, and H5Dopen2 do. However, H5Oopen doesn't require
 *              the type of object to be known beforehand. This can be
 *              useful in user-defined links, for instance, when only a
 *              path is known.
 *
 *              The opened object should be closed again with H5Oclose
 *              or H5Gclose, H5Tclose, or H5Dclose.
 *
 * Return:	Success:	An open object identifier
 *		Failure:	Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5Oopen_ff(hid_t loc_id, const char *name, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5I_type_t  opened_type;
    void       *opened_obj = NULL;
    H5VL_loc_params_t loc_params;
    hid_t       ret_value = FAIL;

    FUNC_ENTER_API(FAIL)

    if(!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5I_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* Open the object through the VOL */
    if(NULL == (opened_obj = H5VL_object_open(obj, loc_params, vol_plugin, &opened_type, 
                                              H5AC_dxpl_id, eq_id)))
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to open object")

    if ((ret_value = H5VL_object_register(opened_obj, opened_type, vol_plugin, TRUE)) < 0)
        HGOTO_ERROR(H5E_ATOM, H5E_CANTREGISTER, FAIL, "unable to atomize dataset handle")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Oopen_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Olink_ff
 *
 * Purpose:	Creates a hard link from NEW_NAME to the object specified
 *		by OBJ_ID using properties defined in the Link Creation
 *              Property List LCPL.
 *
 *		This function should be used to link objects that have just
 *              been created.
 *
 *		NEW_NAME is interpreted relative to
 *		NEW_LOC_ID, which is either a file ID or a
 *		group ID.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Olink_ff(hid_t obj_id, hid_t new_loc_id, const char *new_name, hid_t lcpl_id,
           hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj1 = NULL;        /* object token of loc_id */
    void    *obj2 = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin1 = NULL;  /* VOL plugin information */
    H5VL_t  *vol_plugin2 = NULL;  /* VOL plugin information */
    H5VL_loc_params_t loc_params1;
    H5VL_loc_params_t loc_params2;
    H5P_genplist_t *plist;      /* Property list pointer */
    herr_t         ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check arguments */
    if(new_loc_id == H5L_SAME_LOC)
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "cannot use H5L_SAME_LOC when only one location is specified")
    if(!new_name || !*new_name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name specified")
/* Avoid compiler warning on 32-bit machines */
#if H5_SIZEOF_SIZE_T > H5_SIZEOF_INT32_T
    if(HDstrlen(new_name) > H5L_MAX_LINK_NAME_LEN)
        HGOTO_ERROR(H5E_ARGS, H5E_BADRANGE, FAIL, "name too long")
#endif /* H5_SIZEOF_SIZE_T > H5_SIZEOF_INT32_T */
    if(lcpl_id != H5P_DEFAULT && (TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a link creation property list")

    /* Check the group access property list */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;

    loc_params1.type = H5VL_OBJECT_BY_SELF;
    loc_params1.obj_type = H5I_get_type(obj_id);

    loc_params2.type = H5VL_OBJECT_BY_NAME;
    loc_params2.obj_type = H5I_get_type(new_loc_id);
    loc_params2.loc_data.loc_by_name.name = new_name;
    loc_params2.loc_data.loc_by_name.plist_id = lapl_id;

    if(H5L_SAME_LOC != obj_id) {
        /* get the file object */
        if(NULL == (obj1 = (void *)H5VL_get_object(obj_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin1 = (H5VL_t *)H5I_get_aux(obj_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    if(H5L_SAME_LOC != new_loc_id) {
        /* get the file object */
        if(NULL == (obj2 = (void *)H5VL_get_object(new_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
        /* get the plugin pointer */
        if (NULL == (vol_plugin2 = (H5VL_t *)H5I_get_aux(new_loc_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")
    }
    /* Make sure that the VOL plugins are the same */
    if(H5L_SAME_LOC != new_loc_id && H5L_SAME_LOC != obj_id) {
        if (vol_plugin1->cls != vol_plugin2->cls)
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "Objects are accessed through different VOL plugins and can't be linked")
    }

    /* Get the plist structure */
    if(NULL == (plist = (H5P_genplist_t *)H5I_object(lcpl_id)))
        HGOTO_ERROR(H5E_ATOM, H5E_BADATOM, FAIL, "can't find object for ID")

    /* set creation properties */
    if(H5P_set(plist, H5VL_LINK_TARGET, &obj1) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for target id")
    if(H5P_set(plist, H5VL_LINK_TARGET_LOC_PARAMS, &loc_params1) < 0)
        HGOTO_ERROR(H5E_PLIST, H5E_CANTGET, FAIL, "can't set property value for target id")

    /* Create the link through the VOL */
    if((ret_value = H5VL_link_create(H5VL_LINK_CREATE_HARD, obj2, loc_params2, 
                                     (vol_plugin1!=NULL ? vol_plugin1 : vol_plugin2),
                                     lcpl_id, lapl_id, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to create link")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Olink_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Oexists_by_name_ff
 *
 * Purpose:	Determine if a linked-to object exists
 *
 * Return:	Success:	TRUE/FALSE
 * 		Failure:	Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Oexists_by_name_ff(hid_t loc_id, const char *name, htri_t *ret, hid_t lapl_id,
                     uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t  ret_value = SUCCEED;       /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
    if(!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* change the ref count through the VOL */
    if(H5VL_object_get(obj, loc_params, vol_plugin, H5VL_OBJECT_EXISTS, 
                       H5AC_dxpl_id, eq_id, ret) < 0)
        HGOTO_ERROR(H5E_OHDR, H5E_CANTGET, FAIL, "unable to determine if '%s' exists", name)

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Oexists_by_name_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Oset_comment_ff
 *
 * Purpose:     Gives the specified object a comment.  The COMMENT string
 *		should be a null terminated string.  An object can have only
 *		one comment at a time.  Passing NULL for the COMMENT argument
 *		will remove the comment property from the object.
 *
 * Note:	Deprecated in favor of using attributes on objects
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Oset_comment_ff(hid_t obj_id, const char *comment, uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(obj_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(obj_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(obj_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* set comment on object through the VOL */
    if(H5VL_object_misc(obj, loc_params, vol_plugin, H5VL_OBJECT_SET_COMMENT, 
                        H5AC_dxpl_id, eq_id, comment) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to set comment value")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Oset_comment_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Oset_comment_by_name_ff
 *
 * Purpose:     Gives the specified object a comment.  The COMMENT string
 *		should be a null terminated string.  An object can have only
 *		one comment at a time.  Passing NULL for the COMMENT argument
 *		will remove the comment property from the object.
 *
 * Note:	Deprecated in favor of using attributes on objects
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Oset_comment_by_name_ff(hid_t loc_id, const char *name, const char *comment,
                          hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t      ret_value = SUCCEED;    /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
    if(!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    /* set comment on object through the VOL */
    if(H5VL_object_misc(obj, loc_params, vol_plugin, H5VL_OBJECT_SET_COMMENT, 
                        H5AC_dxpl_id, eq_id, comment) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to set comment value")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Oset_comment_by_name_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Oget_comment_ff
 *
 * Purpose:	Retrieve comment for an object.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Oget_comment_ff(hid_t loc_id, char *comment, size_t bufsize, ssize_t *ret,
                  uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t ret_value = SUCCEED;              /* Return value */

    FUNC_ENTER_API(FAIL)

    loc_params.type = H5VL_OBJECT_BY_SELF;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    if(H5VL_object_get(obj, loc_params, vol_plugin, H5VL_OBJECT_GET_COMMENT, 
                       H5AC_dxpl_id, eq_id, comment, bufsize, ret) < 0)
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "unable to get object comment")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Oget_comment_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Oget_comment_by_name_ff
 *
 * Purpose:	Retrieve comment for an object.
 *
 * Return:	Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Oget_comment_by_name_ff(hid_t loc_id, const char *name, char *comment, size_t bufsize,
                          ssize_t *ret, hid_t lapl_id, uint64_t trans, hid_t eq_id)
{
    void    *obj = NULL;        /* object token of loc_id */
    H5VL_t  *vol_plugin;        /* VOL plugin information */
    H5VL_loc_params_t loc_params;
    herr_t ret_value = SUCCEED;              /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Check args */
    if(!name || !*name)
        HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "no name")
    if(H5P_DEFAULT == lapl_id)
        lapl_id = H5P_LINK_ACCESS_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lapl_id, H5P_LINK_ACCESS))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link access property list ID")

    loc_params.type = H5VL_OBJECT_BY_NAME;
    loc_params.loc_data.loc_by_name.name = name;
    loc_params.loc_data.loc_by_name.plist_id = lapl_id;
    loc_params.obj_type = H5I_get_type(loc_id);

    /* get the file object */
    if(NULL == (obj = (void *)H5VL_get_object(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid file identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information")

    if(H5VL_object_get(obj, loc_params, vol_plugin, H5VL_OBJECT_GET_COMMENT, 
                       H5AC_dxpl_id, eq_id, comment, bufsize, ret) < 0)
        HGOTO_ERROR(H5E_INTERNAL, H5E_CANTGET, FAIL, "unable to get object info")

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Oget_comment_by_name_ff() */


/*-------------------------------------------------------------------------
 * Function:    H5Ocopy_ff
 *
 * Purpose:     Copy an object (group or dataset) to destination location
 *              within a file or cross files. PLIST_ID is a property list
 *              which is used to pass user options and properties to the
 *              copy. The name, dst_name, must not already be taken by some
 *              other object in the destination group.
 *
 * Return:      Non-negative on success/Negative on failure
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Ocopy_ff(hid_t src_loc_id, const char *src_name, hid_t dst_loc_id,
           const char *dst_name, hid_t ocpypl_id, hid_t lcpl_id, 
           uint64_t trans, hid_t eq_id)
{
    void    *obj1 = NULL;        /* object token of src_id */
    H5VL_t  *vol_plugin1;        /* VOL plugin information */
    H5VL_loc_params_t loc_params1;
    void    *obj2 = NULL;        /* object token of dst_id */
    H5VL_t  *vol_plugin2;        /* VOL plugin information */
    H5VL_loc_params_t loc_params2;
    herr_t      ret_value = SUCCEED;        /* Return value */

    FUNC_ENTER_API(FAIL)

    /* Get correct property lists */
    if(H5P_DEFAULT == lcpl_id)
        lcpl_id = H5P_LINK_CREATE_DEFAULT;
    else
        if(TRUE != H5P_isa_class(lcpl_id, H5P_LINK_CREATE))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not link creation property list")

    /* Get object copy property list */
    if(H5P_DEFAULT == ocpypl_id)
        ocpypl_id = H5P_OBJECT_COPY_DEFAULT;
    else
        if(TRUE != H5P_isa_class(ocpypl_id, H5P_OBJECT_COPY))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not object copy property list")

    /* get the object */
    if(NULL == (obj1 = (void *)H5I_object(src_loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid location identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin1 = (H5VL_t *)H5I_get_aux(src_loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "src ID does not contain VOL information")
    loc_params1.type = H5VL_OBJECT_BY_SELF;
    loc_params1.obj_type = H5I_get_type(src_loc_id);

    /* get the object */
    if(NULL == (obj2 = (void *)H5I_object(dst_loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "invalid location identifier")
    /* get the plugin pointer */
    if (NULL == (vol_plugin2 = (H5VL_t *)H5I_get_aux(dst_loc_id)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "dst ID does not contain VOL information")
    loc_params2.type = H5VL_OBJECT_BY_SELF;
    loc_params2.obj_type = H5I_get_type(dst_loc_id);

    /* Open the object through the VOL */
    if((ret_value = H5VL_object_copy(obj1, loc_params1, vol_plugin1, src_name, 
                                     obj2, loc_params2, vol_plugin2, dst_name, 
                                     ocpypl_id, lcpl_id, H5AC_dxpl_id, eq_id)) < 0)
	HGOTO_ERROR(H5E_SYM, H5E_CANTINIT, FAIL, "unable to open object")
done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Ocopy_ff() */


/*-------------------------------------------------------------------------
 * Function:	H5Oclose_ff
 *
 * Purpose:	Close an open file object.
 *
 *              This is the companion to H5Oopen. It is used to close any
 *              open object in an HDF5 file (but not IDs are that not file
 *              objects, such as property lists and dataspaces). It has
 *              the same effect as calling H5Gclose, H5Dclose, or H5Tclose.
 *
 * Return:	Success:	Non-negative
 *		Failure:	Negative
 *
 * Programmer:	Mohamad Chaarawi
 *              May 2013
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Oclose_ff(hid_t object_id, hid_t eq_id)
{
    H5VL_t      *vol_plugin = NULL;
    herr_t       ret_value = SUCCEED;

    FUNC_ENTER_API(FAIL)

    /* Get the type of the object and close it in the correct way */
    switch(H5I_get_type(object_id)) {
        case H5I_GROUP:
        case H5I_DATATYPE:
        case H5I_DATASET:
            /* check ID */
            if(H5I_object(object_id) == NULL)
                HGOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "not a valid object");
            /* get the plugin pointer */
            if (NULL == (vol_plugin = (H5VL_t *)H5I_get_aux(object_id)))
                HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "ID does not contain VOL information");
            /* set the event queue and dxpl IDs to be passed on to the VOL layer */
            vol_plugin->close_eq_id = eq_id;
            vol_plugin->close_dxpl_id = H5AC_dxpl_id;

            if(H5I_dec_app_ref(object_id) < 0)
                HGOTO_ERROR(H5E_OHDR, H5E_CANTRELEASE, FAIL, "unable to close object");
            break;
        case H5I_UNINIT:
        case H5I_BADID:
        case H5I_FILE:
        case H5I_DATASPACE:
        case H5I_ATTR:
        case H5I_REFERENCE:
        case H5I_VFL:
        case H5I_VOL:
        case H5I_EQ:
        case H5I_GENPROP_CLS:
        case H5I_GENPROP_LST:
        case H5I_ERROR_CLASS:
        case H5I_ERROR_MSG:
        case H5I_ERROR_STACK:
        case H5I_NTYPES:
        default:
            HGOTO_ERROR(H5E_ARGS, H5E_CANTRELEASE, FAIL, "not a valid file object ID (dataset, group, or datatype)")
        break;
    } /* end switch */

done:
    FUNC_LEAVE_API(ret_value)
} /* end H5Oclose_ff() */

#endif /* H5_HAVE_EFF */