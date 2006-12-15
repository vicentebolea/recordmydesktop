/*********************************************************************************
*                             recordMyDesktop                                    *
**********************************************************************************
*                                                                                *
*             Copyright (C) 2006  John Varouhakis                                *
*                                                                                *
*                                                                                *
*    This program is free software; you can redistribute it and/or modify        *
*    it under the terms of the GNU General Public License as published by        *
*    the Free Software Foundation; either version 2 of the License, or           *
*    (at your option) any later version.                                         *
*                                                                                *
*    This program is distributed in the hope that it will be useful,             *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of              *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
*    GNU General Public License for more details.                                *
*                                                                                *
*    You should have received a copy of the GNU General Public License           *
*    along with this program; if not, write to the Free Software                 *
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   *
*                                                                                *
*                                                                                *
*                                                                                *
*    For further information contact me at johnvarouhakis@gmail.com              *
**********************************************************************************/


#include <recordmydesktop.h>

void *PollDamage(ProgData *pdata){
    Window root_return,
           parent_return,
           *children;
    unsigned int i,
                 nchildren,
                 inserts=0;
    XEvent event;

    XSelectInput (pdata->dpy,pdata->specs.root, SubstructureNotifyMask);

    XQueryTree (pdata->dpy,
                pdata->specs.root,
                &root_return,
                &parent_return,
                &children,
                &nchildren);

    for (i = 0; i < nchildren; i++){
        XWindowAttributes attribs;
        if (XGetWindowAttributes (pdata->dpy,children[i],&attribs)){
            if (!attribs.override_redirect && attribs.depth==pdata->specs.depth)
                XDamageCreate (pdata->dpy, children[i],XDamageReportRawRectangles);
        }
    }

    XDamageCreate( pdata->dpy, pdata->brwin.windowid, XDamageReportRawRectangles);


    while(pdata->running){
        //damage polling doesn't stop,eventually full image may be needed
        //30/10/2006 : when and why did I write the above line? what did I mean?
        XNextEvent(pdata->dpy,&event);
        if (event.type == MapNotify ){
            XWindowAttributes attribs;
            if (!((XMapEvent *)(&event))->override_redirect && XGetWindowAttributes (pdata->dpy,
                                        event.xcreatewindow.window,
                                        &attribs)){
                if (!attribs.override_redirect && attribs.depth==pdata->specs.depth)
                    XDamageCreate (pdata->dpy,event.xcreatewindow.window,XDamageReportRawRectangles);
            }
        }
        else if(event.type == pdata->damage_event + XDamageNotify ){
            XDamageNotifyEvent *e =(XDamageNotifyEvent *)( &event );
            WGeometry wgeom;
            CLIP_EVENT_AREA(e,&(pdata->brwin),&wgeom);
            if((wgeom.x>=0)&&(wgeom.y>=0)&&(wgeom.width>0)&&(wgeom.height>0))
            {
                int tlist_sel=pdata->list_selector;
                pthread_mutex_lock(&pdata->list_mutex[tlist_sel]);
                inserts+=RectInsert(&pdata->rect_root[tlist_sel],&wgeom);
                pthread_mutex_unlock(&pdata->list_mutex[tlist_sel]);
            }
        }

    }
    pthread_exit(&errno);
}
