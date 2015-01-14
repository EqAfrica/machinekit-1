/********************************************************************
* Description: interpl.cc
*   Mechanism for queueing NML messages, used by the interpreter and
*   canonical interface to report list of NML statements from program
*   files to HME.
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/


#include <string.h>		/* memcpy() */
#include <assert.h>

#include "rcs.hh"		// LinkedList
#include "interpl.hh"		// these decls
#include "emc.hh"
#include "emcglb.h"
#include "linklist.hh"
#include "nmlmsg.hh"            /* class NMLmsg */
#include "rcs_print.hh"

NML_INTERP_LIST interp_list;	/* NML Union, for interpreter */

NML_INTERP_LIST::NML_INTERP_LIST()
{
    linked_list_ptr = new LinkedList;

    next_line_number = 0;
    next_call_level = -1;
    next_remap_level = -1;
    line_number = 0;
    call_level = -1;
    remap_level = -1;
}

NML_INTERP_LIST::~NML_INTERP_LIST()
{
    if (NULL != linked_list_ptr) {
	delete linked_list_ptr;
	linked_list_ptr = NULL;
    }
}

int NML_INTERP_LIST::append(NMLmsg & nml_msg)
{
    return append(&nml_msg);
}

// sets the line number used for subsequent appends
int NML_INTERP_LIST::set_line_number(int line)
{
    next_line_number = line;
    return 0;
}

// sets the lineno, call_level, and remap_level used for subsequent appends
int NML_INTERP_LIST::set_interp_params(int line, int c_level, int r_level)
{
    next_line_number = line;
    next_call_level = c_level;
    next_remap_level = r_level;
    return 0;
}

int NML_INTERP_LIST::append(NMLmsg * nml_msg_ptr)
{
    /* check for invalid data */
    if (NULL == nml_msg_ptr) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : attempt to append NULL msg\n");
	return -1;
    }

    if (0 == nml_msg_ptr->type) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : attempt to append 0 type\n");
	return -1;
    }

    if (nml_msg_ptr->size > MAX_NML_COMMAND_SIZE - 64) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : command size is too large.");
	return -1;
    }
    if (nml_msg_ptr->size < 4) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : command size is invalid.");
	return -1;
    }
#ifdef DEBUG_INTERPL
    if (sizeof(temp_node) < MAX_NML_COMMAND_SIZE + 4 ||
	sizeof(temp_node) > MAX_NML_COMMAND_SIZE + 16 ||
	((void *) &temp_node.line_number) >
	((void *) &temp_node.command.commandbuf)) {
	rcs_print_error
	    ("NML_INTERP_LIST::append : assumptions about NML_INTERP_LIST_NODE have been violated.");
	return -1;
    }
#endif

    if (NULL == linked_list_ptr) {
	return -1;
    }
    // fill in the NML_INTERP_LIST_NODE
    temp_node.line_number = next_line_number;
    temp_node.call_level = next_call_level;
    temp_node.remap_level = next_remap_level;
    memcpy(temp_node.command.commandbuf, nml_msg_ptr, nml_msg_ptr->size);

    // stick it on the list
    linked_list_ptr->store_at_tail(&temp_node,
				   nml_msg_ptr->size +
				   sizeof(temp_node.line_number) +
                                   sizeof(temp_node.call_level) +
                                   sizeof(temp_node.remap_level) +
				   sizeof(temp_node.dummy) + 32 + (32 -
								   nml_msg_ptr->
								   size %
								   32), 1);

    if (emc_debug & EMC_DEBUG_INTERP_LIST) {
	rcs_print
	    ("%s:%d %s() linked_list_ptr(%p) nml_msg_ptr{size=%ld,type=%s} : list_size=%d, line_number=%d call_level(%d) remap_level(%d)\n",
	     __FILE__, __LINE__, __FUNCTION__,
	     linked_list_ptr, nml_msg_ptr->size, emc_symbol_lookup(nml_msg_ptr->type),
	     linked_list_ptr->list_size, temp_node.line_number, temp_node.call_level, temp_node.remap_level);
    }

    return 0;
}

/* get() does copy and delete the head of the list */
NMLmsg *NML_INTERP_LIST::get()
{
    NMLmsg *ret;
    NML_INTERP_LIST_NODE *node_ptr;

    if (NULL == linked_list_ptr) {
	line_number = 0;
	call_level = -1;
	remap_level = -1;
	return NULL;
    }

    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->retrieve_head();

    if (NULL == node_ptr) {
	line_number = 0;
	call_level = -1;
	remap_level = -1;
	return NULL;
    }
    // save line number of this one, for use by get_line_number
    line_number = node_ptr->line_number;
    call_level = node_ptr->call_level;
    remap_level = node_ptr->remap_level;

    // get it off the front
    ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);

    return ret;
}

/**
 * update NMLcmd, line_number, call_level from current_node,
 */
