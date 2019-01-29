/*******************************************************************************
  Copyright(c) 2009 Geoffrey Hausheer. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
  
  
  Contact Information: gcx@phracturedblue.com <Geoffrey Hausheer>
*******************************************************************************/

#include "../indi_list.h"
#include <glib.h>

indi_list *il_iter(indi_list *l)
{
	return l;
}

indi_list *il_next(indi_list *l)
{
	GSList *gsl = (GSList *)l;
	return g_slist_next(gsl);
}

int il_is_last(indi_list *l)
{
	return (l == NULL);
}

indi_list *il_new()
{
	return NULL;
}

indi_list *il_prepend(indi_list *l, void *data)
{
	return g_slist_prepend((GSList *)l, data);
}

indi_list *il_append(indi_list *l, void *data)
{
	return g_slist_append((GSList *)l, data);
}


indi_list *il_remove(indi_list *l, void *data)
{
	return g_slist_remove((GSList *)l, data);
}

indi_list *il_remove_first(indi_list *l)
{
	return g_slist_remove((GSList *)l, ((GSList *)l)->data);
}

void *il_item(indi_list *l)
{
	GSList *gsl = (GSList *)l;
	if(gsl)
		return gsl->data;
	return NULL;
}

void *il_first(void *l)
{
	if (l)
		return ((GSList *)l)->data;
	return NULL;
}

unsigned int il_length(indi_list *l)
{
 	return g_slist_length((GSList *)l);
}