NMLmsg *NML_INTERP_LIST::update_current()
{
    NMLmsg *ret;
    NML_INTERP_LIST_NODE *node_ptr;

    if (NULL == linked_list_ptr) {
        line_number = 0;
        call_level = -1;
        remap_level = -1;
        return NULL;
    }

    /**
     * get NML current_node of linked_list of current node
     */
    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_current();

    if (NULL == node_ptr) {
        line_number = 0;
        call_level = -1;
        remap_level = -1;
        return NULL;
    }

    /**
     * save line number of current node, for use by get_line_number()
     */
    line_number = node_ptr->line_number;

    /**
     * save call_level of current node, for use by get_call_level()
     */
    call_level = node_ptr->call_level;

    /**
     * save remap_level of current node, for use by get_remap_level()
     */
    remap_level = node_ptr->remap_level;

    /**
     * copy NML message
     */
    ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);

    return ret;
}

/**
 * move current_node to next node
 *
 * return(-1): no next node
 * return(0): successfully move to next node
 */
int NML_INTERP_LIST::move_next()
{
    NML_INTERP_LIST_NODE *node_ptr;

    if (NULL == linked_list_ptr) {
        return -1;
    }

    // move current_node of linked_list to next node
    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_next();

    if (NULL == node_ptr) {
        return -1;
    } else {
        return 0;
    }
}

/**
 * move current_node to last node
 *
 * return(-1): no last node
 * return(0): successfully move to last node
 */
int NML_INTERP_LIST::move_last()
{
    NML_INTERP_LIST_NODE *node_ptr;

    if (NULL == linked_list_ptr) {
        return -1;
    }

    // move current_node of linked_list to next node
    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_last();

    if (NULL == node_ptr) {
        return -1;
    } else {
        return 0;
    }
}

/**
 * get_by_lineno - get 1st NML_cmd which matches the given line_number
 * @lineno: the line number of selected g-code
 *          set to 0 to get the head of interp_list
 **/
NMLmsg *NML_INTERP_LIST::get_by_lineno(int lineno)
{
    NMLmsg *ret;
    NML_INTERP_LIST_NODE *node_ptr;

    if (NULL == linked_list_ptr) {
        line_number = 0;
        return NULL;
    }

    // move current_node of linked_list to head
    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_head();
    if (lineno > 0) {
        // search for node with specified line number
        while (NULL != node_ptr) {
            // save line number of this one, for use by get_line_number
            line_number = node_ptr->line_number;
            if (line_number == lineno) {
                break; // got the node; break while-loop
            } else {
                node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_next();
            }
        }
    }
    // else if (lineno == 0) : get_head()
    assert (lineno >= 0);

    if (NULL == node_ptr) {
        line_number = 0;
        return NULL;
    }

    /**
     * save line number of current node, for use by get_line_number()
     */
    line_number = node_ptr->line_number;

    /**
     * save call_level of current node, for use by get_call_level()
     */
    call_level = node_ptr->call_level;

    /**
     * save remap_level of current node, for use by get_remap_level()
     */
    remap_level = node_ptr->remap_level;

    /**
     * copy NML message
     */
    ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);

    return ret;
}

/**
 * get_next_lineno - get 1st NML_cmd with greater line_number
 * @lineno: the line number of selected g-code
 **/
NMLmsg *NML_INTERP_LIST::get_next_lineno (int lineno)
{
    NMLmsg *ret;
    NML_INTERP_LIST_NODE *node_ptr;

    if (NULL == linked_list_ptr) {
        line_number = 0;
        return NULL;
    }

    // move current_node of linked_list to head
    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_head();
    // search for node with specified line number
    while (NULL != node_ptr) {
        // save line number of this one, for use by get_line_number
        line_number = node_ptr->line_number;
        if (line_number > lineno) {
            break; // got the node; break while-loop
        } else {
            node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_next();
        }
    }
    assert (lineno >= 0);

    if (NULL == node_ptr) {
        line_number = 0;
        return NULL;
    }

    // copy NML message
    ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);

    return ret;
}

bool NML_INTERP_LIST::is_eol()
{
    if (-1 == linked_list_ptr->get_current_id()) {
        return (true);
    } else {
        return (false);
    }
}

void NML_INTERP_LIST::clear()
{
    if (NULL != linked_list_ptr) {
	linked_list_ptr->delete_members();
    }
}

void NML_INTERP_LIST::print()
{
    NMLmsg *ret;
    NML_INTERP_LIST_NODE *node_ptr;
    int line_number;

    if (NULL == linked_list_ptr) {
	return;
    }
    node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_head();

    rcs_print("NML_INTERP_LIST::print(): list size=%d\n",linked_list_ptr->list_size);
    while (NULL != node_ptr) {
	line_number = node_ptr->line_number;
	ret = (NMLmsg *) ((char *) node_ptr->command.commandbuf);
	rcs_print("--> type=%s,  line_number=%d\n",
		  emc_symbol_lookup((int)ret->type),
		  line_number);
	node_ptr = (NML_INTERP_LIST_NODE *) linked_list_ptr->get_next();
    }
    rcs_print("\n");
}

int NML_INTERP_LIST::len()
{
    if (NULL == linked_list_ptr) {
	return 0;
    }

    return ((int) linked_list_ptr->list_size);
}

int NML_INTERP_LIST::get_line_number()
{
    return line_number;
}

int NML_INTERP_LIST::get_call_level()
{
    return call_level;
}

int NML_INTERP_LIST::get_remap_level()
{
    return remap_level;
}
